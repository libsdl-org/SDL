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
 *  \file SDL_storage.h
 *
 *  Include file for storage container SDL API functions
 */

#ifndef SDL_storage_h_
#define SDL_storage_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_properties.h>

#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* !!! FIXME: Don't let this ship without async R/W support!!! */

typedef struct SDL_StorageInterface
{
    int (SDLCALL *close)(void *userdata);

    SDL_bool (SDLCALL *ready)(void *userdata);

    int (SDLCALL *fileSize)(void *userdata, const char *path, Uint64 *length);

    int (SDLCALL *readFile)(void *userdata, const char *path, void *destination, Uint64 length);

    int (SDLCALL *writeFile)(void *userdata, const char *path, const void *source, Uint64 length);

    Uint64 (SDLCALL *spaceRemaining)(void *userdata);
} SDL_StorageInterface;

typedef struct SDL_Storage SDL_Storage;

/**
 * Opens up a read-only container for the application's filesystem.
 *
 * \param override a path to override the backend's default title root
 * \param props a property list that may contain backend-specific information
 * \returns a title storage container on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenUserStorage
 * \sa SDL_OpenStorage
 * \sa SDL_TitleStorageReady
 * \sa SDL_CloseStorage
 * \sa SDL_StorageReady
 * \sa SDL_StorageFileSize
 * \sa SDL_StorageReadFile
 * \sa SDL_StorageWriteFile
 * \sa SDL_StorageSpaceRemaining
 */
extern DECLSPEC SDL_Storage *SDLCALL SDL_OpenTitleStorage(const char *override, SDL_PropertiesID props);

/**
 * Opens up a container for a user's unique read/write filesystem.
 *
 * While title storage can generally be kept open throughout runtime, user
 * storage should only be opened when the client is ready to read/write files.
 * This allows the backend to properly batch R/W operations and flush them
 * when the container has been closed; ensuring safe and optimal save I/O.
 *
 * \param org the name of your organization
 * \param app the name of your application
 * \param props a property list that may contain backend-specific information
 * \returns a user storage container on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenStorage
 * \sa SDL_CloseStorage
 * \sa SDL_StorageReady
 * \sa SDL_StorageFileSize
 * \sa SDL_StorageReadFile
 * \sa SDL_StorageWriteFile
 * \sa SDL_StorageSpaceRemaining
 */
extern DECLSPEC SDL_Storage *SDLCALL SDL_OpenUserStorage(const char *org, const char *app, SDL_PropertiesID props);

/**
 * Opens up a container using a client-provided storage interface.
 *
 * Applications do not need to use this function unless they are providing
 * their own SDL_Storage implementation. If you just need an SDL_Storage, you
 * should use the built-in implementations in SDL, like SDL_OpenTitleStorage()
 * or SDL_OpenUserStorage().
 *
 * \param iface the function table to be used by this container
 * \param userdata the pointer that will be passed to the store interface
 * \returns a storage container on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 * \sa SDL_CloseStorage
 * \sa SDL_StorageReady
 * \sa SDL_StorageFileSize
 * \sa SDL_StorageReadFile
 * \sa SDL_StorageWriteFile
 * \sa SDL_StorageSpaceRemaining
 */
extern DECLSPEC SDL_Storage *SDLCALL SDL_OpenStorage(const SDL_StorageInterface *iface, void *userdata);

/**
 * Closes and frees a storage container.
 *
 * \param storage a storage container to close
 * \returns 0 if the container was freed with no errors, a negative value
 *          otherwise; call SDL_GetError() for more information. Even if the
 *          function returns an error, the container data will be freed; the
 *          error is only for informational purposes.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 * \sa SDL_OpenStorage
 * \sa SDL_StorageReady
 * \sa SDL_StorageFileSize
 * \sa SDL_StorageReadFile
 * \sa SDL_StorageWriteFile
 * \sa SDL_StorageSpaceRemaining
 */
extern DECLSPEC int SDLCALL SDL_CloseStorage(SDL_Storage *storage);

/**
 * Checks if the storage container is ready to use.
 *
 * This function should be called in regular intervals until it returns
 * SDL_TRUE - however, it is not recommended to spinwait on this call, as the
 * backend may depend on a synchronous message loop.
 *
 * \param storage a storage container to query
 * \returns SDL_TRUE if the container is ready, SDL_FALSE otherwise
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 * \sa SDL_OpenStorage
 * \sa SDL_CloseStorage
 * \sa SDL_StorageFileSize
 * \sa SDL_StorageReadFile
 * \sa SDL_StorageWriteFile
 * \sa SDL_StorageSpaceRemaining
 */
extern DECLSPEC SDL_bool SDLCALL SDL_StorageReady(SDL_Storage *storage);

/**
 * Query the size of a file within a storage container.
 *
 * \param storage a storage container to query
 * \param path the relative path of the file to query
 * \param length a pointer to be filled with the file's length
 * \returns 0 if the file could be queried, a negative value otherwise; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 * \sa SDL_OpenStorage
 * \sa SDL_CloseStorage
 * \sa SDL_StorageReady
 * \sa SDL_StorageReadFile
 * \sa SDL_StorageWriteFile
 * \sa SDL_StorageSpaceRemaining
 */
extern DECLSPEC int SDLCALL SDL_StorageFileSize(SDL_Storage *storage, const char *path, Uint64 *length);

/**
 * Synchronously read a file from a storage container into a client-provided
 * buffer.
 *
 * \param storage a storage container to read from
 * \param path the relative path of the file to read
 * \param destination a client-provided buffer to read the file into
 * \param length the length of the destination buffer
 * \returns 0 if the file was read, a negative value otherwise; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 * \sa SDL_OpenStorage
 * \sa SDL_CloseStorage
 * \sa SDL_StorageReady
 * \sa SDL_StorageFileSize
 * \sa SDL_StorageWriteFile
 * \sa SDL_StorageSpaceRemaining
 */
extern DECLSPEC int SDLCALL SDL_StorageReadFile(SDL_Storage *storage, const char *path, void *destination, Uint64 length);

/**
 * Synchronously write a file from client memory into a storage container.
 *
 * \param storage a storage container to write to
 * \param path the relative path of the file to write
 * \param source a client-provided buffer to write from
 * \param length the length of the source buffer
 * \returns 0 if the file was written, a negative value otherwise; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 * \sa SDL_OpenStorage
 * \sa SDL_CloseStorage
 * \sa SDL_StorageReady
 * \sa SDL_StorageFileSize
 * \sa SDL_StorageReadFile
 * \sa SDL_StorageSpaceRemaining
 */
extern DECLSPEC int SDL_StorageWriteFile(SDL_Storage *storage, const char *path, const void *source, Uint64 length);

/**
 * Queries the remaining space in a storage container.
 *
 * \param storage a storage container to query
 * \returns the amount of remaining space, in bytes
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_OpenTitleStorage
 * \sa SDL_OpenUserStorage
 * \sa SDL_OpenStorage
 * \sa SDL_CloseStorage
 * \sa SDL_StorageReady
 * \sa SDL_StorageFileSize
 * \sa SDL_StorageReadFile
 * \sa SDL_StorageReadFileAsync
 * \sa SDL_StorageWriteFile
 * \sa SDL_StorageWriteFileAsync
 */
extern DECLSPEC Uint64 SDLCALL SDL_StorageSpaceRemaining(SDL_Storage *storage);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_storage_h_ */
