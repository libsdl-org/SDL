/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License,Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../../SDL_internal.h"
#include "../src/core/ohos/SDL_ohos.h"

#ifdef SDL_FILESYSTEM_OHOS

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include <unistd.h>

#include "SDL_error.h"
#include "SDL_filesystem.h"
#include "SDL_system.h"


char *
SDL_GetBasePath(void)
{
    /* The current working directory is / on OHOS */
    SDL_Unsupported();
    return NULL;
}

char *
SDL_GetPrefPath(const char *org, const char *app)
{
    const char *path = SDL_OHOSGetInternalStoragePath();
    if (path) {
        size_t pathlen = SDL_strlen(path)+2;
        char *fullpath = (char *)SDL_malloc(pathlen);
        if (!fullpath) {
            SDL_OutOfMemory();
            return NULL;
        }
        SDL_snprintf(fullpath, pathlen, "%s/", path);
        return fullpath;
    }
    return NULL;
}

#endif /* SDL_FILESYSTEM_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
