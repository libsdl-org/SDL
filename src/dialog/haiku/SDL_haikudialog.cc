/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
extern "C" {
#include "../SDL_dialog_utils.h"
}
#include "../../core/haiku/SDL_BeApp.h"

#include <string>
#include <vector>

#include <sys/stat.h>

#include <FilePanel.h>
#include <Entry.h>
#include <Looper.h>
#include <Messenger.h>
#include <Path.h>
#include <TypeConstants.h>

bool StringEndsWith(const std::string& str, const std::string& end)
{
    return str.size() >= end.size() && !str.compare(str.size() - end.size(), end.size(), end);
}

std::vector<std::string> StringSplit(const std::string& str, const std::string& split)
{
    std::vector<std::string> result;
    std::string s = str;
    size_t pos = 0;

    while ((pos = s.find(split)) != std::string::npos) {
        result.push_back(s.substr(0, pos));
        s = s.substr(pos + split.size());
    }

    result.push_back(s);

    return result;
}

class SDLBRefFilter : public BRefFilter
{
public:
    SDLBRefFilter(const SDL_DialogFileFilter *filters, int nfilters) :
        BRefFilter(),
        m_filters(filters),
        m_nfilters(nfilters)
    {
    }

    virtual bool Filter(const entry_ref *ref, BNode *node, struct stat_beos *stat, const char *mimeType) override
    {
        BEntry entry(ref);
        BPath path;
        entry.GetPath(&path);
        std::string result = path.Path();

        if (!m_filters)
            return true;

        struct stat info;
        node->GetStat(&info);
        if (S_ISDIR(info.st_mode))
            return true;

        for (int i = 0; i < m_nfilters; i++) {
            for (const auto& suffix : StringSplit(m_filters[i].pattern, ";")) {
                if (StringEndsWith(result, std::string(".") + suffix)) {
                    return true;
                }
            }
        }

        return false;
    }

private:
    const SDL_DialogFileFilter * const m_filters;
    int m_nfilters;
};

class CallbackLooper : public BLooper
{
public:
    CallbackLooper(SDL_DialogFileCallback callback, void *userdata) :
        m_callback(callback),
        m_userdata(userdata),
        m_files(),
        m_messenger(),
        m_panel(),
        m_filter()
    {
    }

    ~CallbackLooper()
    {
        delete m_messenger;
        delete m_panel;
        delete m_filter;
    }

    void SetToBeFreed(BMessenger *messenger, BFilePanel *panel, SDLBRefFilter *filter)
    {
        m_messenger = messenger;
        m_panel = panel;
        m_filter = filter;
    }

    virtual void MessageReceived(BMessage *msg) override
    {
        entry_ref file;
        BPath path;
        BEntry entry;
        std::string result;
        const char *filename;
        int32 nFiles = 0;

        switch (msg->what)
        {
        case B_REFS_RECEIVED: // Open
            msg->GetInfo("refs", NULL, &nFiles);
            for (int i = 0; i < nFiles; i++) {
                msg->FindRef("refs", i, &file);
                entry.SetTo(&file);
                entry.GetPath(&path);
                result = path.Path();
                m_files.push_back(result);
            }
            break;

        case B_SAVE_REQUESTED: // Save
            msg->FindRef("directory", &file);
            entry.SetTo(&file);
            entry.GetPath(&path);
            result = path.Path();
            result += "/";
            msg->FindString("name", &filename);
            result += filename;
            m_files.push_back(result);
            break;

        case B_CANCEL: // Whenever the dialog is closed (Cancel but also after Open and Save)
        {
            nFiles = m_files.size();
            const char* files[nFiles + 1];
            for (int i = 0; i < nFiles; i++) {
                files[i] = m_files[i].c_str();
            }
            files[nFiles] = NULL;
            m_callback(m_userdata, files, -1);
            Quit();
            SDL_QuitBeApp();
            delete this;
        }
            break;

        default:
            BHandler::MessageReceived(msg);
            break;
        }
    }

private:
    SDL_DialogFileCallback m_callback;
    void *m_userdata;
    std::vector<std::string> m_files;

    // Only to free stuff later
    BMessenger *m_messenger;
    BFilePanel *m_panel;
    SDLBRefFilter *m_filter;
};

void ShowDialog(bool save, SDL_DialogFileCallback callback, void *userdata, bool many, bool modal, const SDL_DialogFileFilter *filters, int nfilters, bool folder, const char *location)
{
    if (!SDL_InitBeApp()) {
        char* err = SDL_strdup(SDL_GetError());
        SDL_SetError("Couldn't init Be app: %s", err);
        SDL_free(err);
        callback(userdata, NULL, -1);
        return;
    }

    if (filters) {
        const char *msg = validate_filters(filters, nfilters);

        if (msg) {
            SDL_SetError("%s", msg);
            callback(userdata, NULL, -1);
            return;
        }
    }

    if (SDL_GetHint(SDL_HINT_FILE_DIALOG_DRIVER) != NULL) {
        SDL_SetError("File dialog driver unsupported");
        callback(userdata, NULL, -1);
        return;
    }

    // No unique_ptr's because they need to survive the end of the function
    CallbackLooper *looper = new(std::nothrow) CallbackLooper(callback, userdata);
    BMessenger *messenger = new(std::nothrow) BMessenger(NULL, looper);
    SDLBRefFilter *filter = new(std::nothrow) SDLBRefFilter(filters, nfilters);

    if (looper == NULL || messenger == NULL || filter == NULL) {
        SDL_free(looper);
        SDL_free(messenger);
        SDL_free(filter);
        SDL_OutOfMemory();
        callback(userdata, NULL, -1);
        return;
    }

    BEntry entry;
    entry_ref entryref;
    if (location) {
        entry.SetTo(location);
        entry.GetRef(&entryref);
    }

    BFilePanel *panel = new BFilePanel(save ? B_SAVE_PANEL : B_OPEN_PANEL, messenger, location ? &entryref : NULL, folder ? B_DIRECTORY_NODE : B_FILE_NODE, many, NULL, filter, modal);
    looper->SetToBeFreed(messenger, panel, filter);
    looper->Run();
    panel->Show();
}

void SDL_ShowOpenFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location, bool allow_many)
{
    ShowDialog(false, callback, userdata, allow_many == true, !!window, filters, nfilters, false, default_location);
}

void SDL_ShowSaveFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location)
{
    ShowDialog(true, callback, userdata, false, !!window, filters, nfilters, false, default_location);
}

void SDL_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const char* default_location, bool allow_many)
{
    ShowDialog(false, callback, userdata, allow_many == true, !!window, NULL, 0, true, default_location);
}
