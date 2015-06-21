/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

#include <xaudio2.h>
#include "SDL_xaudio2_winrthelpers.h"

#if WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
using Windows::Devices::Enumeration::DeviceClass;
using Windows::Devices::Enumeration::DeviceInformation;
using Windows::Devices::Enumeration::DeviceInformationCollection;
#endif

extern "C" HRESULT __cdecl IXAudio2_GetDeviceCount(IXAudio2 * ixa2, UINT32 * devcount)
{
#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
    // There doesn't seem to be any audio device enumeration on Windows Phone.
    // In lieu of this, just treat things as if there is one and only one
    // audio device.
    *devcount = 1;
    return S_OK;
#else
    // TODO, WinRT: make xaudio2 device enumeration only happen once, and in the background
    auto operation = DeviceInformation::FindAllAsync(DeviceClass::AudioRender);
    while (operation->Status != Windows::Foundation::AsyncStatus::Completed)
    {
    }
 
    DeviceInformationCollection^ devices = operation->GetResults();
    *devcount = devices->Size;
    return S_OK;
#endif
}

extern "C" HRESULT IXAudio2_GetDeviceDetails(IXAudio2 * unused, UINT32 index, XAUDIO2_DEVICE_DETAILS * details)
{
#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
    // Windows Phone doesn't seem to have the same device enumeration APIs that
    // Windows 8/RT has, or it doesn't have them at all.  In lieu of this,
    // just treat things as if there is one, and only one, default device.
    if (index != 0)
    {
        return XAUDIO2_E_INVALID_CALL;
    }

    if (details)
    {
        wcsncpy_s(details->DeviceID, ARRAYSIZE(details->DeviceID), L"default", _TRUNCATE);
        wcsncpy_s(details->DisplayName, ARRAYSIZE(details->DisplayName), L"default", _TRUNCATE);
    }
    return S_OK;
#else
    auto operation = DeviceInformation::FindAllAsync(DeviceClass::AudioRender);
    while (operation->Status != Windows::Foundation::AsyncStatus::Completed)
    {
    }
 
    DeviceInformationCollection^ devices = operation->GetResults();
    if (index >= devices->Size)
    {
        return XAUDIO2_E_INVALID_CALL;
    }

    DeviceInformation^ d = devices->GetAt(index);
    if (details)
    {
        wcsncpy_s(details->DeviceID, ARRAYSIZE(details->DeviceID), d->Id->Data(), _TRUNCATE);
        wcsncpy_s(details->DisplayName, ARRAYSIZE(details->DisplayName), d->Name->Data(), _TRUNCATE);
    }
    return S_OK;
#endif
}
