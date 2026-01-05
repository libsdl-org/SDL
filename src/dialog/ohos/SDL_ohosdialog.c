/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#include "../SDL_dialog.h"
#include "../../core/ohos/SDL_ohos.h"

#define MAX_FILENUM 256
static SDL_DialogFileCallback current = NULL;
static void* userdatad = NULL;
static const char* paths[MAX_FILENUM];
static int idxCurrent = 0;

void SDL_OHOS_FileSelected(const char* data)
{
    if (idxCurrent >= MAX_FILENUM) {
        return;
    }
    paths[idxCurrent] = data;
    idxCurrent++;
}

void SDL_OHOS_ClearSelection()
{
    for (int i = 0; i < MAX_FILENUM; i++) {
        paths[i] = NULL;
    }
    idxCurrent = 0;
}

void SDL_OHOS_ExecCallback()
{
    if (current) {
        current(userdatad, (const char * const *)paths, 0);
    }
}

void SDL_SYS_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props)
{
  current = callback;
  userdatad = userdata;
  const char* defpath = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, "");
  bool allowmany = SDL_GetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
    
  void* rawfilters = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, NULL);
  int filterscount = SDL_GetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 0);
    
  char* filterstring = (char*)SDL_malloc(1024);
    
  if (rawfilters && filterscount > 0) {
    SDL_DialogFileFilter *data = (SDL_DialogFileFilter*)rawfilters;
    char *begin = filterstring;
    for (int idx = 0; idx < filterscount; idx++)
    {
      SDL_DialogFileFilter filter = data[idx];
      
      if (begin - filterstring + SDL_strlen(filter.name) >= 1024) break;
      SDL_memcpy(begin, filter.name, SDL_strlen(filter.name));
      begin += SDL_strlen(filter.name);
      if (begin - filterstring + 1 >= 1024) break;
      *begin = '|';
      begin++;
      if (begin - filterstring + SDL_strlen(filter.pattern) >= 1024) break;
      SDL_memcpy(begin, filter.pattern, SDL_strlen(filter.pattern));
      begin += SDL_strlen(filter.pattern);
      
      if (begin - filterstring + 1 >= 1024) break;
      *begin = (char)0x2;
      begin++;
    }
  }
    
  OHOS_FileDialog(type, defpath, allowmany ? MAX_FILENUM : 1, filterstring);
}
