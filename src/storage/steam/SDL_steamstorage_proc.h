STEAM_PROC(void*, SteamAPI_SteamRemoteStorage_v016, (void))

STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount, (void*))
STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp, (void*))

STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch, (void*))
STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_EndFileWriteBatch, (void*))

STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_FileExists, (void*, const char*))
STEAM_PROC(Sint32, SteamAPI_ISteamRemoteStorage_GetFileSize, (void*, const char*))
STEAM_PROC(Sint64, SteamAPI_ISteamRemoteStorage_GetFileTimestamp, (void*, const char *))
STEAM_PROC(Sint32, SteamAPI_ISteamRemoteStorage_FileRead, (void*, const char*, void*, Sint32))
STEAM_PROC(Sint32, SteamAPI_ISteamRemoteStorage_FileWrite, (void*, const char*, const void*, Sint32))
STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_FileDelete, (void*, const char*))
STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_GetQuota, (void*, Uint64*, Uint64*))

STEAM_PROC(Sint32, SteamAPI_ISteamRemoteStorage_GetFileCount, (void*))
STEAM_PROC(const char *, SteamAPI_ISteamRemoteStorage_GetFileNameAndSize, (void*, int, Sint32*))

#undef STEAM_PROC
