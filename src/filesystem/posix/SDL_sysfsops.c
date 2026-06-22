/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#if defined(SDL_FSOPS_POSIX)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_thread.h>
#include "../SDL_sysfilesystem.h"
#include "../../SDL_hashtable.h"
#include "../../events/SDL_events_c.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef SDL_PLATFORM_ANDROID
#include "../../core/android/SDL_android.h"
#endif

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#include <fcntl.h>
#include <limits.h>
#endif

bool SDL_SYS_EnumerateDirectory(const char *path, SDL_EnumerateDirectoryCallback cb, void *userdata)
{
    char *apath = NULL;  // absolute path (for Android, iOS, etc). Overrides `path`.

#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_IOS)
    if (*path == '\0') {
        return SDL_SetError("No such file or directory");
    } else if (*path != '/') {
        #ifdef SDL_PLATFORM_ANDROID
        if (SDL_strncmp(path, "assets://", 9) == 0) {
            char *pathwithsep = NULL;
            SDL_asprintf(&pathwithsep, "%s%s", path, (path[SDL_strlen(path) - 1] != '/') ? "/" : "");
            const bool retval = pathwithsep ? Android_JNI_EnumerateAssetDirectory(pathwithsep, cb, userdata) : false;
            SDL_free(pathwithsep);
            return retval;
        }
        SDL_asprintf(&apath, "%s/%s", SDL_GetAndroidInternalStoragePath(), path);
        #elif defined(SDL_PLATFORM_IOS)
        char *base = SDL_GetPrefPath("", "");
        if (!base) {
            return false;
        }

        SDL_asprintf(&apath, "%s%s", base, path);
        SDL_free(base);
        #endif

        if (!apath) {
            return false;
        }
    }
#elif 0  // this is just for testing that `apath` works when you aren't on iOS or Android.
    if (*path != '/') {
        char *c = SDL_SYS_GetCurrentDirectory();
        SDL_asprintf(&apath, "%s%s", c, path);
        SDL_free(c);
        if (!apath) {
            return false;
        }
    }
#endif

    char *pathwithsep = NULL;
    int pathwithseplen = SDL_asprintf(&pathwithsep, "%s%s", apath ? apath : path, (apath ? *apath : *path) ? "/" : "");
    const size_t extralen = apath ? (SDL_strlen(apath) - SDL_strlen(path)) : 0;
    SDL_free(apath);
    if ((pathwithseplen == -1) || (!pathwithsep)) {
        return false;
    }

    // trim down to a single path separator at the end, in case the caller added one or more.
    pathwithseplen--;
    while ((pathwithseplen > 0) && (pathwithsep[pathwithseplen - 1] == '/')) {
        pathwithsep[pathwithseplen--] = '\0';
    }

    DIR *dir = opendir(pathwithsep);
    if (!dir) {
#ifdef SDL_PLATFORM_ANDROID  // Maybe it's an asset... that didn't use an "assets://" URL?
        if (*pathwithsep != '/') {  // don't fall back to asset tree for absolute paths, in case opendir() failed for other reasons, like opendir("/") returning EACCES.
            const bool retval = Android_JNI_EnumerateAssetDirectory(pathwithsep + extralen, cb, userdata);
            SDL_free(pathwithsep);
            return retval;
        }
#endif
        SDL_free(pathwithsep);
        return SDL_SetError("Can't open directory: %s", strerror(errno));
    }

    SDL_EnumerationResult result = SDL_ENUM_CONTINUE;
    struct dirent *ent;
    while ((result == SDL_ENUM_CONTINUE) && ((ent = readdir(dir)) != NULL)) {
        const char *name = ent->d_name;
        if ((SDL_strcmp(name, ".") == 0) || (SDL_strcmp(name, "..") == 0)) {
            continue;
        }
        result = cb(userdata, pathwithsep + extralen, name);
    }

    closedir(dir);

    SDL_free(pathwithsep);

    return (result != SDL_ENUM_FAILURE);
}

bool SDL_SYS_RemovePath(const char *path)
{
    int rc;

#ifdef SDL_PLATFORM_ANDROID
    if (*path == '/') {
        rc = remove(path);
    } else {
        char *apath = NULL;
        SDL_asprintf(&apath, "%s/%s", SDL_GetAndroidInternalStoragePath(), path);
        if (!apath) {
            return false;
        }
        rc = remove(apath);
        SDL_free(apath);
    }
#elif defined(SDL_PLATFORM_IOS)
    if (*path == '/') {
        rc = remove(path);
    } else {
        char *base = SDL_GetPrefPath("", "");
        if (!base) {
            return false;
        }

        char *apath = NULL;
        SDL_asprintf(&apath, "%s%s", base, path);
        SDL_free(base);
        if (!apath) {
            return false;
        }
        rc = remove(apath);
        SDL_free(apath);
    }
#else
    rc = remove(path);
#endif
    if (rc < 0) {
        if (errno == ENOENT) {
            // It's already gone, this is a success
            return true;
        }
        return SDL_SetError("Can't remove path: %s", strerror(errno));
    }
    return true;
}

bool SDL_SYS_RenamePath(const char *oldpath, const char *newpath)
{
    int rc;

#ifdef SDL_PLATFORM_ANDROID
    char *aoldpath = NULL;
    char *anewpath = NULL;
    if (*oldpath != '/') {
        SDL_asprintf(&aoldpath, "%s/%s", SDL_GetAndroidInternalStoragePath(), oldpath);
        if (!aoldpath) {
            return false;
        }
        oldpath = aoldpath;
    }
    if (*newpath != '/') {
        SDL_asprintf(&anewpath, "%s/%s", SDL_GetAndroidInternalStoragePath(), newpath);
        if (!anewpath) {
            SDL_free(aoldpath);
            return false;
        }
        newpath = anewpath;
    }
    rc = rename(oldpath, newpath);
    SDL_free(aoldpath);
    SDL_free(anewpath);
#elif defined(SDL_PLATFORM_IOS)
    char *base = NULL;
    if (*oldpath != '/' || *newpath != '/') {
        base = SDL_GetPrefPath("", "");
        if (!base) {
            return false;
        }
    }

    char *aoldpath = NULL;
    char *anewpath = NULL;
    if (*oldpath != '/') {
        SDL_asprintf(&aoldpath, "%s%s", base, oldpath);
        if (!aoldpath) {
            SDL_free(base);
            return false;
        }
        oldpath = aoldpath;
    }
    if (*newpath != '/') {
        SDL_asprintf(&anewpath, "%s%s", base, newpath);
        if (!anewpath) {
            SDL_free(base);
            SDL_free(aoldpath);
            return false;
        }
        newpath = anewpath;
    }
    rc = rename(oldpath, newpath);
    SDL_free(base);
    SDL_free(aoldpath);
    SDL_free(anewpath);
#else
    rc = rename(oldpath, newpath);
#endif
    if (rc < 0) {
        return SDL_SetError("Can't rename path: %s", strerror(errno));
    }
    return true;
}

bool SDL_SYS_CopyFile(const char *oldpath, const char *newpath)
{
    char *buffer = NULL;
    SDL_IOStream *input = NULL;
    SDL_IOStream *output = NULL;
    const size_t maxlen = 4096;
    size_t len;
    bool result = false;

    input = SDL_IOFromFile(oldpath, "rb");
    if (!input) {
        goto done;
    }

    output = SDL_IOFromFile(newpath, "wb");
    if (!output) {
        goto done;
    }

    buffer = (char *)SDL_malloc(maxlen);
    if (!buffer) {
        goto done;
    }

    while ((len = SDL_ReadIO(input, buffer, maxlen)) > 0) {
        if (SDL_WriteIO(output, buffer, len) < len) {
            goto done;
        }
    }
    if (SDL_GetIOStatus(input) != SDL_IO_STATUS_EOF) {
        goto done;
    }

    SDL_CloseIO(input);
    input = NULL;

    if (!SDL_FlushIO(output)) {
        goto done;
    }

    result = SDL_CloseIO(output);
    output = NULL;  // it's gone, even if it failed.

done:
    if (output) {
        SDL_CloseIO(output);
    }
    if (input) {
        SDL_CloseIO(input);
    }
    SDL_free(buffer);

    return result;
}

bool SDL_SYS_CreateDirectory(const char *path)
{
    int rc;

#ifdef SDL_PLATFORM_ANDROID
    if (*path == '/') {
        rc = mkdir(path, 0770);
    } else {
        char *apath = NULL;
        SDL_asprintf(&apath, "%s/%s", SDL_GetAndroidInternalStoragePath(), path);
        if (!apath) {
            return false;
        }
        rc = mkdir(apath, 0770);
        SDL_free(apath);
    }
#elif defined(SDL_PLATFORM_IOS)
    if (*path == '/') {
        rc = mkdir(path, 0770);
    } else {
        char *base = SDL_GetPrefPath("", "");
        if (!base) {
            return false;
        }

        char *apath = NULL;
        SDL_asprintf(&apath, "%s%s", base, path);
        SDL_free(base);
        if (!apath) {
            return false;
        }
        rc = mkdir(apath, 0770);
        SDL_free(apath);
    }
#else
    rc = mkdir(path, 0770);
#endif
    if (rc < 0) {
        const int origerrno = errno;
        if (origerrno == EEXIST) {
            struct stat statbuf;
            if ((stat(path, &statbuf) == 0) && (S_ISDIR(statbuf.st_mode))) {
                return true;  // it already exists and it's a directory, consider it success.
            }
        }
        return SDL_SetError("Can't create directory: %s", strerror(origerrno));
    }
    return true;
}

bool SDL_SYS_GetPathInfo(const char *path, SDL_PathInfo *info)
{
    struct stat statbuf;
    int rc;

#ifdef SDL_PLATFORM_ANDROID
    if (*path == '\0') {
        return SDL_SetError("No such file or directory");
    } else if (*path == '/') {
        rc = stat(path, &statbuf);
    } else if (SDL_strncmp(path, "assets://", 9) == 0) {
        return Android_JNI_GetAssetPathInfo(path, info);
    } else {
        char *apath = NULL;
        SDL_asprintf(&apath, "%s/%s", SDL_GetAndroidInternalStoragePath(), path);
        if (!apath) {
            return false;
        }
        rc = stat(apath, &statbuf);
        SDL_free(apath);
    }
    if (rc < 0) {  // Maybe it's an asset... that didn't use an "assets://" URL?
        return Android_JNI_GetAssetPathInfo(path, info);
    }
#elif defined(SDL_PLATFORM_IOS)
    if (*path == '/') {
        rc = stat(path, &statbuf);
    } else {
        char *base = SDL_GetPrefPath("", "");
        if (!base) {
            return false;
        }

        char *apath = NULL;
        SDL_asprintf(&apath, "%s%s", base, path);
        SDL_free(base);
        if (!apath) {
            return false;
        }
        rc = stat(apath, &statbuf);
        SDL_free(apath);

        if (rc < 0) {
            rc = stat(path, &statbuf);
        }
    }
#else
    rc = stat(path, &statbuf);
#endif
    if (rc < 0) {
        return SDL_SetError("Can't stat: %s", strerror(errno));
    } else if (S_ISREG(statbuf.st_mode)) {
        info->type = SDL_PATHTYPE_FILE;
        info->size = (Uint64) statbuf.st_size;
    } else if (S_ISDIR(statbuf.st_mode)) {
        info->type = SDL_PATHTYPE_DIRECTORY;
        info->size = 0;
    } else {
        info->type = SDL_PATHTYPE_OTHER;
        info->size = (Uint64) statbuf.st_size;
    }

#if defined(HAVE_ST_MTIM)
    // POSIX.1-2008 standard
    info->create_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_ctim.tv_sec) + statbuf.st_ctim.tv_nsec;
    info->modify_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_mtim.tv_sec) + statbuf.st_mtim.tv_nsec;
    info->access_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_atim.tv_sec) + statbuf.st_atim.tv_nsec;
#elif defined(SDL_PLATFORM_APPLE)
    /* Apple platform stat structs use 'st_*timespec' naming. */
    info->create_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_ctimespec.tv_sec) + statbuf.st_ctimespec.tv_nsec;
    info->modify_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_mtimespec.tv_sec) + statbuf.st_mtimespec.tv_nsec;
    info->access_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_atimespec.tv_sec) + statbuf.st_atimespec.tv_nsec;
#else
    info->create_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_ctime);
    info->modify_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_mtime);
    info->access_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_atime);
#endif
    return true;
}

#ifdef HAVE_INOTIFY
static int inotify_fd = -1;
typedef struct WatchEntry
{
    SDL_PathWatchCallback callback;
    void *user_data;
    size_t path_len;
    char path[]; // directory or file path
} WatchEntry;
static SDL_HashTable *watch_descriptor_table = NULL; // stores WatchEntry for a watch descriptor

static int SDLCALL SDL_FileWatchThread(void *user_data);
static SDL_Thread *file_watch_thread = NULL;
static SDL_Mutex *file_watch_lock = NULL;
static SDL_AtomicInt quit_watch_file;

#ifdef HAVE_INOTIFY_INIT1
static int SDL_inotify_init1(void)
{
    return inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
}
#else
static int SDL_inotify_init1(void)
{
    int fd = inotify_init();
    if (fd < 0) {
        return -1;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}
#endif // HAVE_INOTIFY_INIT1

#endif // HAVE_INOTIFY

bool SDL_SYS_AddPathWatch(const char *path, SDL_PathWatchCallback cb, void *user_data)
{
#ifdef HAVE_INOTIFY
    if (!watch_descriptor_table) {
        watch_descriptor_table = SDL_CreateHashTable(0, false, SDL_HashID, SDL_KeyMatchID, SDL_DestroyHashValue, NULL);
        if (!watch_descriptor_table) {
            return false;
        }
        inotify_fd = SDL_inotify_init1();
        if (inotify_fd == -1) {
            SDL_DestroyHashTable(watch_descriptor_table);
            watch_descriptor_table = NULL;
            return SDL_SetError("Could not initialize inotify: %s", strerror(errno));
        }
        file_watch_lock = SDL_CreateMutex();
        if (!file_watch_lock) {
            SDL_DestroyHashTable(watch_descriptor_table);
            watch_descriptor_table = NULL;
            close(inotify_fd);
            inotify_fd = -1;
            return false;
        }
        SDL_SetAtomicInt(&quit_watch_file, 0);
        file_watch_thread = SDL_CreateThread(SDL_FileWatchThread, "SDL_FileWatch", NULL);
        if (!file_watch_thread) {
            SDL_DestroyHashTable(watch_descriptor_table);
            watch_descriptor_table = NULL;
            close(inotify_fd);
            inotify_fd = -1;
            SDL_DestroyMutex(file_watch_lock);
            file_watch_lock = NULL;
            return false;
        }
    }

    const size_t slen = SDL_strlen(path);
    if (slen >= PATH_MAX) {
        return SDL_SetError("path too long");
    }
    WatchEntry *watch_entry = SDL_malloc(sizeof(*watch_entry) + slen + 1);
    if (!watch_entry) {
        return false;
    }
    watch_entry->callback = cb;
    watch_entry->user_data = user_data;
    watch_entry->path_len = slen;
    SDL_memcpy(watch_entry->path, path, slen + 1);

    SDL_LockMutex(file_watch_lock);
    int wd = inotify_add_watch(inotify_fd, path, IN_MODIFY | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVED_TO | IN_MOVED_FROM | IN_MOVE_SELF);
    if (wd == -1) {
        SDL_UnlockMutex(file_watch_lock);
        SDL_free(watch_entry);
        return SDL_SetError("inotify_add_watch failed: %s", strerror(errno));
    }
    if (!SDL_InsertIntoHashTable(watch_descriptor_table, (void *) (intptr_t) wd, watch_entry, false)) {
        inotify_rm_watch(inotify_fd, wd);
        SDL_UnlockMutex(file_watch_lock);
        SDL_free(watch_entry);
        return false;
    }
    SDL_UnlockMutex(file_watch_lock);
    return true;
#else
    return SDL_Unsupported();
#endif // HAVE_INOTIFY
}

#ifdef HAVE_INOTIFY
static void SendFileWatchEvent(SDL_EventType event_type, const char *path) {
    if (SDL_EventEnabled(event_type)) {
        SDL_Event event;
        SDL_zero(event);
        event.type = event_type;
        event.common.timestamp = 0;
        if (path) {
            event.path_watch.path = SDL_CreateTemporaryString(path);
            if (!event.path_watch.path) {
                return;
            }
        }
        SDL_PushEvent(&event);
    }
}

static int SDL_FileWatchThread(void *userdata)
{
    while (SDL_GetAtomicInt(&quit_watch_file) == 0) {
        SDL_Delay(100);
        SDL_LockMutex(file_watch_lock);
        union
        {
            struct inotify_event event;
            char storage[4096];
            char enough_for_inotify[sizeof(struct inotify_event) + NAME_MAX + 1];
        } buf;
        ssize_t bytes;
        size_t remain = 0;
        size_t len;
        char path[PATH_MAX];

        bytes = read(inotify_fd, &buf, sizeof(buf));

        if (bytes > 0) {
            remain = (size_t)bytes;
        }

        while (remain > 0) {
            const WatchEntry *watch_entry;
            if (SDL_FindInHashTable(watch_descriptor_table, (void *) (intptr_t) buf.event.wd, (const void **) &watch_entry)) {
                if (buf.event.mask & IN_Q_OVERFLOW) {
                    SendFileWatchEvent(SDL_EVENT_PATH_WATCH_ERROR, NULL);
                } else if (buf.event.mask & IN_IGNORED) {
                    // removing a watch generates an IN_IGNORED event
                } else if (buf.event.mask & IN_UNMOUNT) {
                    // file system containing watched path was unmounted
                } else if (buf.event.len != 0) {
                    // event happened on a file or direcory in a watched directory
                    if (buf.event.mask & IN_ISDIR) {
                        // SDL functions that return a path always end it with a separator.
                        if (watch_entry->path[watch_entry->path_len - 1] == '/') {
                            (void)SDL_snprintf(path, SDL_arraysize(path), "%s%s/", watch_entry->path, buf.event.name);
                        } else {
                            (void)SDL_snprintf(path, SDL_arraysize(path), "%s/%s/", watch_entry->path, buf.event.name);
                        }
                    } else {
                        if (watch_entry->path[watch_entry->path_len - 1] == '/') {
                            (void)SDL_snprintf(path, SDL_arraysize(path), "%s%s", watch_entry->path, buf.event.name);
                        } else {
                            (void)SDL_snprintf(path, SDL_arraysize(path), "%s/%s", watch_entry->path, buf.event.name);
                        }
                    }

                    SDL_PathWatchEventType event_type = -1;
                    if (buf.event.mask & (IN_CREATE | IN_MOVED_TO)) {
                        // IN_MOVED_TO means a file or directory was renamed
                        event_type = SDL_PATHWATCH_CREATED;
                    } else if (buf.event.mask & (IN_DELETE | IN_MOVED_FROM)) {
                        // IN_MOVED_FROM means a file or directory was renamed
                        event_type = SDL_PATHWATCH_REMOVED;
                    } else if (buf.event.mask & IN_MODIFY) {
                        event_type = SDL_PATHWATCH_MODIFIED;
                    }

                    if (event_type != -1) {
                        if (watch_entry->callback) {
                            watch_entry->callback(watch_entry->user_data, path, event_type);
                        }
                        SendFileWatchEvent(event_type + SDL_EVENT_PATH_MODIFIED, path);
                    }
                } else {
                    // event happened for the watched file or directory
                    SDL_PathWatchEventType event_type = -1;
                    bool remove_entry = false;
                    if (buf.event.mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVED_FROM)) {
                        // IN_MOVE_SELF means a parent directory was renamed
                        event_type = SDL_PATHWATCH_REMOVED_SELF;
                        // inotify identifies file by inode, remove entry now to avoid receiving
                        // event for a different path than the one specified by SDL_AddPathWatch()
                        remove_entry = true;
                    } else if (buf.event.mask & IN_MODIFY) {
                        event_type = SDL_PATHWATCH_MODIFIED;
                    }

                    if (event_type != -1) {
                        if (watch_entry->callback) {
                            watch_entry->callback(watch_entry->user_data, watch_entry->path, event_type);
                        }
                        SendFileWatchEvent(event_type + SDL_EVENT_PATH_MODIFIED, watch_entry->path);
                    }
                    if (remove_entry) {
                        SDL_RemoveFromHashTable(watch_descriptor_table, (void *) (intptr_t) buf.event.wd);
                        inotify_rm_watch(inotify_fd, buf.event.wd);
                    }
                }
            }

            len = sizeof(struct inotify_event) + buf.event.len;
            remain -= len;

            if (remain != 0) {
                SDL_memmove(&buf.storage[0], &buf.storage[len], remain);
            }
        }
        SDL_UnlockMutex(file_watch_lock);
    }
    return 0;
}

typedef struct FindWatchEntryByValueData {
    WatchEntry *entry_to_find;
    const void *key_found;
} FindWatchEntryByValueData;

static bool SDLCALL FindWatchEntryByValue(void *userdata, const SDL_HashTable *table, const void *key, const void *value)
{
    FindWatchEntryByValueData *d = (FindWatchEntryByValueData *) userdata;
    const WatchEntry *iterator = (const WatchEntry *) value;
    if (iterator->path_len == d->entry_to_find->path_len
        && iterator->callback == d->entry_to_find->callback
        && iterator->user_data == d->entry_to_find->user_data
        && SDL_strcmp(iterator->path, d->entry_to_find->path) == 0) {
        d->key_found = key;
        return false;
    }
    return true;
}
#endif // HAVE_INOTIFY

void SDL_SYS_RemovePathWatch(const char *path, SDL_PathWatchCallback cb, void *user_data)
{
#ifdef HAVE_INOTIFY
    if (!watch_descriptor_table) {
        return;
    }
    const size_t slen = SDL_strlen(path);
    if (slen >= PATH_MAX) {
        return;
    }

    union LongestWatchEntry{
        WatchEntry entry;
        char enougn_for_longest_path[sizeof(WatchEntry) + PATH_MAX];
    } watch_entry;
    watch_entry.entry.callback = cb;
    watch_entry.entry.user_data = user_data;
    watch_entry.entry.path_len = slen;
    SDL_memcpy(watch_entry.entry.path, path, slen + 1);

    FindWatchEntryByValueData data = {&watch_entry.entry, (void *) (intptr_t) -1};
    SDL_LockMutex(file_watch_lock);
    if (!SDL_IterateHashTable(watch_descriptor_table, FindWatchEntryByValue, &data)) {
        SDL_UnlockMutex(file_watch_lock);
        return;
    }
    if (data.key_found != (void *) (intptr_t) -1) {
        SDL_RemoveFromHashTable(watch_descriptor_table, data.key_found);
        inotify_rm_watch(inotify_fd, (int) (intptr_t) data.key_found);
    }
    SDL_UnlockMutex(file_watch_lock);
#endif // HAVE_INOTIFY
}

void SDL_SYS_QuitPathWatch(void)
{
#ifdef HAVE_INOTIFY
    if (inotify_fd >= 0) {
        SDL_SetAtomicInt(&quit_watch_file, 1);
        int thread_status;
        SDL_WaitThread(file_watch_thread, &thread_status);
        SDL_assert(thread_status != -1);
        file_watch_thread = NULL;
        close(inotify_fd);
        inotify_fd = -1;
        SDL_DestroyMutex(file_watch_lock);
        file_watch_lock = NULL;
        SDL_DestroyHashTable(watch_descriptor_table);
        watch_descriptor_table = NULL;
    }
#endif // HAVE_INOTIFY
}

// Note that this is actually part of filesystem, not fsops, but everything that uses posix fsops uses this implementation, even with separate filesystem code.
char *SDL_SYS_GetCurrentDirectory(void)
{
    size_t buflen = 64;
    char *buf = NULL;

    while (true) {
        void *ptr = SDL_realloc(buf, buflen);
        if (!ptr) {
            SDL_free(buf);
            return NULL;
        }
        buf = (char *) ptr;

        if (getcwd(buf, buflen-1) != NULL) {
            break;  // we got it!
        }

        if (errno == ERANGE) {
            buflen *= 2;  // try again with a bigger buffer.
            continue;
        }

        SDL_free(buf);
        SDL_SetError("getcwd failed: %s", strerror(errno));
        return NULL;
    }

    // make sure there's a path separator at the end.
    SDL_assert(SDL_strlen(buf) < (buflen + 2));
    buflen = SDL_strlen(buf);
    if ((buflen == 0) || (buf[buflen-1] != '/')) {
        buf[buflen] = '/';
        buf[buflen + 1] = '\0';
    }

    return buf;
}

#endif // SDL_FSOPS_POSIX
