/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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
 *  \file SDL_system.h
 *
 *  Include file for platform specific SDL API functions
 */

#ifndef SDL_system_h_
#define SDL_system_h_

#include "SDL_stdinc.h"
#include "SDL_keyboard.h"
#include "SDL_render.h"
#include "SDL_video.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif


/* Platform specific functions for Windows */
#ifdef __WIN32__
	
/**
   \brief Set a function that is called for every windows message, before TranslateMessage()
*/
typedef void (SDLCALL * SDL_WindowsMessageHook)(void *userdata, void *hWnd, unsigned int message, Uint64 wParam, Sint64 lParam);
extern DECLSPEC void SDLCALL SDL_SetWindowsMessageHook(SDL_WindowsMessageHook callback, void *userdata);

/**
   \brief Returns the D3D9 adapter index that matches the specified display index.

   This adapter index can be passed to IDirect3D9::CreateDevice and controls
   on which monitor a full screen application will appear.
*/
extern DECLSPEC int SDLCALL SDL_Direct3D9GetAdapterIndex( int displayIndex );

typedef struct IDirect3DDevice9 IDirect3DDevice9;
/**
   \brief Returns the D3D9 device associated with a renderer, or NULL if it's not a D3D9 renderer.

   Once you are done using the device, you should release it to avoid a resource leak.
 */
extern DECLSPEC IDirect3DDevice9* SDLCALL SDL_RenderGetD3D9Device(SDL_Renderer * renderer);

typedef struct ID3D11Device ID3D11Device;
/**
   \brief Returns the D3D11 device associated with a renderer, or NULL if it's not a D3D11 renderer.

   Once you are done using the device, you should release it to avoid a resource leak.
 */
extern DECLSPEC ID3D11Device* SDLCALL SDL_RenderGetD3D11Device(SDL_Renderer * renderer);

/**
   \brief Returns the DXGI Adapter and Output indices for the specified display index.

   These can be passed to EnumAdapters and EnumOutputs respectively to get the objects
   required to create a DX10 or DX11 device and swap chain.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_DXGIGetOutputInfo( int displayIndex, int *adapterIndex, int *outputIndex );

#endif /* __WIN32__ */


/* Platform specific functions for Linux */
#ifdef __LINUX__

/**
   \brief Sets the UNIX nice value for a thread, using setpriority() if possible, and RealtimeKit if available.

   \return 0 on success, or -1 on error.
 */
extern DECLSPEC int SDLCALL SDL_LinuxSetThreadPriority(Sint64 threadID, int priority);
 
#endif /* __LINUX__ */
	
/* Platform specific functions for iOS */
#ifdef __IPHONEOS__

#define SDL_iOSSetAnimationCallback(window, interval, callback, callbackParam) SDL_iPhoneSetAnimationCallback(window, interval, callback, callbackParam)
extern DECLSPEC int SDLCALL SDL_iPhoneSetAnimationCallback(SDL_Window * window, int interval, void (*callback)(void*), void *callbackParam);

#define SDL_iOSSetEventPump(enabled) SDL_iPhoneSetEventPump(enabled)
extern DECLSPEC void SDLCALL SDL_iPhoneSetEventPump(SDL_bool enabled);

#endif /* __IPHONEOS__ */


/* Platform specific functions for Android */
#ifdef __ANDROID__

/**
   \brief Get the JNI environment for the current thread

   This returns JNIEnv*, but the prototype is void* so we don't need jni.h
 */
extern DECLSPEC void * SDLCALL SDL_AndroidGetJNIEnv(void);

/**
   \brief Get the SDL Activity object for the application

   This returns jobject, but the prototype is void* so we don't need jni.h
   The jobject returned by SDL_AndroidGetActivity is a local reference.
   It is the caller's responsibility to properly release it
   (using env->Push/PopLocalFrame or manually with env->DeleteLocalRef)
 */
extern DECLSPEC void * SDLCALL SDL_AndroidGetActivity(void);

/**
   \brief Return API level of the current device

    API level 30: Android 11
    API level 29: Android 10
    API level 28: Android 9
    API level 27: Android 8.1
    API level 26: Android 8.0
    API level 25: Android 7.1
    API level 24: Android 7.0
    API level 23: Android 6.0
    API level 22: Android 5.1
    API level 21: Android 5.0
    API level 20: Android 4.4W
    API level 19: Android 4.4
    API level 18: Android 4.3
    API level 17: Android 4.2
    API level 16: Android 4.1
    API level 15: Android 4.0.3
    API level 14: Android 4.0
    API level 13: Android 3.2
    API level 12: Android 3.1
    API level 11: Android 3.0
    API level 10: Android 2.3.3
 */
extern DECLSPEC int SDLCALL SDL_GetAndroidSDKVersion(void);

/**
   \brief Return true if the application is running on Android TV
 */
extern DECLSPEC SDL_bool SDLCALL SDL_IsAndroidTV(void);

/**
   \brief Return true if the application is running on a Chromebook
 */
extern DECLSPEC SDL_bool SDLCALL SDL_IsChromebook(void);

/**
  \brief Return true is the application is running on a Samsung DeX docking station
 */
extern DECLSPEC SDL_bool SDLCALL SDL_IsDeXMode(void);

/**
 \brief Trigger the Android system back button behavior.
 */
extern DECLSPEC void SDLCALL SDL_AndroidBackButton(void);

/**
   See the official Android developer guide for more information:
   http://developer.android.com/guide/topics/data/data-storage.html
*/
#define SDL_ANDROID_EXTERNAL_STORAGE_READ   0x01
#define SDL_ANDROID_EXTERNAL_STORAGE_WRITE  0x02

/**
   \brief Get the path used for internal storage for this application.

   This path is unique to your application and cannot be written to
   by other applications.
 */
extern DECLSPEC const char * SDLCALL SDL_AndroidGetInternalStoragePath(void);

/**
   \brief Get the current state of external storage, a bitmask of these values:
    SDL_ANDROID_EXTERNAL_STORAGE_READ
    SDL_ANDROID_EXTERNAL_STORAGE_WRITE

   If external storage is currently unavailable, this will return 0.
*/
extern DECLSPEC int SDLCALL SDL_AndroidGetExternalStorageState(void);

/**
   \brief Get the path used for external storage for this application.

   This path is unique to your application, but is public and can be
   written to by other applications.
 */
extern DECLSPEC const char * SDLCALL SDL_AndroidGetExternalStoragePath(void);

/**
   \brief Request permissions at runtime.

   This blocks the calling thread until the permission is granted or
   denied. Returns SDL_TRUE if the permission was granted.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_AndroidRequestPermission(const char *permission);

/**
   \brief Shows android toast notification

   \note Shows toast in UI thread [https://developer.android.com/guide/topics/ui/notifiers/toasts]

   \param message   : text message to be shown
        duration  : 0 - short [https://developer.android.com/reference/android/widget/Toast#LENGTH_SHORT],
                    1 - long  [https://developer.android.com/reference/android/widget/Toast#LENGTH_LONG]
        gravity   : the location at which the notification should appear on the screen.
                    It's an optional parameter. Set -1 if you don't want specify any gravity or
                    choose some value from https://developer.android.com/reference/android/view/Gravity
        xOffset   : set this parameter only when gravity >=0
        yOffset   : set this parameter only when gravity >=0
   \return 0 if success, -1 if any error occurs.
*/
extern DECLSPEC int SDLCALL SDL_AndroidShowToast(const char* message, int duration, int gravity, int xOffset, int yOffset);

#endif /* __ANDROID__ */

/* Platform specific functions for WinRT */
#ifdef __WINRT__

/**
 *  \brief WinRT / Windows Phone path types
 */
typedef enum
{
    /** \brief The installed app's root directory.
        Files here are likely to be read-only. */
    SDL_WINRT_PATH_INSTALLED_LOCATION,

    /** \brief The app's local data store.  Files may be written here */
    SDL_WINRT_PATH_LOCAL_FOLDER,

    /** \brief The app's roaming data store.  Unsupported on Windows Phone.
        Files written here may be copied to other machines via a network
        connection.
    */
    SDL_WINRT_PATH_ROAMING_FOLDER,

    /** \brief The app's temporary data store.  Unsupported on Windows Phone.
        Files written here may be deleted at any time. */
    SDL_WINRT_PATH_TEMP_FOLDER
} SDL_WinRT_Path;


/**
 *  \brief WinRT Device Family
 */
typedef enum
{
    /** \brief Unknown family  */
    SDL_WINRT_DEVICEFAMILY_UNKNOWN,

    /** \brief Desktop family*/
    SDL_WINRT_DEVICEFAMILY_DESKTOP,

    /** \brief Mobile family (for example smartphone) */
    SDL_WINRT_DEVICEFAMILY_MOBILE,

    /** \brief XBox family */
    SDL_WINRT_DEVICEFAMILY_XBOX,
} SDL_WinRT_DeviceFamily;


/**
 *  \brief Retrieves a WinRT defined path on the local file system
 *
 *  \note Documentation on most app-specific path types on WinRT
 *      can be found on MSDN, at the URL:
 *      http://msdn.microsoft.com/en-us/library/windows/apps/hh464917.aspx
 *
 *  \param pathType The type of path to retrieve.
 *  \return A UCS-2 string (16-bit, wide-char) containing the path, or NULL
 *      if the path is not available for any reason.  Not all paths are
 *      available on all versions of Windows.  This is especially true on
 *      Windows Phone.  Check the documentation for the given
 *      SDL_WinRT_Path for more information on which path types are
 *      supported where.
 */
extern DECLSPEC const wchar_t * SDLCALL SDL_WinRTGetFSPathUNICODE(SDL_WinRT_Path pathType);

/**
 *  \brief Retrieves a WinRT defined path on the local file system
 *
 *  \note Documentation on most app-specific path types on WinRT
 *      can be found on MSDN, at the URL:
 *      http://msdn.microsoft.com/en-us/library/windows/apps/hh464917.aspx
 *
 *  \param pathType The type of path to retrieve.
 *  \return A UTF-8 string (8-bit, multi-byte) containing the path, or NULL
 *      if the path is not available for any reason.  Not all paths are
 *      available on all versions of Windows.  This is especially true on
 *      Windows Phone.  Check the documentation for the given
 *      SDL_WinRT_Path for more information on which path types are
 *      supported where.
 */
extern DECLSPEC const char * SDLCALL SDL_WinRTGetFSPathUTF8(SDL_WinRT_Path pathType);

/**
 *  \brief Detects the device family of WinRT plattform on runtime
 *
 *  \return Device family
 */
extern DECLSPEC SDL_WinRT_DeviceFamily SDLCALL SDL_WinRTGetDeviceFamily();

#endif /* __WINRT__ */

/**
 \brief Return true if the current device is a tablet.
 */
extern DECLSPEC SDL_bool SDLCALL SDL_IsTablet(void);

/* Functions used by iOS application delegates to notify SDL about state changes */
extern DECLSPEC void SDLCALL SDL_OnApplicationWillTerminate(void);
extern DECLSPEC void SDLCALL SDL_OnApplicationDidReceiveMemoryWarning(void);
extern DECLSPEC void SDLCALL SDL_OnApplicationWillResignActive(void);
extern DECLSPEC void SDLCALL SDL_OnApplicationDidEnterBackground(void);
extern DECLSPEC void SDLCALL SDL_OnApplicationWillEnterForeground(void);
extern DECLSPEC void SDLCALL SDL_OnApplicationDidBecomeActive(void);
#ifdef __IPHONEOS__
extern DECLSPEC void SDLCALL SDL_OnApplicationDidChangeStatusBarOrientation(void);
#endif

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_system_h_ */

/* vi: set ts=4 sw=4 expandtab: */
