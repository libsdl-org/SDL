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

#include "SDL_hid.h"

HidD_GetAttributes_t SDL_HidD_GetAttributes;
HidD_GetString_t SDL_HidD_GetManufacturerString;
HidD_GetString_t SDL_HidD_GetProductString;
HidP_GetCaps_t SDL_HidP_GetCaps;
HidP_GetButtonCaps_t SDL_HidP_GetButtonCaps;
HidP_GetValueCaps_t SDL_HidP_GetValueCaps;
HidP_MaxDataListLength_t SDL_HidP_MaxDataListLength;
HidP_GetData_t SDL_HidP_GetData;

static HMODULE s_pHIDDLL = 0;
static int s_HIDDLLRefCount = 0;


bool WIN_LoadHIDDLL(void)
{
    if (s_pHIDDLL) {
        SDL_assert(s_HIDDLLRefCount > 0);
        s_HIDDLLRefCount++;
        return true; // already loaded
    }

    s_pHIDDLL = LoadLibrary(TEXT("hid.dll"));
    if (!s_pHIDDLL) {
        return false;
    }

    SDL_assert(s_HIDDLLRefCount == 0);
    s_HIDDLLRefCount = 1;

    SDL_HidD_GetAttributes = (HidD_GetAttributes_t)GetProcAddress(s_pHIDDLL, "HidD_GetAttributes");
    SDL_HidD_GetManufacturerString = (HidD_GetString_t)GetProcAddress(s_pHIDDLL, "HidD_GetManufacturerString");
    SDL_HidD_GetProductString = (HidD_GetString_t)GetProcAddress(s_pHIDDLL, "HidD_GetProductString");
    SDL_HidP_GetCaps = (HidP_GetCaps_t)GetProcAddress(s_pHIDDLL, "HidP_GetCaps");
    SDL_HidP_GetButtonCaps = (HidP_GetButtonCaps_t)GetProcAddress(s_pHIDDLL, "HidP_GetButtonCaps");
    SDL_HidP_GetValueCaps = (HidP_GetValueCaps_t)GetProcAddress(s_pHIDDLL, "HidP_GetValueCaps");
    SDL_HidP_MaxDataListLength = (HidP_MaxDataListLength_t)GetProcAddress(s_pHIDDLL, "HidP_MaxDataListLength");
    SDL_HidP_GetData = (HidP_GetData_t)GetProcAddress(s_pHIDDLL, "HidP_GetData");
    if (!SDL_HidD_GetManufacturerString || !SDL_HidD_GetProductString ||
        !SDL_HidP_GetCaps || !SDL_HidP_GetButtonCaps ||
        !SDL_HidP_GetValueCaps || !SDL_HidP_MaxDataListLength || !SDL_HidP_GetData) {
        WIN_UnloadHIDDLL();
        return false;
    }

    return true;
}

void WIN_UnloadHIDDLL(void)
{
    if (s_pHIDDLL) {
        SDL_assert(s_HIDDLLRefCount > 0);
        if (--s_HIDDLLRefCount == 0) {
            FreeLibrary(s_pHIDDLL);
            s_pHIDDLL = NULL;
        }
    } else {
        SDL_assert(s_HIDDLLRefCount == 0);
    }
}

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)

// CM_Register_Notification definitions

#define CR_SUCCESS 0

DECLARE_HANDLE(HCMNOTIFICATION);
typedef HCMNOTIFICATION *PHCMNOTIFICATION;

typedef enum _CM_NOTIFY_FILTER_TYPE
{
    CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE = 0,
    CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE,
    CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE,
    CM_NOTIFY_FILTER_TYPE_MAX
} CM_NOTIFY_FILTER_TYPE, *PCM_NOTIFY_FILTER_TYPE;

typedef struct _CM_NOTIFY_FILTER
{
    DWORD cbSize;
    DWORD Flags;
    CM_NOTIFY_FILTER_TYPE FilterType;
    DWORD Reserved;
    union
    {
        struct
        {
            GUID ClassGuid;
        } DeviceInterface;
        struct
        {
            HANDLE hTarget;
        } DeviceHandle;
        struct
        {
            WCHAR InstanceId[200];
        } DeviceInstance;
    } u;
} CM_NOTIFY_FILTER, *PCM_NOTIFY_FILTER;

typedef enum _CM_NOTIFY_ACTION
{
    CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL = 0,
    CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL,
    CM_NOTIFY_ACTION_DEVICEQUERYREMOVE,
    CM_NOTIFY_ACTION_DEVICEQUERYREMOVEFAILED,
    CM_NOTIFY_ACTION_DEVICEREMOVEPENDING,
    CM_NOTIFY_ACTION_DEVICEREMOVECOMPLETE,
    CM_NOTIFY_ACTION_DEVICECUSTOMEVENT,
    CM_NOTIFY_ACTION_DEVICEINSTANCEENUMERATED,
    CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED,
    CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED,
    CM_NOTIFY_ACTION_MAX
} CM_NOTIFY_ACTION, *PCM_NOTIFY_ACTION;

typedef struct _CM_NOTIFY_EVENT_DATA
{
    CM_NOTIFY_FILTER_TYPE FilterType;
    DWORD Reserved;
    union
    {
        struct
        {
            GUID ClassGuid;
            WCHAR SymbolicLink[ANYSIZE_ARRAY];
        } DeviceInterface;
        struct
        {
            GUID EventGuid;
            LONG NameOffset;
            DWORD DataSize;
            BYTE Data[ANYSIZE_ARRAY];
        } DeviceHandle;
        struct
        {
            WCHAR InstanceId[ANYSIZE_ARRAY];
        } DeviceInstance;
    } u;
} CM_NOTIFY_EVENT_DATA, *PCM_NOTIFY_EVENT_DATA;

typedef DWORD (CALLBACK *PCM_NOTIFY_CALLBACK)(HCMNOTIFICATION hNotify, PVOID Context, CM_NOTIFY_ACTION Action, PCM_NOTIFY_EVENT_DATA EventData, DWORD EventDataSize);

typedef DWORD (WINAPI *CM_Register_NotificationFunc)(PCM_NOTIFY_FILTER pFilter, PVOID pContext, PCM_NOTIFY_CALLBACK pCallback, PHCMNOTIFICATION pNotifyContext);
typedef DWORD (WINAPI *CM_Unregister_NotificationFunc)(HCMNOTIFICATION NotifyContext);

// Device property definitions, for looking up container IDs

typedef struct
{
    GUID fmtid;
    ULONG pid;
} SDL_DEVPROPKEY;

#define SDL_DEVPROP_TYPE_GUID   0x0000000D
#define SDL_DEVPROP_TYPE_STRING 0x00000012

static const SDL_DEVPROPKEY SDL_DEVPKEY_Device_InstanceId = { { 0x78c34fc8, 0x104a, 0x4aca, { 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57 } }, 256 };
static const SDL_DEVPROPKEY SDL_DEVPKEY_Device_ContainerId = { { 0x8c7ed206, 0x3f8a, 0x4827, { 0xb3, 0xab, 0xae, 0x9e, 0x1f, 0xae, 0xfc, 0x6c } }, 2 };

typedef DWORD (WINAPI *CM_Get_Device_Interface_PropertyWFunc)(LPCWSTR pszDeviceInterface, const SDL_DEVPROPKEY *PropertyKey, ULONG *PropertyType, PBYTE PropertyBuffer, PULONG PropertyBufferSize, ULONG ulFlags);
typedef DWORD (WINAPI *CM_Locate_DevNodeWFunc)(DWORD *pdnDevInst, WCHAR *pDeviceID, ULONG ulFlags);
typedef DWORD (WINAPI *CM_Get_DevNode_PropertyWFunc)(DWORD dnDevInst, const SDL_DEVPROPKEY *PropertyKey, ULONG *PropertyType, PBYTE PropertyBuffer, PULONG PropertyBufferSize, ULONG ulFlags);

#define CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES 0x00000001

static GUID GUID_DEVINTERFACE_HID = { 0x4D1E55B2L, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

static SDL_InitState SDL_cfgmgr32_init;
static CM_Register_NotificationFunc SDL_CM_Register_Notification;
static CM_Unregister_NotificationFunc SDL_CM_Unregister_Notification;
static CM_Get_Device_Interface_PropertyWFunc SDL_CM_Get_Device_Interface_PropertyW;
static CM_Locate_DevNodeWFunc SDL_CM_Locate_DevNodeW;
static CM_Get_DevNode_PropertyWFunc SDL_CM_Get_DevNode_PropertyW;

static int s_DeviceNotificationsRequested;
static HCMNOTIFICATION s_DeviceNotificationFuncHandle;
static Uint64 s_LastDeviceNotification = 1;
static HANDLE s_HotplugEvent = INVALID_HANDLE_VALUE;
static SDL_AtomicInt s_HotplugRunning;
static SDL_Thread *s_HotplugThread;

static void WIN_LoadCfgMgr32(void)
{
    if (SDL_ShouldInit(&SDL_cfgmgr32_init)) {
        // We'll keep this loaded for future use
        HMODULE cfgmgr32 = LoadLibraryA("cfgmgr32.dll");
        if (cfgmgr32) {
            SDL_CM_Register_Notification = (CM_Register_NotificationFunc)GetProcAddress(cfgmgr32, "CM_Register_Notification");
            SDL_CM_Unregister_Notification = (CM_Unregister_NotificationFunc)GetProcAddress(cfgmgr32, "CM_Unregister_Notification");
            SDL_CM_Get_Device_Interface_PropertyW = (CM_Get_Device_Interface_PropertyWFunc)GetProcAddress(cfgmgr32, "CM_Get_Device_Interface_PropertyW");
            SDL_CM_Locate_DevNodeW = (CM_Locate_DevNodeWFunc)GetProcAddress(cfgmgr32, "CM_Locate_DevNodeW");
            SDL_CM_Get_DevNode_PropertyW = (CM_Get_DevNode_PropertyWFunc)GetProcAddress(cfgmgr32, "CM_Get_DevNode_PropertyW");
        }
        SDL_SetInitialized(&SDL_cfgmgr32_init, true);
    }
}

bool WIN_GetDeviceInterfaceContainerID(const char *path, SDL_GUID *container_id)
{
    WCHAR *interface_path = NULL;
    WCHAR *instance_id = NULL;
    ULONG property_type;
    ULONG size = 0;
    DWORD devnode;
    GUID guid;
    bool result = false;

    SDL_zerop(container_id);

    WIN_LoadCfgMgr32();
    if (!SDL_CM_Get_Device_Interface_PropertyW || !SDL_CM_Locate_DevNodeW || !SDL_CM_Get_DevNode_PropertyW || !path) {
        return false;
    }

    interface_path = WIN_UTF8ToStringW(path);
    if (!interface_path) {
        goto done;
    }

    // Find the device instance from the interface path
    SDL_CM_Get_Device_Interface_PropertyW(interface_path, &SDL_DEVPKEY_Device_InstanceId, &property_type, NULL, &size, 0);
    if (size == 0) {
        goto done;
    }
    instance_id = (WCHAR *)SDL_malloc(size);
    if (!instance_id) {
        goto done;
    }
    if (SDL_CM_Get_Device_Interface_PropertyW(interface_path, &SDL_DEVPKEY_Device_InstanceId, &property_type, (PBYTE)instance_id, &size, 0) != CR_SUCCESS ||
        property_type != SDL_DEVPROP_TYPE_STRING) {
        goto done;
    }
    if (SDL_CM_Locate_DevNodeW(&devnode, instance_id, 0) != CR_SUCCESS) {
        goto done;
    }

    // Get the container ID from the device instance
    size = sizeof(guid);
    if (SDL_CM_Get_DevNode_PropertyW(devnode, &SDL_DEVPKEY_Device_ContainerId, &property_type, (PBYTE)&guid, &size, 0) != CR_SUCCESS ||
        property_type != SDL_DEVPROP_TYPE_GUID) {
        goto done;
    }
    SDL_memcpy(container_id->data, &guid, sizeof(container_id->data));
    result = true;

done:
    SDL_free(interface_path);
    SDL_free(instance_id);
    return result;
}

#ifdef SDL_VIDEO_DRIVER_WINDOWS
// Defined in SDL_windowsevents.c
extern void WIN_CheckKeyboardAndMouseHotplug(bool hid_loaded);
#endif

static int SDLCALL DeviceHotplugThread(void *unused)
{
    bool hid_loaded = WIN_LoadHIDDLL();

    // Always run the initial device detection
    do {
#ifdef SDL_VIDEO_DRIVER_WINDOWS
        WIN_CheckKeyboardAndMouseHotplug(hid_loaded);
#endif
        WaitForSingleObject(s_HotplugEvent, INFINITE);
    } while (SDL_GetAtomicInt(&s_HotplugRunning));

    if (hid_loaded) {
        WIN_UnloadHIDDLL();
    }
    return 0;
}

static DWORD CALLBACK SDL_DeviceNotificationFunc(HCMNOTIFICATION hNotify, PVOID context, CM_NOTIFY_ACTION action, PCM_NOTIFY_EVENT_DATA eventData, DWORD event_data_size)
{
    if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL ||
        action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) {
        s_LastDeviceNotification = SDL_GetTicksNS();
        SetEvent(s_HotplugEvent);
    }
    return ERROR_SUCCESS;
}

void WIN_InitDeviceNotification(void)
{
    ++s_DeviceNotificationsRequested;
    if (s_DeviceNotificationsRequested > 1) {
        return;
    }

    // Start the device hotplug thread
    SDL_SetAtomicInt(&s_HotplugRunning, true);
    s_HotplugEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_HotplugThread = SDL_CreateThread(DeviceHotplugThread, "DeviceHotplugThread", NULL);

    WIN_LoadCfgMgr32();
    if (SDL_CM_Register_Notification && SDL_CM_Unregister_Notification) {
        CM_NOTIFY_FILTER notify_filter;

        SDL_zero(notify_filter);
        notify_filter.cbSize = sizeof(notify_filter);
        notify_filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        notify_filter.u.DeviceInterface.ClassGuid = GUID_DEVINTERFACE_HID;
        if (SDL_CM_Register_Notification(&notify_filter, NULL, SDL_DeviceNotificationFunc, &s_DeviceNotificationFuncHandle) == CR_SUCCESS) {
            return;
        }
    }

    // FIXME: Should we log errors?
}

Uint64 WIN_GetLastDeviceNotification(void)
{
    return s_LastDeviceNotification;
}

struct WIN_DeviceInterfaceNotification
{
    HCMNOTIFICATION handle;
    WIN_DeviceInterfaceCallback callback;
    void *userdata;
};

static DWORD CALLBACK WIN_DeviceInterfaceNotificationFunc(HCMNOTIFICATION hNotify, PVOID context, CM_NOTIFY_ACTION action, PCM_NOTIFY_EVENT_DATA eventData, DWORD event_data_size)
{
    WIN_DeviceInterfaceNotification *notification = (WIN_DeviceInterfaceNotification *)context;

    if (action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL ||
        action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) {
        notification->callback(notification->userdata);
    }
    return ERROR_SUCCESS;
}

WIN_DeviceInterfaceNotification *WIN_RegisterDeviceInterfaceNotification(WIN_DeviceInterfaceCallback callback, void *userdata)
{
    WIN_DeviceInterfaceNotification *notification;
    CM_NOTIFY_FILTER notify_filter;

    WIN_LoadCfgMgr32();
    if (!SDL_CM_Register_Notification || !SDL_CM_Unregister_Notification) {
        return NULL;
    }

    notification = (WIN_DeviceInterfaceNotification *)SDL_calloc(1, sizeof(*notification));
    if (!notification) {
        return NULL;
    }
    notification->callback = callback;
    notification->userdata = userdata;

    SDL_zero(notify_filter);
    notify_filter.cbSize = sizeof(notify_filter);
    notify_filter.Flags = CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;
    notify_filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    if (SDL_CM_Register_Notification(&notify_filter, notification, WIN_DeviceInterfaceNotificationFunc, &notification->handle) != CR_SUCCESS) {
        SDL_free(notification);
        return NULL;
    }
    return notification;
}

void WIN_UnregisterDeviceInterfaceNotification(WIN_DeviceInterfaceNotification *notification)
{
    if (notification) {
        SDL_CM_Unregister_Notification(notification->handle);
        SDL_free(notification);
    }
}

void WIN_QuitDeviceNotification(void)
{
    if (--s_DeviceNotificationsRequested > 0) {
        return;
    }
    // Make sure we have balanced calls to init/quit
    SDL_assert(s_DeviceNotificationsRequested == 0);

    // Stop the device hotplug thread
    SDL_SetAtomicInt(&s_HotplugRunning, false);
    SetEvent(s_HotplugEvent);
    SDL_WaitThread(s_HotplugThread, NULL);
    s_HotplugThread = NULL;

    if (s_DeviceNotificationFuncHandle) {
        SDL_CM_Unregister_Notification(s_DeviceNotificationFuncHandle);
        s_DeviceNotificationFuncHandle = NULL;
    }
}

#else

void WIN_InitDeviceNotification(void)
{
}

Uint64 WIN_GetLastDeviceNotification( void )
{
    return 0;
}

void WIN_QuitDeviceNotification(void)
{
}

WIN_DeviceInterfaceNotification *WIN_RegisterDeviceInterfaceNotification(WIN_DeviceInterfaceCallback callback, void *userdata)
{
    return NULL;
}

void WIN_UnregisterDeviceInterfaceNotification(WIN_DeviceInterfaceNotification *notification)
{
}

bool WIN_GetDeviceInterfaceContainerID(const char *path, SDL_GUID *container_id)
{
    SDL_zerop(container_id);
    return false;
}

#endif // !SDL_PLATFORM_XBOXONE && !SDL_PLATFORM_XBOXSERIES
