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

/**
 * # CategoryStorage
 *
 * SDL storage container management.
 */

#ifndef SDL_storage_h_
#define SDL_storage_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_properties.h>

#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* !!! FIXME: Don't let this ship without async R/W support!!! */

/**
 * Function interface for SDL_Storage.
 *
 * Apps that want to supply a custom implementation of SDL_Storage will fill
 * in all the functions in this struct, and then pass it to SDL_OpenStorage to
 * create a custom SDL_Storage object.
 *
 * It is not usually necessary to do this; SDL provides standard
 * implementations for many things you might expect to do with an SDL_Storage.
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_StorageInterface
{
    /* Called when the storage is closed */
    int (SDLCALL *close)(void *userdata);

    /* Optional, returns whether the storage is currently ready for access */
    SDL_bool (SDLCALL *ready)(void *userdata);

    /* Enumerate a directory, optional for write-only storage */
    int (SDLCALL *enumerate)(void *userdata, const char *path, SDL_EnumerateDirectoryCallback callback, void *callback_userdata);

    /* Get path information, optional for write-only storage */
    int (SDLCALL *info)(void *userdata, const char *path, SDL_PathInfo *info);

    /* Read a file from storage, optional for write-only storage */
    int (SDLCALL *read_file)(void *userdata, const char *path, void *destination, Uint64 length);

    /* Write a file to storage, optional for read-only storage */
    int (SDLCALL *write_file)(void *userdata, const char *path, const void *source, Uint64 length);

    /* Create a directory, optional for read-only storage */
    int (SDLCALL *mkdir)(void *userdata, const char *path);

    /* Remove a file or empty directory, optional for read-only storage */
    int (SDLCALL *remove)(void *userdata, const char *path);

    /* Rename a path, optional for read-only storage */
    int (SDLCALL *rename)(void *userdata, const char *oldpath, const char *newpath);

    /* Copy a file, optional for read-only storage */
    int (SDLCALL *copy)(void *userdata, const char *oldpath, const char *newpath);

    /* Get the space remaining, optional for read-only storage */
    Uint64 (SDLCALL *space_remaining)(void *userdata);
} SDL_StorageInterface;

/**
 * An abstract interface for filesystem access.
 *
 * This is an opaque datatype. One can create this object using standard SDL
 * functions like SDL_OpenTitleStorage or SDL_OpenUserStorage, etc, or create
 * an object with a custom implementation using SDL_OpenStorage.
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_Storage SDL_Storage;

/**
 * Opens up a read-only container for the application's filesystem.
 *
 * \param override a path to override the backend's default title root.
 * \param props a property list that may contain backend-specific information.
 * \returns a title storage container on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CloseStorage
 * \sa SDL_GetStorageFileSize
 * \sa SDL_OpenUserStorage
 * \sa SDL_ReadStorageFile
 */
extern SDL_DECLSPEC SDL_Storage * SDLCALL SDL_OpenTitleStorage(const char *override, SDL_PropertiesID props);

/**
 * Opens up a container for a user's unique read/write filesystem.
 *
 * While title storage can generally be kept open throughout runtime, user
 * storage should only be opened when the client is ready to read/write files.
 * This allows the backend to properly batch file operations and flush them
 * when the container has been closed; ensuring safe and optimal save I/O.
 *
 * \param org the name of your organization.
 * \param app the name of your application.
 * \param props a property list that may contain backend-specific information.
 * \returns a user storage container on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CloseStorage
 * \sa SDL_GetStorageFileSize
 * \sa SDL_GetStorageSpaceRemaining
 * \sa SDL_OpenTitleStorage
 * \sa SDL_ReadStorageFile
 * \sa SDL_StorageReady
 * \sa SDL_WriteStorageFile
 */
extern SDL_DECLSPEC SDL_Storage * SDLCALL SDL_OpenUserStorage(const char *org, const char *app, SDL_PropertiesID props);

/**
 * Opens up a container for local filesystem storage.
 *
 * This is provided for development and tools. Portable applications should
 * use SDL_OpenTitleStorage() for access to game data and
 * SDL_OpenUserStorage() for access to user data.
 *
 * \param path the base path prepended to all storage paths, or NULL for no
 *             base path.
 * \returns a filesystem storage container on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CloseStorage
 * \sa SDL_GetStorageFileSize
 * \sa SDL_GetStorageSpaceRemaining
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 * \sa SDL_ReadStorageFile
 * \sa SDL_WriteStorageFile
 */
extern SDL_DECLSPEC SDL_Storage * SDLCALL SDL_OpenFileStorage(const char *path);

/**
 * Opens up a container using a client-provided storage interface.
 *
 * Applications do not need to use this function unless they are providing
 * their own SDL_Storage implementation. If you just need an SDL_Storage, you
 * should use the built-in implementations in SDL, like SDL_OpenTitleStorage()
 * or SDL_OpenUserStorage().
 *
 * \param iface the function table to be used by this container.
 * \param userdata the pointer that will be passed to the store interface.
 * \returns a storage container on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CloseStorage
 * \sa SDL_GetStorageFileSize
 * \sa SDL_GetStorageSpaceRemaining
 * \sa SDL_ReadStorageFile
 * \sa SDL_StorageReady
 * \sa SDL_WriteStorageFile
 */
extern SDL_DECLSPEC SDL_Storage * SDLCALL SDL_OpenStorage(const SDL_StorageInterface *iface, void *userdata);

/**
 * Closes and frees a storage container.
 *
 * \param storage a storage container to close.
 * \returns 0 if the container was freed with no errors, a negative value
 *          otherwise; call SDL_GetError() for more information. Even if the
 *          function returns an error, the container data will be freed; the
 *          error is only for informational purposes.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenFileStorage
 * \sa SDL_OpenStorage
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 */
extern SDL_DECLSPEC int SDLCALL SDL_CloseStorage(SDL_Storage *storage);

/**
 * Checks if the storage container is ready to use.
 *
 * This function should be called in regular intervals until it returns
 * SDL_TRUE - however, it is not recommended to spinwait on this call, as the
 * backend may depend on a synchronous message loop.
 *
 * \param storage a storage container to query.
 * \returns SDL_TRUE if the container is ready, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_StorageReady(SDL_Storage *storage);

/**
 * Query the size of a file within a storage container.
 *
 * \param storage a storage container to query.
 * \param path the relative path of the file to query.
 * \param length a pointer to be filled with the file's length.
 * \returns 0 if the file could be queried or a negative error code on
 *          failure; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ReadStorageFile
 * \sa SDL_StorageReady
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetStorageFileSize(SDL_Storage *storage, const char *path, Uint64 *length);

/**
 * Synchronously read a file from a storage container into a client-provided
 * buffer.
 *
 * \param storage a storage container to read from.
 * \param path the relative path of the file to read.
 * \param destination a client-provided buffer to read the file into.
 * \param length the length of the destination buffer.
 * \returns 0 if the file was read or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetStorageFileSize
 * \sa SDL_StorageReady
 * \sa SDL_WriteStorageFile
 */
extern SDL_DECLSPEC int SDLCALL SDL_ReadStorageFile(SDL_Storage *storage, const char *path, void *destination, Uint64 length);

/**
 * Synchronously write a file from client memory into a storage container.
 *
 * \param storage a storage container to write to.
 * \param path the relative path of the file to write.
 * \param source a client-provided buffer to write from.
 * \param length the length of the source buffer.
 * \returns 0 if the file was written or a negative error code on failure;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetStorageSpaceRemaining
 * \sa SDL_ReadStorageFile
 * \sa SDL_StorageReady
 */
extern SDL_DECLSPEC int SDLCALL SDL_WriteStorageFile(SDL_Storage *storage, const char *path, const void *source, Uint64 length);

/**
 * Create a directory in a writable storage container.
 *
 * \param storage a storage container.
 * \param path the path of the directory to create.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StorageReady
 */
extern SDL_DECLSPEC int SDLCALL SDL_CreateStorageDirectory(SDL_Storage *storage, const char *path);

/**
 * Enumerate a directory in a storage container through a callback function.
 *
 * This function provides every directory entry through an app-provided
 * callback, called once for each directory entry, until all results have been
 * provided or the callback returns <= 0.
 *
 * \param storage a storage container.
 * \param path the path of the directory to enumerate.
 * \param callback a function that is called for each entry in the directory.
 * \param userdata a pointer that is passed to `callback`.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StorageReady
 */
extern SDL_DECLSPEC int SDLCALL SDL_EnumerateStorageDirectory(SDL_Storage *storage, const char *path, SDL_EnumerateDirectoryCallback callback, void *userdata);

/**
 * Remove a file or an empty directory in a writable storage container.
 *
 * \param storage a storage container.
 * \param path the path of the directory to enumerate.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StorageReady
 */
extern SDL_DECLSPEC int SDLCALL SDL_RemoveStoragePath(SDL_Storage *storage, const char *path);

/**
 * Rename a file or directory in a writable storage container.
 *
 * \param storage a storage container.
 * \param oldpath the old path.
 * \param newpath the new path.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StorageReady
 */
extern SDL_DECLSPEC int SDLCALL SDL_RenameStoragePath(SDL_Storage *storage, const char *oldpath, const char *newpath);

/**
 * Copy a file in a writable storage container.
 *
 * \param storage a storage container.
 * \param oldpath the old path.
 * \param newpath the new path.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StorageReady
 */
extern SDL_DECLSPEC int SDLCALL SDL_CopyStorageFile(SDL_Storage *storage, const char *oldpath, const char *newpath);

/**
 * Get information about a filesystem path in a storage container.
 *
 * \param storage a storage container.
 * \param path the path to query.
 * \param info a pointer filled in with information about the path, or NULL to
 *             check for the existence of a file.
 * \returns 0 on success or a negative error code if the file doesn't exist,
 *          or another failure; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StorageReady
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetStoragePathInfo(SDL_Storage *storage, const char *path, SDL_PathInfo *info);

/**
 * Queries the remaining space in a storage container.
 *
 * \param storage a storage container to query.
 * \returns the amount of remaining space, in bytes.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_StorageReady
 * \sa SDL_WriteStorageFile
 */
extern SDL_DECLSPEC Uint64 SDLCALL SDL_GetStorageSpaceRemaining(SDL_Storage *storage);

/**
 * Enumerate a directory tree, filtered by pattern, and return a list.
 *
 * Files are filtered out if they don't match the string in `pattern`, which
 * may contain wildcard characters '*' (match everything) and '?' (match one
 * character). If pattern is NULL, no filtering is done and all results are
 * returned. Subdirectories are permitted, and are specified with a path
 * separator of '/'. Wildcard characters '*' and '?' never match a path
 * separator.
 *
 * `flags` may be set to SDL_GLOB_CASEINSENSITIVE to make the pattern matching
 * case-insensitive.
 *
 * The returned array is always NULL-terminated, for your iterating
 * convenience, but if `count` is non-NULL, on return it will contain the
 * number of items in the array, not counting the NULL terminator.
 *
 * \param storage a storage container.
 * \param path the path of the directory to enumerate.
 * \param pattern the pattern that files in the directory must match. Can be
 *                NULL.
 * \param flags `SDL_GLOB_*` bitflags that affect this search.
 * \param count on return, will be set to the number of items in the returned
 *              array. Can be NULL.
 * \returns an array of strings on success or NULL on failure; call
 *          SDL_GetError() for more information. The caller should pass the
 *          returned pointer to SDL_free when done with it. This is a single
 *          allocation that should be freed with SDL_free() when it is no
 *          longer needed.
 *
 * \threadsafety It is safe to call this function from any thread, assuming
 *               the `storage` object is thread-safe.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC char ** SDLCALL SDL_GlobStorageDirectory(SDL_Storage *storage, const char *path, const char *pattern, SDL_GlobFlags flags, int *count);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_storage_h_ */
