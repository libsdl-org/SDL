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
#include "../SDL_dialog_utils.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include "../../core/windows/SDL_windows.h"
#include "../../thread/SDL_systhread.h"

// If this number is too small, selecting too many files will give an error
#define SELECTLIST_SIZE 65536

typedef struct
{
    bool is_save;
    wchar_t *filters_str;
    int nfilters;
    char *default_file;
    SDL_Window *parent;
    DWORD flags;
    bool allow_many;
    SDL_DialogFileCallback callback;
    void *userdata;
    char *title;
    char *accept;
    char *cancel;
} winArgs;

typedef struct
{
    SDL_Window *parent;
    bool allow_many;
    SDL_DialogFileCallback callback;
    char *default_folder;
    void *userdata;
    char *title;
    char *accept;
    char *cancel;
} winFArgs;

void freeWinArgs(winArgs *args)
{
    SDL_free(args->default_file);
    SDL_free(args->filters_str);
    SDL_free(args->title);
    SDL_free(args->accept);
    SDL_free(args->cancel);

    SDL_free(args);
}

void freeWinFArgs(winFArgs *args)
{
    SDL_free(args->default_folder);
    SDL_free(args->title);
    SDL_free(args->accept);
    SDL_free(args->cancel);

    SDL_free(args);
}

/** Converts dialog.nFilterIndex to SDL-compatible value */
int getFilterIndex(int as_reported_by_windows)
{
    return as_reported_by_windows - 1;
}

char *clear_filt_names(const char *filt)
{
    char *cleared = SDL_strdup(filt);

    for (char *c = cleared; *c; c++) {
        /* 0x01 bytes are used as temporary replacement for the various 0x00
           bytes required by Win32 (one null byte between each filter, two at
           the end of the filters). Filter out these bytes from the filter names
           to avoid early-ending the filters if someone puts two consecutive
           0x01 bytes in their filter names. */
        if (*c == '\x01') {
            *c = ' ';
        }
    }

    return cleared;
}

bool windows_ShowModernFileFolderDialog(SDL_FileDialogType dialog_type, const char *default_file, SDL_Window *parent, bool allow_many, SDL_DialogFileCallback callback, void *userdata, const char *title, const char *accept, const char *cancel, wchar_t *filter_wchar, int nfilters)
{
    bool is_save = dialog_type == SDL_FILEDIALOG_SAVEFILE;
    bool is_folder = dialog_type == SDL_FILEDIALOG_OPENFOLDER;

    if (is_save) {
        // Just in case; the code relies on that
        allow_many = false;
    }

    IFileDialog *pFileDialog = NULL;
    IFileOpenDialog *pFileOpenDialog = NULL;
    IFileDialog2 *pFileDialog2 = NULL;
    IShellItemArray *pItemArray = NULL;
    IShellItem *pItem = NULL;
    IShellItem *pFolderItem = NULL;
    LPWSTR filePath = NULL;
    COMDLG_FILTERSPEC *filter_data = NULL;
    char **files = NULL;
    wchar_t *title_w = NULL;
    wchar_t *accept_w = NULL;
    wchar_t *cancel_w = NULL;
    FILEOPENDIALOGOPTIONS pfos;

    wchar_t *default_file_w = NULL;
    wchar_t *default_folder_w = NULL;

    bool success = false;
    bool co_init = false;

    if (filter_wchar && nfilters > 0) {
        wchar_t *filter_ptr = filter_wchar;
        filter_data = SDL_calloc(sizeof(COMDLG_FILTERSPEC), nfilters);
        if (!filter_data) {
            goto quit;
        }

        for (int i = 0; i < nfilters; i++) {
            filter_data[i].pszName = filter_ptr;
            filter_ptr += SDL_wcslen(filter_ptr) + 1;
            filter_data[i].pszSpec = filter_ptr;
            filter_ptr += SDL_wcslen(filter_ptr) + 1;
        }

        // assert(*filter_ptr == L'\0');
    }

    if (title && !(title_w = WIN_UTF8ToStringW(title))) {
        goto quit;
    }

    if (accept && !(accept_w = WIN_UTF8ToStringW(accept))) {
        goto quit;
    }

    if (cancel && !(cancel_w = WIN_UTF8ToStringW(cancel))) {
        goto quit;
    }

    if (default_file) {
        default_folder_w = WIN_UTF8ToStringW(default_file);

        if (!default_folder_w) {
            goto quit;
        }

        for (wchar_t *chrptr = default_folder_w; *chrptr; chrptr++) {
            if (*chrptr == L'/' || *chrptr == L'\\') {
                default_file_w = chrptr;
            }
        }

        if (!default_file_w) {
            default_file_w = default_folder_w;
            default_folder_w = NULL;
        } else {
            *default_file_w = L'\0';
            default_file_w++;

            if (SDL_wcslen(default_file_w) == 0) {
                default_file_w = NULL;
            }
        }
    }

#define CHECK(op) if (!SUCCEEDED(op)) { goto quit; }

    CHECK(WIN_CoInitialize());

    co_init = true;

    CHECK(CoCreateInstance(is_save ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void**)&pFileDialog));
    CHECK(pFileDialog->lpVtbl->QueryInterface(pFileDialog, &IID_IFileDialog2, (void**)&pFileDialog2));

    if (allow_many) {
        CHECK(pFileDialog->lpVtbl->QueryInterface(pFileDialog, &IID_IFileOpenDialog, (void**)&pFileOpenDialog));
    }

    CHECK(pFileDialog2->lpVtbl->GetOptions(pFileDialog2, &pfos));

    pfos |= FOS_NOCHANGEDIR;
    if (allow_many) pfos |= FOS_ALLOWMULTISELECT;
    if (is_save) pfos |= FOS_OVERWRITEPROMPT;
    if (is_folder) pfos |= FOS_PICKFOLDERS;

    CHECK(pFileDialog2->lpVtbl->SetOptions(pFileDialog2, pfos));

    if (cancel_w) {
        CHECK(pFileDialog2->lpVtbl->SetCancelButtonLabel(pFileDialog2, cancel_w));
    }

    if (accept_w) {
        CHECK(pFileDialog->lpVtbl->SetOkButtonLabel(pFileDialog, accept_w));
    }

    if (title_w) {
        CHECK(pFileDialog->lpVtbl->SetTitle(pFileDialog, title_w));
    }

    if (filter_data) {
        CHECK(pFileDialog->lpVtbl->SetFileTypes(pFileDialog, nfilters, filter_data));
    }

    // SetFolder would enforce using the same location each and every time, but
    // Windows docs recommend against it
    if (default_folder_w) {
        CHECK(SHCreateItemFromParsingName(default_folder_w, NULL, &IID_IShellItem, (void**)&pFolderItem));
        CHECK(pFileDialog->lpVtbl->SetDefaultFolder(pFileDialog, pFolderItem));
    }

    if (default_file_w) {
        CHECK(pFileDialog->lpVtbl->SetFileName(pFileDialog, default_file_w));
    }

    if (parent) {
        HWND window = (HWND) SDL_GetPointerProperty(SDL_GetWindowProperties(parent), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

        HRESULT hr = pFileDialog->lpVtbl->Show(pFileDialog, window);

        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            const char * const results[] = { NULL };
            UINT selected_filter;

            // This is a one-based index, not zero-based. Doc link in similar comment below
            CHECK(pFileDialog->lpVtbl->GetFileTypeIndex(pFileDialog, &selected_filter));
            callback(userdata, results, selected_filter - 1);
            success = true;
            goto quit;
        } else if (!SUCCEEDED(hr)) {
            goto quit;
        }
    } else {
        HRESULT hr = pFileDialog->lpVtbl->Show(pFileDialog, NULL);

        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            const char * const results[] = { NULL };
            UINT selected_filter;

            // This is a one-based index, not zero-based. Doc link in similar comment below
            CHECK(pFileDialog->lpVtbl->GetFileTypeIndex(pFileDialog, &selected_filter));
            callback(userdata, results, selected_filter - 1);
            success = true;
            goto quit;
        } else if (!SUCCEEDED(hr)) {
            goto quit;
        }
    }

    if (allow_many) {
        DWORD nResults;
        UINT selected_filter;

        CHECK(pFileOpenDialog->lpVtbl->GetResults(pFileOpenDialog, &pItemArray));
        CHECK(pFileDialog->lpVtbl->GetFileTypeIndex(pFileDialog, &selected_filter));
        CHECK(pItemArray->lpVtbl->GetCount(pItemArray, &nResults));

        files = SDL_calloc(nResults + 1, sizeof(char*));
        if (!files) {
            goto quit;
        }
        char** files_ptr = files;

        for (DWORD i = 0; i < nResults; i++) {
            CHECK(pItemArray->lpVtbl->GetItemAt(pItemArray, i, &pItem));
            CHECK(pItem->lpVtbl->GetDisplayName(pItem, SIGDN_FILESYSPATH, &filePath));

            *(files_ptr++) = WIN_StringToUTF8(filePath);

            CoTaskMemFree(filePath);
            filePath = NULL;
            pItem->lpVtbl->Release(pItem);
            pItem = NULL;
        }

        callback(userdata, (const char * const *) files, selected_filter - 1);
        success = true;
    } else {
        // This is a one-based index, not zero-based.
        // https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifiledialog-getfiletypeindex#parameters
        UINT selected_filter;

        CHECK(pFileDialog->lpVtbl->GetResult(pFileDialog, &pItem));
        CHECK(pFileDialog->lpVtbl->GetFileTypeIndex(pFileDialog, &selected_filter));
        CHECK(pItem->lpVtbl->GetDisplayName(pItem, SIGDN_FILESYSPATH, &filePath));

        char *file = WIN_StringToUTF8(filePath);
        if (!file) {
            goto quit;
        }
        const char * const results[] = { file, NULL };
        callback(userdata, results, selected_filter - 1);
        success = true;
        SDL_free(file);
    }

    success = true;

#undef CHECK

quit:
    if (!success) {
        WIN_SetError("dialogg");
        callback(userdata, NULL, -1);
    }

    if (co_init) {
        WIN_CoUninitialize();
    }

    if (pFileOpenDialog) {
        pFileOpenDialog->lpVtbl->Release(pFileOpenDialog);
    }

    if (pFileDialog2) {
        pFileDialog2->lpVtbl->Release(pFileDialog2);
    }

    if (pFileDialog) {
        pFileDialog->lpVtbl->Release(pFileDialog);
    }

    if (pItem) {
        pItem->lpVtbl->Release(pItem);
    }

    if (pFolderItem) {
        pFolderItem->lpVtbl->Release(pFolderItem);
    }

    if (pItemArray) {
        pItemArray->lpVtbl->Release(pItemArray);
    }

    if (filePath) {
        CoTaskMemFree(filePath);
    }

    // If both default_file_w and default_folder_w are non-NULL, then
    // default_file_w is a pointer into default_folder_w.
    if (default_folder_w) {
        SDL_free(default_folder_w);
    } else if (default_file_w) {
        SDL_free(default_file_w);
    }

    if (title_w) {
        SDL_free(title_w);
    }

    if (accept_w) {
        SDL_free(accept_w);
    }

    if (cancel_w) {
        SDL_free(cancel_w);
    }

    if (filter_data) {
        SDL_free(filter_data);
    }

    if (files) {
        for (char** files_ptr = files; *files_ptr; files_ptr++) {
            SDL_free(*files_ptr);
        }
        SDL_free(files);
    }

    return success;
}

// TODO: The new version of file dialogs
void windows_ShowFileDialog(void *ptr)
{

    winArgs *args = (winArgs *) ptr;
    bool is_save = args->is_save;
    const char *default_file = args->default_file;
    SDL_Window *parent = args->parent;
    DWORD flags = args->flags;
    bool allow_many = args->allow_many;
    SDL_DialogFileCallback callback = args->callback;
    void *userdata = args->userdata;
    const char *title = args->title;
    const char *accept = args->accept;
    const char *cancel = args->cancel;
    wchar_t *filter_wchar = args->filters_str;
    int nfilters = args->nfilters;

    if (windows_ShowModernFileFolderDialog(is_save ? SDL_FILEDIALOG_SAVEFILE : SDL_FILEDIALOG_OPENFILE, default_file, parent, allow_many, callback, userdata, title, accept, cancel, filter_wchar, nfilters)) {
        return;
    }

    /* GetOpenFileName and GetSaveFileName have the same signature
       (yes, LPOPENFILENAMEW even for the save dialog) */
    typedef BOOL (WINAPI *pfnGetAnyFileNameW)(LPOPENFILENAMEW);
    typedef DWORD (WINAPI *pfnCommDlgExtendedError)(void);
    HMODULE lib = LoadLibraryW(L"Comdlg32.dll");
    pfnGetAnyFileNameW pGetAnyFileName = NULL;
    pfnCommDlgExtendedError pCommDlgExtendedError = NULL;

    if (lib) {
        pGetAnyFileName = (pfnGetAnyFileNameW) GetProcAddress(lib, is_save ? "GetSaveFileNameW" : "GetOpenFileNameW");
        pCommDlgExtendedError = (pfnCommDlgExtendedError) GetProcAddress(lib, "CommDlgExtendedError");
    } else {
        SDL_SetError("Couldn't load Comdlg32.dll");
        callback(userdata, NULL, -1);
        return;
    }

    if (!pGetAnyFileName) {
        SDL_SetError("Couldn't load GetOpenFileName/GetSaveFileName from library");
        callback(userdata, NULL, -1);
        return;
    }

    if (!pCommDlgExtendedError) {
        SDL_SetError("Couldn't load CommDlgExtendedError from library");
        callback(userdata, NULL, -1);
        return;
    }

    HWND window = NULL;

    if (parent) {
        window = (HWND) SDL_GetPointerProperty(SDL_GetWindowProperties(parent), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    }

    wchar_t *filebuffer; // lpstrFile
    wchar_t initfolder[MAX_PATH] = L""; // lpstrInitialDir

    /* If SELECTLIST_SIZE is too large, putting filebuffer on the stack might
       cause an overflow */
    filebuffer = (wchar_t *) SDL_malloc(SELECTLIST_SIZE * sizeof(wchar_t));

    // Necessary for the return code below
    SDL_memset(filebuffer, 0, SELECTLIST_SIZE * sizeof(wchar_t));

    if (default_file) {
        /* On Windows 10, 11 and possibly others, lpstrFile can be initialized
           with a path and the dialog will start at that location, but *only if
           the path contains a filename*. If it ends with a folder (directory
           separator), it fails with 0x3002 (12290) FNERR_INVALIDFILENAME. For
           that specific case, lpstrInitialDir must be used instead, but just
           for that case, because lpstrInitialDir doesn't support file names.

           On top of that, lpstrInitialDir hides a special algorithm that
           decides which folder to actually use as starting point, which may or
           may not be the one provided, or some other unrelated folder. Also,
           the algorithm changes between platforms. Assuming the documentation
           is correct, the algorithm is there under 'lpstrInitialDir':

           https://learn.microsoft.com/en-us/windows/win32/api/commdlg/ns-commdlg-openfilenamew

           Finally, lpstrFile does not support forward slashes. lpstrInitialDir
           does, though. */

        char last_c = default_file[SDL_strlen(default_file) - 1];

        if (last_c == '\\' || last_c == '/') {
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, default_file, -1, initfolder, MAX_PATH);
        } else {
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, default_file, -1, filebuffer, MAX_PATH);

            for (int i = 0; i < SELECTLIST_SIZE; i++) {
                if (filebuffer[i] == L'/') {
                    filebuffer[i] = L'\\';
                }
            }
        }
    }

    wchar_t *title_w = NULL;

    if (title) {
        title_w = WIN_UTF8ToStringW(title);
        if (!title_w) {
            SDL_free(filebuffer);
            callback(userdata, NULL, -1);
            return;
        }
    }

    OPENFILENAMEW dialog;
    dialog.lStructSize = sizeof(OPENFILENAME);
    dialog.hwndOwner = window;
    dialog.hInstance = 0;
    dialog.lpstrFilter = filter_wchar;
    dialog.lpstrCustomFilter = NULL;
    dialog.nMaxCustFilter = 0;
    dialog.nFilterIndex = 0;
    dialog.lpstrFile = filebuffer;
    dialog.nMaxFile = SELECTLIST_SIZE;
    dialog.lpstrFileTitle = NULL;
    dialog.lpstrInitialDir = *initfolder ? initfolder : NULL;
    dialog.lpstrTitle = title_w;
    dialog.Flags = flags | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    dialog.nFileOffset = 0;
    dialog.nFileExtension = 0;
    dialog.lpstrDefExt = NULL;
    dialog.lCustData = 0;
    dialog.lpfnHook = NULL;
    dialog.lpTemplateName = NULL;
    // Skipped many mac-exclusive and reserved members
    dialog.FlagsEx = 0;

    BOOL result = pGetAnyFileName(&dialog);

    SDL_free(title_w);

    if (result) {
        if (!(flags & OFN_ALLOWMULTISELECT)) {
            // File is a C string stored in dialog.lpstrFile
            char *chosen_file = WIN_StringToUTF8W(dialog.lpstrFile);
            const char *opts[2] = { chosen_file, NULL };
            callback(userdata, opts, getFilterIndex(dialog.nFilterIndex));
            SDL_free(chosen_file);
        } else {
            /* File is either a C string if the user chose a single file, else
               it's a series of strings formatted like:

                   "C:\\path\\to\\folder\0filename1.ext\0filename2.ext\0\0"

               The code below will only stop on a double NULL in all cases, so
               it is important that the rest of the buffer has been zeroed. */
            char chosen_folder[MAX_PATH];
            char chosen_file[MAX_PATH];
            wchar_t *file_ptr = dialog.lpstrFile;
            size_t nfiles = 0;
            size_t chosen_folder_size;
            char **chosen_files_list = (char **) SDL_malloc(sizeof(char *) * (nfiles + 1));

            if (!chosen_files_list) {
                callback(userdata, NULL, -1);
                SDL_free(filebuffer);
                return;
            }

            chosen_files_list[nfiles] = NULL;

            if (WideCharToMultiByte(CP_UTF8, 0, file_ptr, -1, chosen_folder, MAX_PATH, NULL, NULL) >= MAX_PATH) {
                SDL_SetError("Path too long or invalid character in path");
                SDL_free(chosen_files_list);
                callback(userdata, NULL, -1);
                SDL_free(filebuffer);
                return;
            }

            chosen_folder_size = SDL_strlen(chosen_folder);
            SDL_strlcpy(chosen_file, chosen_folder, MAX_PATH);
            chosen_file[chosen_folder_size] = '\\';

            file_ptr += SDL_strlen(chosen_folder) + 1;

            while (*file_ptr) {
                nfiles++;
                char **new_cfl = (char **) SDL_realloc(chosen_files_list, sizeof(char*) * (nfiles + 1));

                if (!new_cfl) {
                    for (size_t i = 0; i < nfiles - 1; i++) {
                        SDL_free(chosen_files_list[i]);
                    }

                    SDL_free(chosen_files_list);
                    callback(userdata, NULL, -1);
                    SDL_free(filebuffer);
                    return;
                }

                chosen_files_list = new_cfl;
                chosen_files_list[nfiles] = NULL;

                int diff = ((int) chosen_folder_size) + 1;

                if (WideCharToMultiByte(CP_UTF8, 0, file_ptr, -1, chosen_file + diff, MAX_PATH - diff, NULL, NULL) >= MAX_PATH - diff) {
                    SDL_SetError("Path too long or invalid character in path");

                    for (size_t i = 0; i < nfiles - 1; i++) {
                        SDL_free(chosen_files_list[i]);
                    }

                    SDL_free(chosen_files_list);
                    callback(userdata, NULL, -1);
                    SDL_free(filebuffer);
                    return;
                }

                file_ptr += SDL_strlen(chosen_file) + 1 - diff;

                chosen_files_list[nfiles - 1] = SDL_strdup(chosen_file);

                if (!chosen_files_list[nfiles - 1]) {
                    for (size_t i = 0; i < nfiles - 1; i++) {
                        SDL_free(chosen_files_list[i]);
                    }

                    SDL_free(chosen_files_list);
                    callback(userdata, NULL, -1);
                    SDL_free(filebuffer);
                    return;
                }
            }

            // If the user chose only one file, it's all just one string
            if (nfiles == 0) {
                nfiles++;
                char **new_cfl = (char **) SDL_realloc(chosen_files_list, sizeof(char*) * (nfiles + 1));

                if (!new_cfl) {
                    SDL_free(chosen_files_list);
                    callback(userdata, NULL, -1);
                    SDL_free(filebuffer);
                    return;
                }

                chosen_files_list = new_cfl;
                chosen_files_list[nfiles] = NULL;
                chosen_files_list[nfiles - 1] = SDL_strdup(chosen_folder);

                if (!chosen_files_list[nfiles - 1]) {
                    SDL_free(chosen_files_list);
                    callback(userdata, NULL, -1);
                    SDL_free(filebuffer);
                    return;
                }
            }

            callback(userdata, (const char * const*) chosen_files_list, getFilterIndex(dialog.nFilterIndex));

            for (size_t i = 0; i < nfiles; i++) {
                SDL_free(chosen_files_list[i]);
            }

            SDL_free(chosen_files_list);
        }
    } else {
        DWORD error = pCommDlgExtendedError();
        // Error code 0 means the user clicked the cancel button.
        if (error == 0) {
            /* Unlike SDL's handling of errors, Windows does reset the error
               code to 0 after calling GetOpenFileName if another Windows
               function before set a different error code, so it's safe to
               check for success. */
            const char *opts[1] = { NULL };
            callback(userdata, opts, getFilterIndex(dialog.nFilterIndex));
        } else {
            SDL_SetError("Windows error, CommDlgExtendedError: %ld", pCommDlgExtendedError());
            callback(userdata, NULL, -1);
        }
    }

    SDL_free(filebuffer);
}

int windows_file_dialog_thread(void *ptr)
{
    windows_ShowFileDialog(ptr);
    freeWinArgs(ptr);
    return 0;
}

int CALLBACK browse_callback_proc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    switch (uMsg) {
    case BFFM_INITIALIZED:
        if (lpData) {
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
        }
        break;
    case BFFM_SELCHANGED:
        break;
    case BFFM_VALIDATEFAILED:
        break;
    default:
        break;
    }
    return 0;
}

void windows_ShowFolderDialog(void *ptr)
{
    winFArgs *args = (winFArgs *) ptr;
    SDL_Window *window = args->parent;
    SDL_DialogFileCallback callback = args->callback;
    void *userdata = args->userdata;
    HWND parent = NULL;
    int allow_many = args->allow_many;
    char *default_folder = args->default_folder;
    const char *title = args->title;
    const char *accept = args->accept;
    const char *cancel = args->cancel;

    if (windows_ShowModernFileFolderDialog(SDL_FILEDIALOG_OPENFOLDER, default_folder, window, allow_many, callback, userdata, title, accept, cancel, NULL, 0)) {
        return;
    }

    if (window) {
        parent = (HWND) SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    }

    wchar_t *title_w = NULL;

    if (title) {
        title_w = WIN_UTF8ToStringW(title);
        if (!title_w) {
            callback(userdata, NULL, -1);
            return;
        }
    }

    wchar_t buffer[MAX_PATH];

    BROWSEINFOW dialog;
    dialog.hwndOwner = parent;
    dialog.pidlRoot = NULL;
    dialog.pszDisplayName = buffer;
    dialog.lpszTitle = title_w;
    dialog.ulFlags = BIF_USENEWUI;
    dialog.lpfn = browse_callback_proc;
    dialog.lParam = (LPARAM)args->default_folder;
    dialog.iImage = 0;

    LPITEMIDLIST lpItem = SHBrowseForFolderW(&dialog);

    SDL_free(title_w);

    if (lpItem != NULL) {
        SHGetPathFromIDListW(lpItem, buffer);
        char *chosen_file = WIN_StringToUTF8W(buffer);
        const char *files[2] = { chosen_file, NULL };
        callback(userdata, (const char * const*) files, -1);
        SDL_free(chosen_file);
    } else {
        const char *files[1] = { NULL };
        callback(userdata, (const char * const*) files, -1);
    }
}

int windows_folder_dialog_thread(void *ptr)
{
    windows_ShowFolderDialog(ptr);
    freeWinFArgs((winFArgs *)ptr);
    return 0;
}

wchar_t *win_get_filters(const SDL_DialogFileFilter *filters, int nfilters)
{
    wchar_t *filter_wchar = NULL;

    if (filters) {
        // '\x01' is used in place of a null byte
        // suffix needs two null bytes in case the filter list is empty
        char *filterlist = convert_filters(filters, nfilters, clear_filt_names,
                                           "", "", "\x01\x01", "", "\x01",
                                           "\x01", "*.", ";*.", "");

        if (!filterlist) {
            return NULL;
        }

        int filter_len = (int)SDL_strlen(filterlist);

        for (char *c = filterlist; *c; c++) {
            if (*c == '\x01') {
                *c = '\0';
            }
        }

        int filter_wlen = MultiByteToWideChar(CP_UTF8, 0, filterlist, filter_len, NULL, 0);
        filter_wchar = (wchar_t *)SDL_malloc(filter_wlen * sizeof(wchar_t));
        if (!filter_wchar) {
            SDL_free(filterlist);
            return NULL;
        }

        MultiByteToWideChar(CP_UTF8, 0, filterlist, filter_len, filter_wchar, filter_wlen);

        SDL_free(filterlist);
    }

    return filter_wchar;
}

static void ShowFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location, bool allow_many, bool is_save, const char *title, const char *accept, const char *cancel)
{
    winArgs *args;
    SDL_Thread *thread;
    wchar_t *filters_str;

    if (SDL_GetHint(SDL_HINT_FILE_DIALOG_DRIVER) != NULL) {
        SDL_SetError("File dialog driver unsupported");
        callback(userdata, NULL, -1);
        return;
    }

    args = (winArgs *)SDL_malloc(sizeof(*args));
    if (args == NULL) {
        callback(userdata, NULL, -1);
        return;
    }

    filters_str = win_get_filters(filters, nfilters);

    DWORD flags = 0;
    if (allow_many) {
        flags |= OFN_ALLOWMULTISELECT;
    }
    if (is_save) {
        flags |= OFN_OVERWRITEPROMPT;
    }

    if (!filters_str && filters) {
        callback(userdata, NULL, -1);
        SDL_free(args);
        return;
    }

    args->is_save = is_save;
    args->filters_str = filters_str;
    args->nfilters = nfilters;
    args->default_file = default_location ? SDL_strdup(default_location) : NULL;
    args->parent = window;
    args->flags = flags;
    args->allow_many = allow_many;
    args->callback = callback;
    args->userdata = userdata;
    args->title = title ? SDL_strdup(title) : NULL;
    args->accept = accept ? SDL_strdup(accept) : NULL;
    args->cancel = cancel ? SDL_strdup(cancel) : NULL;

    thread = SDL_CreateThread(windows_file_dialog_thread, "SDL_Windows_ShowFileDialog", (void *) args);

    if (thread == NULL) {
        callback(userdata, NULL, -1);
        // The thread won't have run, therefore the data won't have been freed
        freeWinArgs(args);
        return;
    }

    SDL_DetachThread(thread);
}

void ShowFolderDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const char *default_location, bool allow_many, const char *title, const char *accept, const char *cancel)
{
    winFArgs *args;
    SDL_Thread *thread;

    if (SDL_GetHint(SDL_HINT_FILE_DIALOG_DRIVER) != NULL) {
        SDL_SetError("File dialog driver unsupported");
        callback(userdata, NULL, -1);
        return;
    }

    args = (winFArgs *)SDL_malloc(sizeof(*args));
    if (args == NULL) {
        callback(userdata, NULL, -1);
        return;
    }

    args->parent = window;
    args->allow_many = allow_many;
    args->callback = callback;
    args->default_folder = default_location ? SDL_strdup(default_location) : NULL;
    args->userdata = userdata;
    args->title = title ? SDL_strdup(title) : NULL;
    args->accept = accept ? SDL_strdup(accept) : NULL;
    args->cancel = cancel ? SDL_strdup(cancel) : NULL;

    thread = SDL_CreateThread(windows_folder_dialog_thread, "SDL_Windows_ShowFolderDialog", (void *) args);

    if (thread == NULL) {
        callback(userdata, NULL, -1);
        // The thread won't have run, therefore the data won't have been freed
        freeWinFArgs(args);
        return;
    }

    SDL_DetachThread(thread);
}

void SDL_SYS_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props)
{
    /* The internal functions will start threads, and the properties may be freed as soon as this function returns.
       Save a copy of what we need before invoking the functions and starting the threads. */
    SDL_Window *window = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, NULL);
    SDL_DialogFileFilter *filters = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, NULL);
    int nfilters = (int) SDL_GetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 0);
    bool allow_many = SDL_GetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);
    const char *default_location = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, NULL);
    const char *title = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, NULL);
    const char *accept = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, NULL);
    const char *cancel = SDL_GetStringProperty(props, SDL_PROP_FILE_DIALOG_CANCEL_STRING, NULL);
    bool is_save = false;

    switch (type) {
    case SDL_FILEDIALOG_SAVEFILE:
        is_save = true;
        SDL_FALLTHROUGH;
    case SDL_FILEDIALOG_OPENFILE:
        ShowFileDialog(callback, userdata, window, filters, nfilters, default_location, allow_many, is_save, title, accept, cancel);
        break;

    case SDL_FILEDIALOG_OPENFOLDER:
        ShowFolderDialog(callback, userdata, window, default_location, allow_many, title, accept, cancel);
        break;
    };
}
