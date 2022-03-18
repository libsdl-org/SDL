/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#if defined(__WIN32__) || defined(__WINRT__)

#include "SDL_windows.h"
#include "SDL_error.h"

#include <objbase.h>  /* for CoInitialize/CoUninitialize (Win32 only) */

#ifndef _WIN32_WINNT_VISTA
#define _WIN32_WINNT_VISTA  0x0600
#endif
#ifndef _WIN32_WINNT_WIN7
#define _WIN32_WINNT_WIN7   0x0601
#endif
#ifndef _WIN32_WINNT_WIN8
#define _WIN32_WINNT_WIN8   0x0602
#endif


/* Sets an error message based on an HRESULT */
int
WIN_SetErrorFromHRESULT(const char *prefix, HRESULT hr)
{
    TCHAR buffer[1024];
    char *message;
    TCHAR *p = buffer;
    DWORD c = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, 0,
                  buffer, SDL_arraysize(buffer), NULL);
    buffer[c] = 0;
    /* kill CR/LF that FormatMessage() sticks at the end */
    while (*p) {
        if (*p == '\r') {
            *p = 0;
            break;
        }
        ++p;
    }
    message = WIN_StringToUTF8(buffer);
    SDL_SetError("%s%s%s", prefix ? prefix : "", prefix ? ": " : "", message);
    SDL_free(message);
    return -1;
}

/* Sets an error message based on GetLastError() */
int
WIN_SetError(const char *prefix)
{
    return WIN_SetErrorFromHRESULT(prefix, GetLastError());
}

HRESULT
WIN_CoInitialize(void)
{
    /* SDL handles any threading model, so initialize with the default, which
       is compatible with OLE and if that doesn't work, try multi-threaded mode.

       If you need multi-threaded mode, call CoInitializeEx() before SDL_Init()
    */
#ifdef __WINRT__
    /* DLudwig: On WinRT, it is assumed that COM was initialized in main().
       CoInitializeEx is available (not CoInitialize though), however
       on WinRT, main() is typically declared with the [MTAThread]
       attribute, which, AFAIK, should initialize COM.
    */
    return S_OK;
#else
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    }

    /* S_FALSE means success, but someone else already initialized. */
    /* You still need to call CoUninitialize in this case! */
    if (hr == S_FALSE) {
        return S_OK;
    }

    return hr;
#endif
}

void
WIN_CoUninitialize(void)
{
#ifndef __WINRT__
    /* Don't uninitialize COM because of what appears to be a bug in Microsoft WGI reference counting.
     *
     * If you plug in a non-Xbox controller and let the application run for 30 seconds, then it crashes in CoUninitialize()
     * with this stack trace:

        Windows.Gaming.Input.dll!GameController::~GameController(void)	Unknown
        Windows.Gaming.Input.dll!GameController::`vector deleting destructor'(unsigned int)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::Details::RuntimeClassImpl<struct Microsoft::WRL::RuntimeClassFlags<1>,1,1,0,struct Windows::Gaming::Input::IGameController,struct Windows::Gaming::Input::IGameControllerBatteryInfo,struct Microsoft::WRL::CloakedIid<struct Windows::Gaming::Input::Internal::IGameControllerPrivate>,class Microsoft::WRL::FtmBase>::Release(void)	Unknown
        Windows.Gaming.Input.dll!Windows::Gaming::Input::Custom::Details::AggregableRuntimeClass<struct Windows::Gaming::Input::IGamepad,struct Windows::Gaming::Input::IGamepad2,struct Microsoft::WRL::CloakedIid<struct Windows::Gaming::Input::Custom::IGameControllerInputSink>,struct Microsoft::WRL::CloakedIid<struct Windows::Gaming::Input::Custom::IGipGameControllerInputSink>,struct Microsoft::WRL::CloakedIid<struct Windows::Gaming::Input::Custom::IHidGameControllerInputSink>,struct Microsoft::WRL::CloakedIid<struct Windows::Gaming::Input::Custom::IXusbGameControllerInputSink>,class Microsoft::WRL::Details::Nil,class Microsoft::WRL::Details::Nil,class Microsoft::WRL::Details::Nil>::Release(void)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::ComPtr<`WaitForCompletion<Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<Windows::Storage::Streams::IBuffer *,unsigned int>,Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IBuffer *,unsigned int>>'::`2'::FTMEventDelegate>::~ComPtr<`WaitForCompletion<Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<Windows::Storage::Streams::IBuffer *,unsigned int>,Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IBuffer *,unsigned int>>'::`2'::FTMEventDelegate>()	Unknown
        Windows.Gaming.Input.dll!`eh vector destructor iterator'(void *,unsigned int,int,void (*)(void *))	Unknown
        Windows.Gaming.Input.dll!Windows::Gaming::Input::Custom::Details::GameControllerCollection<class Windows::Gaming::Input::RawGameController,struct Windows::Gaming::Input::IRawGameController>::~GameControllerCollection<class Windows::Gaming::Input::RawGameController,struct Windows::Gaming::Input::IRawGameController>(void)	Unknown
        Windows.Gaming.Input.dll!Windows::Gaming::Input::Custom::Details::GameControllerCollection<class Windows::Gaming::Input::RawGameController,struct Windows::Gaming::Input::IRawGameController>::`vector deleting destructor'(unsigned int)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::Details::RuntimeClassImpl<struct Microsoft::WRL::RuntimeClassFlags<1>,1,1,0,struct Windows::Foundation::Collections::IIterable<class Windows::Gaming::Input::ArcadeStick *>,struct Windows::Foundation::Collections::IVectorView<class Windows::Gaming::Input::ArcadeStick *>,class Microsoft::WRL::FtmBase>::Release(void)	Unknown
        Windows.Gaming.Input.dll!Windows::Gaming::Input::Custom::Details::CustomGameControllerFactoryBase<class Windows::Gaming::Input::FlightStick,class Windows::Gaming::Input::FlightStick,struct Windows::Gaming::Input::IFlightStick,struct Windows::Gaming::Input::IFlightStickStatics,class Microsoft::WRL::Details::Nil>::~CustomGameControllerFactoryBase<class Windows::Gaming::Input::FlightStick,class Windows::Gaming::Input::FlightStick,struct Windows::Gaming::Input::IFlightStick,struct Windows::Gaming::Input::IFlightStickStatics,class Microsoft::WRL::Details::Nil>(void)	Unknown
        Windows.Gaming.Input.dll!Windows::Gaming::Input::Custom::Details::CustomGameControllerFactoryBase<class Windows::Gaming::Input::FlightStick,class Windows::Gaming::Input::FlightStick,struct Windows::Gaming::Input::IFlightStick,struct Windows::Gaming::Input::IFlightStickStatics,class Microsoft::WRL::Details::Nil>::`vector deleting destructor'(unsigned int)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::ActivationFactory<struct Microsoft::WRL::Implements<class Microsoft::WRL::FtmBase,struct Microsoft::WRL::CloakedIid<struct Windows::Gaming::Input::Custom::ICustomGameControllerFactory> >,struct Windows::Gaming::Input::IFlightStickStatics,class Microsoft::WRL::Details::Nil,0>::Release(void)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::ComPtr<`WaitForCompletion<Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<Windows::Storage::Streams::IBuffer *,unsigned int>,Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IBuffer *,unsigned int>>'::`2'::FTMEventDelegate>::~ComPtr<`WaitForCompletion<Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<Windows::Storage::Streams::IBuffer *,unsigned int>,Windows::Foundation::IAsyncOperationWithProgress<Windows::Storage::Streams::IBuffer *,unsigned int>>'::`2'::FTMEventDelegate>()	Unknown
        Windows.Gaming.Input.dll!NtList<struct FactoryManager::FactoryListEntry>::~NtList<struct FactoryManager::FactoryListEntry>(void)	Unknown
        Windows.Gaming.Input.dll!FactoryManager::`vector deleting destructor'(unsigned int)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::ActivationFactory<struct Microsoft::WRL::Implements<class Microsoft::WRL::FtmBase,struct Windows::Gaming::Input::Custom::IGameControllerFactoryManagerStatics>,struct Windows::Gaming::Input::Custom::IGameControllerFactoryManagerStatics2,struct Microsoft::WRL::CloakedIid<struct Windows::Gaming::Input::Internal::IGameControllerFactoryManagerStaticsPrivate>,0>::Release(void)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::Details::TerminateMap(class Microsoft::WRL::Details::ModuleBase *,unsigned short const *,bool)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::Module<1,class Microsoft::WRL::Details::DefaultModule<1> >::~Module<1,class Microsoft::WRL::Details::DefaultModule<1> >(void)	Unknown
        Windows.Gaming.Input.dll!Microsoft::WRL::Details::DefaultModule<1>::`vector deleting destructor'(unsigned int)	Unknown
        Windows.Gaming.Input.dll!`dynamic atexit destructor for 'Microsoft::WRL::Details::StaticStorage<Microsoft::WRL::Details::DefaultModule<1>,0,int>::instance_''()	Unknown
        Windows.Gaming.Input.dll!__CRT_INIT@12()	Unknown
        Windows.Gaming.Input.dll!__DllMainCRTStartup()	Unknown
        ntdll.dll!_LdrxCallInitRoutine@16()	Unknown
        ntdll.dll!LdrpCallInitRoutine()	Unknown
        ntdll.dll!LdrpProcessDetachNode()	Unknown
        ntdll.dll!LdrpUnloadNode()	Unknown
        ntdll.dll!LdrpDecrementModuleLoadCountEx()	Unknown
        ntdll.dll!LdrUnloadDll()	Unknown
        KernelBase.dll!FreeLibrary()	Unknown
        combase.dll!FreeLibraryWithLogging(LoadOrFreeWhy why, HINSTANCE__ * hMod, const wchar_t * pswzOptionalFileName) Line 193	C++
        combase.dll!CClassCache::CDllPathEntry::CFinishObject::Finish() Line 3311	C++
        combase.dll!CClassCache::CFinishComposite::Finish() Line 3421	C++
        combase.dll!CClassCache::CleanUpDllsForProcess() Line 7009	C++
        [Inline Frame] combase.dll!CCCleanUpDllsForProcess() Line 8773	C++
        combase.dll!ProcessUninitialize() Line 2243	C++
        combase.dll!DecrementProcessInitializeCount() Line 993	C++
        combase.dll!wCoUninitialize(COleTls & Tls, int fHostThread) Line 4126	C++
        combase.dll!CoUninitialize() Line 3945	C++
    */
    /*CoUninitialize();*/
#endif
}

#ifndef __WINRT__
static BOOL
IsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(
        VerSetConditionMask(
        VerSetConditionMask(
        0, VER_MAJORVERSION, VER_GREATER_EQUAL ),
        VER_MINORVERSION, VER_GREATER_EQUAL ),
        VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );

    SDL_zero(osvi);
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;

    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}
#endif

BOOL WIN_IsWindowsVistaOrGreater(void)
{
#ifdef __WINRT__
    return TRUE;
#else
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_VISTA), LOBYTE(_WIN32_WINNT_VISTA), 0);
#endif
}

BOOL WIN_IsWindows7OrGreater(void)
{
#ifdef __WINRT__
    return TRUE;
#else
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN7), LOBYTE(_WIN32_WINNT_WIN7), 0);
#endif
}

BOOL WIN_IsWindows8OrGreater(void)
{
#ifdef __WINRT__
    return TRUE;
#else
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN8), LOBYTE(_WIN32_WINNT_WIN8), 0);
#endif
}

/*
WAVExxxCAPS gives you 31 bytes for the device name, and just truncates if it's
longer. However, since WinXP, you can use the WAVExxxCAPS2 structure, which
will give you a name GUID. The full name is in the Windows Registry under
that GUID, located here: HKLM\System\CurrentControlSet\Control\MediaCategories

Note that drivers can report GUID_NULL for the name GUID, in which case,
Windows makes a best effort to fill in those 31 bytes in the usual place.
This info summarized from MSDN:

http://web.archive.org/web/20131027093034/http://msdn.microsoft.com/en-us/library/windows/hardware/ff536382(v=vs.85).aspx

Always look this up in the registry if possible, because the strings are
different! At least on Win10, I see "Yeti Stereo Microphone" in the
Registry, and a unhelpful "Microphone(Yeti Stereo Microph" in winmm. Sigh.

(Also, DirectSound shouldn't be limited to 32 chars, but its device enum
has the same problem.)

WASAPI doesn't need this. This is just for DirectSound/WinMM.
*/
char *
WIN_LookupAudioDeviceName(const WCHAR *name, const GUID *guid)
{
#if __WINRT__
    return WIN_StringToUTF8(name);  /* No registry access on WinRT/UWP, go with what we've got. */
#else
    static const GUID nullguid = { 0 };
    const unsigned char *ptr;
    char keystr[128];
    WCHAR *strw = NULL;
    SDL_bool rc;
    HKEY hkey;
    DWORD len = 0;
    char *retval = NULL;

    if (WIN_IsEqualGUID(guid, &nullguid)) {
        return WIN_StringToUTF8(name);  /* No GUID, go with what we've got. */
    }

    ptr = (const unsigned char *) guid;
    SDL_snprintf(keystr, sizeof (keystr),
        "System\\CurrentControlSet\\Control\\MediaCategories\\{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        ptr[3], ptr[2], ptr[1], ptr[0], ptr[5], ptr[4], ptr[7], ptr[6],
        ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);

    strw = WIN_UTF8ToString(keystr);
    rc = (RegOpenKeyExW(HKEY_LOCAL_MACHINE, strw, 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS);
    SDL_free(strw);
    if (!rc) {
        return WIN_StringToUTF8(name);  /* oh well. */
    }

    rc = (RegQueryValueExW(hkey, L"Name", NULL, NULL, NULL, &len) == ERROR_SUCCESS);
    if (!rc) {
        RegCloseKey(hkey);
        return WIN_StringToUTF8(name);  /* oh well. */
    }

    strw = (WCHAR *) SDL_malloc(len + sizeof (WCHAR));
    if (!strw) {
        RegCloseKey(hkey);
        return WIN_StringToUTF8(name);  /* oh well. */
    }

    rc = (RegQueryValueExW(hkey, L"Name", NULL, NULL, (LPBYTE) strw, &len) == ERROR_SUCCESS);
    RegCloseKey(hkey);
    if (!rc) {
        SDL_free(strw);
        return WIN_StringToUTF8(name);  /* oh well. */
    }

    strw[len / 2] = 0;  /* make sure it's null-terminated. */

    retval = WIN_StringToUTF8(strw);
    SDL_free(strw);
    return retval ? retval : WIN_StringToUTF8(name);
#endif /* if __WINRT__ / else */
}

BOOL
WIN_IsEqualGUID(const GUID * a, const GUID * b)
{
    return (SDL_memcmp(a, b, sizeof (*a)) == 0);
}

BOOL
WIN_IsEqualIID(REFIID a, REFIID b)
{
    return (SDL_memcmp(a, b, sizeof (*a)) == 0);
}

void
WIN_RECTToRect(const RECT *winrect, SDL_Rect *sdlrect)
{
    sdlrect->x = winrect->left;
    sdlrect->w = (winrect->right - winrect->left) + 1;
    sdlrect->y = winrect->top;
    sdlrect->h = (winrect->bottom - winrect->top) + 1;
}

void
WIN_RectToRECT(const SDL_Rect *sdlrect, RECT *winrect)
{
    winrect->left = sdlrect->x;
    winrect->right = sdlrect->x + sdlrect->w - 1;
    winrect->top = sdlrect->y;
    winrect->bottom = sdlrect->y + sdlrect->h - 1;
}

#endif /* __WIN32__ || __WINRT__ */

/* vi: set ts=4 sw=4 expandtab: */
