STEAM_PROC(void*, SteamAPI_SteamRemoteStorage_v016, (void))

STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_IsCloudEnabledForAccount, (void*))
STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_IsCloudEnabledForApp, (void*))

STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_BeginFileWriteBatch, (void*))
STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_EndFileWriteBatch, (void*))

STEAM_PROC(int32_t, SteamAPI_ISteamRemoteStorage_GetFileSize, (void*, const char*))
STEAM_PROC(int32_t, SteamAPI_ISteamRemoteStorage_FileRead, (void*, const char*, void*, int32_t))
STEAM_PROC(int32_t, SteamAPI_ISteamRemoteStorage_FileWrite, (void*, const char*, const void*, int32_t))
STEAM_PROC(bool, SteamAPI_ISteamRemoteStorage_GetQuota, (void*, uint64_t*, uint64_t*))

#undef STEAM_PROC
