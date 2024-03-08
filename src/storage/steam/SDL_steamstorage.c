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

#include "../SDL_sysstorage.h"

#include <stdbool.h> /* Needed by Steamworks */

// !!! FIXME: Async API can use SteamRemoteStorage_ReadFileAsync
// !!! FIXME: Async API can use SteamRemoteStorage_WriteFileAsync

#define STEAM_PROC(ret, func, parms) \
    typedef ret (*steamfntype_##func) parms;
#include "SDL_steamstorage_proc.h"

typedef struct STEAM_RemoteStorage
{
    void *libsteam_api;
    #define STEAM_PROC(ret, func, parms) \
        steamfntype_##func func;
    #include "SDL_steamstorage_proc.h"
} STEAM_RemoteStorage;

static int STEAM_UserStorageClose(void *userdata)
{
    int result = 0;
    STEAM_RemoteStorage *steam = (STEAM_RemoteStorage*) userdata;
    void *steamremotestorage = steam->SteamAPI_SteamRemoteStorage_v016();
    if (steamremotestorage == NULL) {
        result = SDL_SetError("SteamRemoteStorage unavailable");
    } else if (!steam->SteamAPI_ISteamRemoteStorage_EndFileWriteBatch(steamremotestorage)) {
        result = SDL_SetError("SteamRemoteStorage()->EndFileWriteBatch() failed");
    }
    SDL_UnloadObject(steam->libsteam_api);
    SDL_free(steam);
    return result;
}

static SDL_bool STEAM_UserStorageReady(void *userdata)
{
    return SDL_TRUE;
}

static int STEAM_UserStorageFileSize(void *userdata, const char *path, Uint64 *length)
{
    STEAM_RemoteStorage *steam = (STEAM_RemoteStorage*) userdata;
    void *steamremotestorage = steam->SteamAPI_SteamRemoteStorage_v016();
    if (steamremotestorage == NULL) {
        return SDL_SetError("SteamRemoteStorage unavailable");
    }
    *length = steam->SteamAPI_ISteamRemoteStorage_GetFileSize(steamremotestorage, path);
    return 0;
}

static int STEAM_UserStorageReadFile(void *userdata, const char *path, void *destination, Uint64 length)
{
    int retval;
    STEAM_RemoteStorage *steam = (STEAM_RemoteStorage*) userdata;
    void *steamremotestorage = steam->SteamAPI_SteamRemoteStorage_v016();
    if (steamremotestorage == NULL) {
        return SDL_SetError("SteamRemoteStorage unavailable");
    }
    if (length > SDL_MAX_SINT32) {
        return SDL_SetError("SteamRemoteStorage only supports INT32_MAX read size");
    }
    retval = steam->SteamAPI_ISteamRemoteStorage_FileRead(steamremotestorage, path, destination, (Sint32) length) == length;
    return retval - 1;
}

static int STEAM_UserStorageWriteFile(void *userdata, const char *path, const void *source, Uint64 length)
{
    int retval;
    STEAM_RemoteStorage *steam = (STEAM_RemoteStorage*) userdata;
    void *steamremotestorage = steam->SteamAPI_SteamRemoteStorage_v016();
    if (steamremotestorage == NULL) {
        return SDL_SetError("SteamRemoteStorage unavailable");
    }
    if (length > SDL_MAX_SINT32) {
        return SDL_SetError("SteamRemoteStorage only supports INT32_MAX write size");
    }
    retval = steam->SteamAPI_ISteamRemoteStorage_FileWrite(steamremotestorage, path, source, (Sint32) length) == length;
    return retval - 1;
}

static Uint64 STEAM_UserStorageSpaceRemaining(void *userdata)
{
    Uint64 total, remaining;
    STEAM_RemoteStorage *steam = (STEAM_RemoteStorage*) userdata;
    void *steamremotestorage = steam->SteamAPI_SteamRemoteStorage_v016();
    if (steamremotestorage == NULL) {
        SDL_SetError("SteamRemoteStorage unavailable");
        return 0;
    }
    if (!steam->SteamAPI_ISteamRemoteStorage_GetQuota(steamremotestorage, &total, &remaining)) {
        SDL_SetError("SteamRemoteStorage()->GetQuota failed");
        return 0;
    }
    return remaining;
}

static const SDL_StorageInterface STEAM_user_iface = {
    STEAM_UserStorageClose,
    STEAM_UserStorageReady,
    STEAM_UserStorageFileSize,
    STEAM_UserStorageReadFile,
    STEAM_UserStorageWriteFile,
    STEAM_UserStorageSpaceRemaining
};

static SDL_Storage *STEAM_User_Create(const char *org, const char *app, SDL_PropertiesID props)
{
    SDL_Storage *result;
    STEAM_RemoteStorage *steam;
    void *steamremotestorage;

    steam = (STEAM_RemoteStorage*) SDL_malloc(sizeof(STEAM_RemoteStorage));
    if (steam == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    steam->libsteam_api = SDL_LoadObject(
#if defined(_WIN64)
        "steam_api64.dll"
#elif defined(_WIN32)
        "steam_api.dll"
#elif defined(__APPLE__)
        "libsteam_api.dylib"
#else
        "libsteam_api.so"
#endif
    );
    if (steam->libsteam_api == NULL) {
        SDL_free(steam);
        return NULL;
    }

    #define STEAM_PROC(ret, func, parms) \
        steam->func = (steamfntype_##func) SDL_LoadFunction(steam->libsteam_api, #func); \
        if (steam->func == NULL) { \
            SDL_SetError("Could not load function " #func); \
            goto steamfail; \
        }
    #include "SDL_steamstorage_proc.h"

    steamremotestorage = steam->SteamAPI_SteamRemoteStorage_v016();
    if (steamremotestorage == NULL) {
        SDL_SetError("SteamRemoteStorage unavailable");
        goto steamfail;
    }
    if (!steam->SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount(steamremotestorage)) {
        SDL_SetError("Steam cloud is disabled for this user");
        goto steamfail;
    }
    if (!steam->SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp(steamremotestorage)) {
        SDL_SetError("Steam cloud is disabled for this application");
        goto steamfail;
    }
    if (!steam->SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch(steamremotestorage)) {
        SDL_SetError("SteamRemoteStorage()->BeginFileWriteBatch failed");
        goto steamfail;
    }

    result = SDL_OpenStorage(&STEAM_user_iface, steam);
    if (result == NULL) {
        goto steamfail;
    }
    return result;

steamfail:
    SDL_UnloadObject(steam->libsteam_api);
    SDL_free(steam);
    return NULL;
}

UserStorageBootStrap STEAM_userbootstrap = {
    "steam",
    "SDL Steam user storage driver",
    STEAM_User_Create
};
