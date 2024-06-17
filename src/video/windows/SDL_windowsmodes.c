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

#if defined(SDL_VIDEO_DRIVER_WINDOWS) && !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)

#include "SDL_windowsvideo.h"
#include "../../events/SDL_displayevents_c.h"

#ifdef HAVE_DXGI1_6_H
#define COBJMACROS
#include <dxgi1_6.h>
#endif

/* Windows CE compatibility */
#ifndef CDS_FULLSCREEN
#define CDS_FULLSCREEN 0
#endif

/* #define DEBUG_MODES */
/* #define HIGHDPI_DEBUG_VERBOSE */

static void WIN_UpdateDisplayMode(SDL_VideoDevice *_this, LPCWSTR deviceName, DWORD index, SDL_DisplayMode *mode)
{
    SDL_DisplayModeData *data = (SDL_DisplayModeData *)mode->driverdata;
    HDC hdc;

    data->DeviceMode.dmFields = (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS);

    /* NOLINTNEXTLINE(bugprone-assignment-in-if-condition): No simple way to extract the assignment */
    if (index == ENUM_CURRENT_SETTINGS && (hdc = CreateDC(deviceName, NULL, NULL, NULL)) != NULL) {
        char bmi_data[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
        LPBITMAPINFO bmi;
        HBITMAP hbm;

        SDL_zeroa(bmi_data);
        bmi = (LPBITMAPINFO)bmi_data;
        bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

        hbm = CreateCompatibleBitmap(hdc, 1, 1);
        GetDIBits(hdc, hbm, 0, 1, NULL, bmi, DIB_RGB_COLORS);
        GetDIBits(hdc, hbm, 0, 1, NULL, bmi, DIB_RGB_COLORS);
        DeleteObject(hbm);
        DeleteDC(hdc);
        if (bmi->bmiHeader.biCompression == BI_BITFIELDS) {
            switch (*(Uint32 *)bmi->bmiColors) {
            case 0x00FF0000:
                mode->format = SDL_PIXELFORMAT_XRGB8888;
                break;
            case 0x000000FF:
                mode->format = SDL_PIXELFORMAT_XBGR8888;
                break;
            case 0xF800:
                mode->format = SDL_PIXELFORMAT_RGB565;
                break;
            case 0x7C00:
                mode->format = SDL_PIXELFORMAT_XRGB1555;
                break;
            }
        } else if (bmi->bmiHeader.biCompression == BI_RGB) {
            if (bmi->bmiHeader.biBitCount == 24) {
                mode->format = SDL_PIXELFORMAT_RGB24;
            } else if (bmi->bmiHeader.biBitCount == 8) {
                mode->format = SDL_PIXELFORMAT_INDEX8;
            } else if (bmi->bmiHeader.biBitCount == 4) {
                mode->format = SDL_PIXELFORMAT_INDEX4LSB;
            }
        }
    } else if (mode->format == SDL_PIXELFORMAT_UNKNOWN) {
        /* FIXME: Can we tell what this will be? */
        if ((data->DeviceMode.dmFields & DM_BITSPERPEL) == DM_BITSPERPEL) {
            switch (data->DeviceMode.dmBitsPerPel) {
            case 32:
                mode->format = SDL_PIXELFORMAT_XRGB8888;
                break;
            case 24:
                mode->format = SDL_PIXELFORMAT_RGB24;
                break;
            case 16:
                mode->format = SDL_PIXELFORMAT_RGB565;
                break;
            case 15:
                mode->format = SDL_PIXELFORMAT_XRGB1555;
                break;
            case 8:
                mode->format = SDL_PIXELFORMAT_INDEX8;
                break;
            case 4:
                mode->format = SDL_PIXELFORMAT_INDEX4LSB;
                break;
            }
        }
    }
}

static SDL_DisplayOrientation WIN_GetNaturalOrientation(DEVMODE *mode)
{
    int width = mode->dmPelsWidth;
    int height = mode->dmPelsHeight;

    /* Use unrotated width/height to guess orientation */
    if (mode->dmDisplayOrientation == DMDO_90 || mode->dmDisplayOrientation == DMDO_270) {
        int temp = width;
        width = height;
        height = temp;
    }

    if (width >= height) {
        return SDL_ORIENTATION_LANDSCAPE;
    } else {
        return SDL_ORIENTATION_PORTRAIT;
    }
}

static SDL_DisplayOrientation WIN_GetDisplayOrientation(DEVMODE *mode)
{
    if (WIN_GetNaturalOrientation(mode) == SDL_ORIENTATION_LANDSCAPE) {
        switch (mode->dmDisplayOrientation) {
        case DMDO_DEFAULT:
            return SDL_ORIENTATION_LANDSCAPE;
        case DMDO_90:
            return SDL_ORIENTATION_PORTRAIT;
        case DMDO_180:
            return SDL_ORIENTATION_LANDSCAPE_FLIPPED;
        case DMDO_270:
            return SDL_ORIENTATION_PORTRAIT_FLIPPED;
        default:
            return SDL_ORIENTATION_UNKNOWN;
        }
    } else {
        switch (mode->dmDisplayOrientation) {
        case DMDO_DEFAULT:
            return SDL_ORIENTATION_PORTRAIT;
        case DMDO_90:
            return SDL_ORIENTATION_LANDSCAPE_FLIPPED;
        case DMDO_180:
            return SDL_ORIENTATION_PORTRAIT_FLIPPED;
        case DMDO_270:
            return SDL_ORIENTATION_LANDSCAPE;
        default:
            return SDL_ORIENTATION_UNKNOWN;
        }
    }
}

static float WIN_GetRefreshRate(DEVMODE *mode)
{
    /* We're not currently using DXGI to query display modes, so fake NTSC timings */
    switch (mode->dmDisplayFrequency) {
    case 119:
    case 59:
    case 29:
        return ((100 * (mode->dmDisplayFrequency + 1) * 1000) / 1001) / 100.0f;
    default:
        return (float)mode->dmDisplayFrequency;
    }
}

static float WIN_GetContentScale(SDL_VideoDevice *_this, HMONITOR hMonitor)
{
    const SDL_VideoData *videodata = (const SDL_VideoData *)_this->driverdata;
    int dpi = 0;

    if (videodata->GetDpiForMonitor) {
        UINT hdpi_uint, vdpi_uint;
        if (videodata->GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &hdpi_uint, &vdpi_uint) == S_OK) {
            dpi = (int)hdpi_uint;
        }
    }
    if (dpi == 0) {
        /* Window 8.0 and below: same DPI for all monitors */
        HDC hdc = GetDC(NULL);
        if (hdc) {
            dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(NULL, hdc);
        }
    }
    if (dpi == 0) {
        /* Safe default */
        dpi = USER_DEFAULT_SCREEN_DPI;
    }
    return dpi / (float)USER_DEFAULT_SCREEN_DPI;
}

static SDL_bool WIN_GetDisplayMode(SDL_VideoDevice *_this, HMONITOR hMonitor, LPCWSTR deviceName, DWORD index, SDL_DisplayMode *mode, SDL_DisplayOrientation *natural_orientation, SDL_DisplayOrientation *current_orientation)
{
    SDL_DisplayModeData *data;
    DEVMODE devmode;

    devmode.dmSize = sizeof(devmode);
    devmode.dmDriverExtra = 0;
    if (!EnumDisplaySettingsW(deviceName, index, &devmode)) {
        return SDL_FALSE;
    }

    data = (SDL_DisplayModeData *)SDL_malloc(sizeof(*data));
    if (!data) {
        return SDL_FALSE;
    }

    SDL_zerop(mode);
    mode->driverdata = data;
    data->DeviceMode = devmode;

    mode->format = SDL_PIXELFORMAT_UNKNOWN;
    mode->w = data->DeviceMode.dmPelsWidth;
    mode->h = data->DeviceMode.dmPelsHeight;
    mode->refresh_rate = WIN_GetRefreshRate(&data->DeviceMode);

    /* Fill in the mode information */
    WIN_UpdateDisplayMode(_this, deviceName, index, mode);

    if (natural_orientation) {
        *natural_orientation = WIN_GetNaturalOrientation(&devmode);
    }
    if (current_orientation) {
        *current_orientation = WIN_GetDisplayOrientation(&devmode);
    }

    return SDL_TRUE;
}

static char *WIN_GetDisplayNameVista(SDL_VideoData *videodata, const WCHAR *deviceName)
{
    DISPLAYCONFIG_PATH_INFO *paths = NULL;
    DISPLAYCONFIG_MODE_INFO *modes = NULL;
    char *retval = NULL;
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    UINT32 i;
    LONG rc;

    if (!videodata->GetDisplayConfigBufferSizes || !videodata->QueryDisplayConfig || !videodata->DisplayConfigGetDeviceInfo) {
        return NULL;
    }

    do {
        rc = videodata->GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
        if (rc != ERROR_SUCCESS) {
            goto WIN_GetDisplayNameVista_failed;
        }

        SDL_free(paths);
        SDL_free(modes);

        paths = (DISPLAYCONFIG_PATH_INFO *)SDL_malloc(sizeof(DISPLAYCONFIG_PATH_INFO) * pathCount);
        modes = (DISPLAYCONFIG_MODE_INFO *)SDL_malloc(sizeof(DISPLAYCONFIG_MODE_INFO) * modeCount);
        if ((!paths) || (!modes)) {
            goto WIN_GetDisplayNameVista_failed;
        }

        rc = videodata->QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths, &modeCount, modes, 0);
    } while (rc == ERROR_INSUFFICIENT_BUFFER);

    if (rc == ERROR_SUCCESS) {
        for (i = 0; i < pathCount; i++) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
            DISPLAYCONFIG_TARGET_DEVICE_NAME targetName;

            SDL_zero(sourceName);
            sourceName.header.adapterId = paths[i].targetInfo.adapterId;
            sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            sourceName.header.size = sizeof(sourceName);
            sourceName.header.id = paths[i].sourceInfo.id;
            rc = videodata->DisplayConfigGetDeviceInfo(&sourceName.header);
            if (rc != ERROR_SUCCESS) {
                break;
            } else if (SDL_wcscmp(deviceName, sourceName.viewGdiDeviceName) != 0) {
                continue;
            }

            SDL_zero(targetName);
            targetName.header.adapterId = paths[i].targetInfo.adapterId;
            targetName.header.id = paths[i].targetInfo.id;
            targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            targetName.header.size = sizeof(targetName);
            rc = videodata->DisplayConfigGetDeviceInfo(&targetName.header);
            if (rc == ERROR_SUCCESS) {
                retval = WIN_StringToUTF8W(targetName.monitorFriendlyDeviceName);
                /* if we got an empty string, treat it as failure so we'll fallback
                   to getting the generic name. */
                if (retval && (*retval == '\0')) {
                    SDL_free(retval);
                    retval = NULL;
                }
            }
            break;
        }
    }

    SDL_free(paths);
    SDL_free(modes);
    return retval;

WIN_GetDisplayNameVista_failed:
    SDL_free(retval);
    SDL_free(paths);
    SDL_free(modes);
    return NULL;
}

#ifdef HAVE_DXGI1_6_H
static SDL_bool WIN_GetMonitorDESC1(HMONITOR hMonitor, DXGI_OUTPUT_DESC1 *desc)
{
    typedef HRESULT (WINAPI * PFN_CREATE_DXGI_FACTORY)(REFIID riid, void **ppFactory);
    PFN_CREATE_DXGI_FACTORY CreateDXGIFactoryFunc = NULL;
    void *hDXGIMod = NULL;
    SDL_bool found = SDL_FALSE;

#ifdef SDL_PLATFORM_WINRT
    CreateDXGIFactoryFunc = CreateDXGIFactory1;
#else
    hDXGIMod = SDL_LoadObject("dxgi.dll");
    if (hDXGIMod) {
        CreateDXGIFactoryFunc = (PFN_CREATE_DXGI_FACTORY)SDL_LoadFunction(hDXGIMod, "CreateDXGIFactory1");
    }
#endif
    if (CreateDXGIFactoryFunc) {
        static const GUID SDL_IID_IDXGIFactory1 = { 0x770aae78, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };
        static const GUID SDL_IID_IDXGIOutput6 = { 0x068346e8, 0xaaec, 0x4b84, { 0xad, 0xd7, 0x13, 0x7f, 0x51, 0x3f, 0x77, 0xa1 } };
        IDXGIFactory1 *dxgiFactory;

        if (SUCCEEDED(CreateDXGIFactoryFunc(&SDL_IID_IDXGIFactory1, (void **)&dxgiFactory))) {
            IDXGIAdapter1 *dxgiAdapter;
            UINT adapter = 0;
            while (!found && SUCCEEDED(IDXGIFactory1_EnumAdapters1(dxgiFactory, adapter, &dxgiAdapter))) {
                IDXGIOutput *dxgiOutput;
                UINT output = 0;
                while (!found && SUCCEEDED(IDXGIAdapter1_EnumOutputs(dxgiAdapter, output, &dxgiOutput))) {
                    IDXGIOutput6 *dxgiOutput6;
                    if (SUCCEEDED(IDXGIOutput_QueryInterface(dxgiOutput, &SDL_IID_IDXGIOutput6, (void **)&dxgiOutput6))) {
                        if (SUCCEEDED(IDXGIOutput6_GetDesc1(dxgiOutput6, desc))) {
                            if (desc->Monitor == hMonitor) {
                                found = SDL_TRUE;
                            }
                        }
                        IDXGIOutput6_Release(dxgiOutput6);
                    }
                    IDXGIOutput_Release(dxgiOutput);
                    ++output;
                }
                IDXGIAdapter1_Release(dxgiAdapter);
                ++adapter;
            }
            IDXGIFactory2_Release(dxgiFactory);
        }
    }
    if (hDXGIMod) {
        SDL_UnloadObject(hDXGIMod);
    }
    return found;
}

static SDL_bool WIN_GetMonitorPathInfo(SDL_VideoData *videodata, HMONITOR hMonitor, DISPLAYCONFIG_PATH_INFO *path_info)
{
    LONG result;
    MONITORINFOEXW view_info;
    UINT32 i;
    UINT32 num_path_array_elements = 0;
    UINT32 num_mode_info_array_elements = 0;
    DISPLAYCONFIG_PATH_INFO *path_infos = NULL, *new_path_infos;
    DISPLAYCONFIG_MODE_INFO *mode_infos = NULL, *new_mode_infos;
    SDL_bool found = SDL_FALSE;

    if (!videodata->GetDisplayConfigBufferSizes || !videodata->QueryDisplayConfig || !videodata->DisplayConfigGetDeviceInfo) {
        return SDL_FALSE;
    }

    SDL_zero(view_info);
    view_info.cbSize = sizeof(view_info);
    if (!GetMonitorInfoW(hMonitor, (MONITORINFO *)&view_info)) {
        goto done;
    }

    do {
        if (videodata->GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &num_path_array_elements, &num_mode_info_array_elements) != ERROR_SUCCESS) {
            SDL_free(path_infos);
            SDL_free(mode_infos);
            return SDL_FALSE;
        }

        new_path_infos = (DISPLAYCONFIG_PATH_INFO *)SDL_realloc(path_infos, num_path_array_elements * sizeof(*path_infos));
        if (!new_path_infos) {
            goto done;
        }
        path_infos = new_path_infos;

        new_mode_infos = (DISPLAYCONFIG_MODE_INFO *)SDL_realloc(mode_infos, num_mode_info_array_elements * sizeof(*mode_infos));
        if (!new_mode_infos) {
            goto done;
        }
        mode_infos = new_mode_infos;

        result = videodata->QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &num_path_array_elements, path_infos, &num_mode_info_array_elements, mode_infos, NULL);

    } while (result == ERROR_INSUFFICIENT_BUFFER);

    if (result == ERROR_SUCCESS) {
        for (i = 0; i < num_path_array_elements; ++i) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME device_name;

            SDL_zero(device_name);
            device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            device_name.header.size = sizeof(device_name);
            device_name.header.adapterId = path_infos[i].sourceInfo.adapterId;
            device_name.header.id = path_infos[i].sourceInfo.id;
            if (videodata->DisplayConfigGetDeviceInfo(&device_name.header) == ERROR_SUCCESS) {
                if (SDL_wcscmp(view_info.szDevice, device_name.viewGdiDeviceName) == 0) {
                    SDL_copyp(path_info, &path_infos[i]);
                    found = SDL_TRUE;
                    break;
                }
            }
        }
    }

done:
    SDL_free(path_infos);
    SDL_free(mode_infos);

    return found;
}

static float WIN_GetSDRWhitePoint(SDL_VideoDevice *_this, HMONITOR hMonitor)
{
    DISPLAYCONFIG_PATH_INFO path_info;
    SDL_VideoData *videodata = _this->driverdata;
    float SDR_white_level = 1.0f;

    if (WIN_GetMonitorPathInfo(videodata, hMonitor, &path_info)) {
        DISPLAYCONFIG_SDR_WHITE_LEVEL white_level;

        SDL_zero(white_level);
        white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
        white_level.header.size = sizeof(white_level);
        white_level.header.adapterId = path_info.targetInfo.adapterId;
        white_level.header.id = path_info.targetInfo.id;
        /* WIN_GetMonitorPathInfo() succeeded: DisplayConfigGetDeviceInfo is not NULL */
        if (videodata->DisplayConfigGetDeviceInfo(&white_level.header) == ERROR_SUCCESS &&
            white_level.SDRWhiteLevel > 0) {
            SDR_white_level = (white_level.SDRWhiteLevel / 1000.0f);
        }
    }
    return SDR_white_level;
}

static void WIN_GetHDRProperties(SDL_VideoDevice *_this, HMONITOR hMonitor, SDL_HDROutputProperties *HDR)
{
    DXGI_OUTPUT_DESC1 desc;

    SDL_zerop(HDR);

    if (WIN_GetMonitorDESC1(hMonitor, &desc)) {
        if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
            HDR->SDR_white_level = WIN_GetSDRWhitePoint(_this, hMonitor);
            HDR->HDR_headroom = (desc.MaxLuminance / 80.0f) / HDR->SDR_white_level;
        }
    }
}
#endif /* HAVE_DXGI1_6_H */

static void WIN_AddDisplay(SDL_VideoDevice *_this, HMONITOR hMonitor, const MONITORINFOEXW *info, int *display_index)
{
    int i, index = *display_index;
    SDL_VideoDisplay display;
    SDL_DisplayData *displaydata;
    SDL_DisplayMode mode;
    SDL_DisplayOrientation natural_orientation;
    SDL_DisplayOrientation current_orientation;
    float content_scale = WIN_GetContentScale(_this, hMonitor);

#ifdef DEBUG_MODES
    SDL_Log("Display: %s\n", WIN_StringToUTF8W(info->szDevice));
#endif

    if (!WIN_GetDisplayMode(_this, hMonitor, info->szDevice, ENUM_CURRENT_SETTINGS, &mode, &natural_orientation, &current_orientation)) {
        return;
    }

    // Prevent adding duplicate displays. Do this after we know the display is
    // ready to be added to allow any displays that we can't fully query to be
    // removed
    for (i = 0; i < _this->num_displays; ++i) {
        SDL_DisplayData *driverdata = _this->displays[i]->driverdata;
        if (SDL_wcscmp(driverdata->DeviceName, info->szDevice) == 0) {
            SDL_bool moved = (index != i);
            SDL_bool changed_bounds = SDL_FALSE;

            if (driverdata->state != DisplayRemoved) {
                /* We've already enumerated this display, don't move it */
                return;
            }

            if (index >= _this->num_displays) {
                /* This should never happen due to the check above, but just in case... */
                return;
            }

            if (moved) {
                SDL_VideoDisplay *tmp;

                tmp = _this->displays[index];
                _this->displays[index] = _this->displays[i];
                _this->displays[i] = tmp;
                i = index;
            }

            driverdata->MonitorHandle = hMonitor;
            driverdata->state = DisplayUnchanged;

            if (!_this->setting_display_mode) {
                SDL_VideoDisplay *existing_display = _this->displays[i];
                SDL_Rect bounds;

                SDL_ResetFullscreenDisplayModes(existing_display);
                SDL_SetDesktopDisplayMode(existing_display, &mode);
                if (WIN_GetDisplayBounds(_this, existing_display, &bounds) == 0 &&
                    SDL_memcmp(&driverdata->bounds, &bounds, sizeof(bounds)) != 0) {
                    changed_bounds = SDL_TRUE;
                    SDL_copyp(&driverdata->bounds, &bounds);
                }
                if (moved || changed_bounds) {
                    SDL_SendDisplayEvent(existing_display, SDL_EVENT_DISPLAY_MOVED, 0);
                }
                SDL_SendDisplayEvent(existing_display, SDL_EVENT_DISPLAY_ORIENTATION, current_orientation);
                SDL_SetDisplayContentScale(existing_display, content_scale);
#ifdef HAVE_DXGI1_6_H
                SDL_HDROutputProperties HDR;
                WIN_GetHDRProperties(_this, hMonitor, &HDR);
                SDL_SetDisplayHDRProperties(existing_display, &HDR);
#endif
            }
            goto done;
        }
    }

    displaydata = (SDL_DisplayData *)SDL_calloc(1, sizeof(*displaydata));
    if (!displaydata) {
        return;
    }
    SDL_memcpy(displaydata->DeviceName, info->szDevice, sizeof(displaydata->DeviceName));
    displaydata->MonitorHandle = hMonitor;
    displaydata->state = DisplayAdded;

    SDL_zero(display);
    display.name = WIN_GetDisplayNameVista(_this->driverdata, info->szDevice);
    if (!display.name) {
        DISPLAY_DEVICEW device;
        SDL_zero(device);
        device.cb = sizeof(device);
        if (EnumDisplayDevicesW(info->szDevice, 0, &device, 0)) {
            display.name = WIN_StringToUTF8W(device.DeviceString);
        }
    }

    display.desktop_mode = mode;
    display.natural_orientation = natural_orientation;
    display.current_orientation = current_orientation;
    display.content_scale = content_scale;
    display.device = _this;
    display.driverdata = displaydata;
    WIN_GetDisplayBounds(_this, &display, &displaydata->bounds);
#ifdef HAVE_DXGI1_6_H
    WIN_GetHDRProperties(_this, hMonitor, &display.HDR);
#endif
    SDL_AddVideoDisplay(&display, SDL_FALSE);
    SDL_free(display.name);

done:
    *display_index += 1;
}

typedef struct _WIN_AddDisplaysData
{
    SDL_VideoDevice *video_device;
    int display_index;
    SDL_bool want_primary;
} WIN_AddDisplaysData;

static BOOL CALLBACK WIN_AddDisplaysCallback(HMONITOR hMonitor,
                                             HDC hdcMonitor,
                                             LPRECT lprcMonitor,
                                             LPARAM dwData)
{
    WIN_AddDisplaysData *data = (WIN_AddDisplaysData *)dwData;
    MONITORINFOEXW info;

    SDL_zero(info);
    info.cbSize = sizeof(info);

    if (GetMonitorInfoW(hMonitor, (LPMONITORINFO)&info) != 0) {
        const SDL_bool is_primary = ((info.dwFlags & MONITORINFOF_PRIMARY) == MONITORINFOF_PRIMARY);

        if (is_primary == data->want_primary) {
            WIN_AddDisplay(data->video_device, hMonitor, &info, &data->display_index);
        }
    }

    // continue enumeration
    return TRUE;
}

static void WIN_AddDisplays(SDL_VideoDevice *_this)
{
    WIN_AddDisplaysData callback_data;
    callback_data.video_device = _this;
    callback_data.display_index = 0;

    callback_data.want_primary = SDL_TRUE;
    EnumDisplayMonitors(NULL, NULL, WIN_AddDisplaysCallback, (LPARAM)&callback_data);

    callback_data.want_primary = SDL_FALSE;
    EnumDisplayMonitors(NULL, NULL, WIN_AddDisplaysCallback, (LPARAM)&callback_data);
}

int WIN_InitModes(SDL_VideoDevice *_this)
{
    WIN_AddDisplays(_this);

    if (_this->num_displays == 0) {
        return SDL_SetError("No displays available");
    }
    return 0;
}

int WIN_GetDisplayBounds(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    const SDL_DisplayData *data = display->driverdata;
    MONITORINFO minfo;
    BOOL rc;

    SDL_zero(minfo);
    minfo.cbSize = sizeof(MONITORINFO);
    rc = GetMonitorInfo(data->MonitorHandle, &minfo);

    if (!rc) {
        return SDL_SetError("Couldn't find monitor data");
    }

    rect->x = minfo.rcMonitor.left;
    rect->y = minfo.rcMonitor.top;
    rect->w = minfo.rcMonitor.right - minfo.rcMonitor.left;
    rect->h = minfo.rcMonitor.bottom - minfo.rcMonitor.top;

    return 0;
}

int WIN_GetDisplayUsableBounds(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    const SDL_DisplayData *data = display->driverdata;
    MONITORINFO minfo;
    BOOL rc;

    SDL_zero(minfo);
    minfo.cbSize = sizeof(MONITORINFO);
    rc = GetMonitorInfo(data->MonitorHandle, &minfo);

    if (!rc) {
        return SDL_SetError("Couldn't find monitor data");
    }

    rect->x = minfo.rcWork.left;
    rect->y = minfo.rcWork.top;
    rect->w = minfo.rcWork.right - minfo.rcWork.left;
    rect->h = minfo.rcWork.bottom - minfo.rcWork.top;

    return 0;
}

int WIN_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display)
{
    SDL_DisplayData *data = display->driverdata;
    DWORD i;
    SDL_DisplayMode mode;

    for (i = 0;; ++i) {
        if (!WIN_GetDisplayMode(_this, data->MonitorHandle, data->DeviceName, i, &mode, NULL, NULL)) {
            break;
        }
        if (SDL_ISPIXELFORMAT_INDEXED(mode.format)) {
            /* We don't support palettized modes now */
            SDL_free(mode.driverdata);
            continue;
        }
        if (mode.format != SDL_PIXELFORMAT_UNKNOWN) {
            if (!SDL_AddFullscreenDisplayMode(display, &mode)) {
                SDL_free(mode.driverdata);
            }
        } else {
            SDL_free(mode.driverdata);
        }
    }
    return 0;
}

#ifdef DEBUG_MODES
static void WIN_LogMonitor(SDL_VideoDevice *_this, HMONITOR mon)
{
    const SDL_VideoData *vid_data = (const SDL_VideoData *)_this->driverdata;
    MONITORINFOEX minfo;
    UINT xdpi = 0, ydpi = 0;
    char *name_utf8;

    if (vid_data->GetDpiForMonitor) {
        vid_data->GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
    }

    SDL_zero(minfo);
    minfo.cbSize = sizeof(minfo);
    GetMonitorInfo(mon, (LPMONITORINFO)&minfo);

    name_utf8 = WIN_StringToUTF8(minfo.szDevice);

    SDL_Log("WIN_LogMonitor: monitor \"%s\": dpi: %d windows screen coordinates: %d, %d, %dx%d",
            name_utf8,
            xdpi,
            minfo.rcMonitor.left,
            minfo.rcMonitor.top,
            minfo.rcMonitor.right - minfo.rcMonitor.left,
            minfo.rcMonitor.bottom - minfo.rcMonitor.top);

    SDL_free(name_utf8);
}
#endif

int WIN_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    SDL_DisplayData *displaydata = display->driverdata;
    SDL_DisplayModeData *data = (SDL_DisplayModeData *)mode->driverdata;
    LONG status;

#ifdef DEBUG_MODES
    SDL_Log("WIN_SetDisplayMode: monitor state before mode change:");
    WIN_LogMonitor(_this, displaydata->MonitorHandle);
#endif

    /* High-DPI notes:

       - ChangeDisplaySettingsEx always takes pixels.
       - e.g. if the display is set to 2880x1800 with 200% scaling in Display Settings
         - calling ChangeDisplaySettingsEx with a dmPelsWidth/Height other than 2880x1800 will
           change the monitor DPI to 96. (100% scaling)
         - calling ChangeDisplaySettingsEx with a dmPelsWidth/Height of 2880x1800 (or a NULL DEVMODE*) will
           reset the monitor DPI to 192. (200% scaling)

       NOTE: these are temporary changes in DPI, not modifications to the Control Panel setting. */
    if (mode->driverdata == display->desktop_mode.driverdata) {
#ifdef DEBUG_MODES
        SDL_Log("WIN_SetDisplayMode: resetting to original resolution");
#endif
        status = ChangeDisplaySettingsExW(displaydata->DeviceName, NULL, NULL, CDS_FULLSCREEN, NULL);
    } else {
#ifdef DEBUG_MODES
        SDL_Log("WIN_SetDisplayMode: changing to %dx%d pixels", data->DeviceMode.dmPelsWidth, data->DeviceMode.dmPelsHeight);
#endif
        status = ChangeDisplaySettingsExW(displaydata->DeviceName, &data->DeviceMode, NULL, CDS_FULLSCREEN, NULL);
    }
    if (status != DISP_CHANGE_SUCCESSFUL) {
        const char *reason = "Unknown reason";
        switch (status) {
        case DISP_CHANGE_BADFLAGS:
            reason = "DISP_CHANGE_BADFLAGS";
            break;
        case DISP_CHANGE_BADMODE:
            reason = "DISP_CHANGE_BADMODE";
            break;
        case DISP_CHANGE_BADPARAM:
            reason = "DISP_CHANGE_BADPARAM";
            break;
        case DISP_CHANGE_FAILED:
            reason = "DISP_CHANGE_FAILED";
            break;
        }
        return SDL_SetError("ChangeDisplaySettingsEx() failed: %s", reason);
    }

#ifdef DEBUG_MODES
    SDL_Log("WIN_SetDisplayMode: monitor state after mode change:");
    WIN_LogMonitor(_this, displaydata->MonitorHandle);
#endif

    EnumDisplaySettingsW(displaydata->DeviceName, ENUM_CURRENT_SETTINGS, &data->DeviceMode);
    WIN_UpdateDisplayMode(_this, displaydata->DeviceName, ENUM_CURRENT_SETTINGS, mode);
    return 0;
}

void WIN_RefreshDisplays(SDL_VideoDevice *_this)
{
    int i;

    // Mark all displays as potentially invalid to detect
    // entries that have actually been removed
    for (i = 0; i < _this->num_displays; ++i) {
        SDL_DisplayData *driverdata = _this->displays[i]->driverdata;
        driverdata->state = DisplayRemoved;
    }

    // Enumerate displays to add any new ones and mark still
    // connected entries as valid
    WIN_AddDisplays(_this);

    // Delete any entries still marked as invalid, iterate
    // in reverse as each delete takes effect immediately
    for (i = _this->num_displays - 1; i >= 0; --i) {
        SDL_VideoDisplay *display = _this->displays[i];
        SDL_DisplayData *driverdata = display->driverdata;
        if (driverdata->state == DisplayRemoved) {
            SDL_DelVideoDisplay(display->id, SDL_TRUE);
        }
    }

    // Send events for any newly added displays
    for (i = 0; i < _this->num_displays; ++i) {
        SDL_VideoDisplay *display = _this->displays[i];
        SDL_DisplayData *driverdata = display->driverdata;
        if (driverdata->state == DisplayAdded) {
            SDL_SendDisplayEvent(display, SDL_EVENT_DISPLAY_ADDED, 0);
        }
    }
}

void WIN_QuitModes(SDL_VideoDevice *_this)
{
    /* All fullscreen windows should have restored modes by now */
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */
