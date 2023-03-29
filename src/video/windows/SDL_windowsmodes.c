/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#if defined(SDL_VIDEO_DRIVER_WINDOWS) && !defined(__XBOXONE__) && !defined(__XBOXSERIES__)

#include "SDL_windowsvideo.h"
#include "../../events/SDL_displayevents_c.h"

/* Windows CE compatibility */
#ifndef CDS_FULLSCREEN
#define CDS_FULLSCREEN 0
#endif

/* #define DEBUG_MODES */
/* #define HIGHDPI_DEBUG_VERBOSE */

static void WIN_UpdateDisplayMode(_THIS, LPCWSTR deviceName, DWORD index, SDL_DisplayMode *mode)
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
                mode->format = SDL_PIXELFORMAT_RGB888;
                break;
            case 0x000000FF:
                mode->format = SDL_PIXELFORMAT_BGR888;
                break;
            case 0xF800:
                mode->format = SDL_PIXELFORMAT_RGB565;
                break;
            case 0x7C00:
                mode->format = SDL_PIXELFORMAT_RGB555;
                break;
            }
        } else if (bmi->bmiHeader.biBitCount == 8) {
            mode->format = SDL_PIXELFORMAT_INDEX8;
        } else if (bmi->bmiHeader.biBitCount == 4) {
            mode->format = SDL_PIXELFORMAT_INDEX4LSB;
        }
    } else if (mode->format == SDL_PIXELFORMAT_UNKNOWN) {
        /* FIXME: Can we tell what this will be? */
        if ((data->DeviceMode.dmFields & DM_BITSPERPEL) == DM_BITSPERPEL) {
            switch (data->DeviceMode.dmBitsPerPel) {
            case 32:
                mode->format = SDL_PIXELFORMAT_RGB888;
                break;
            case 24:
                mode->format = SDL_PIXELFORMAT_RGB24;
                break;
            case 16:
                mode->format = SDL_PIXELFORMAT_RGB565;
                break;
            case 15:
                mode->format = SDL_PIXELFORMAT_RGB555;
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

static SDL_DisplayOrientation WIN_GetDisplayOrientation(DEVMODE *mode)
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

static SDL_bool WIN_GetDisplayMode(_THIS, HMONITOR hMonitor, LPCWSTR deviceName, DWORD index, SDL_DisplayMode *mode, SDL_DisplayOrientation *orientation)
{
    const SDL_VideoData *videodata = (const SDL_VideoData *)_this->driverdata;
    SDL_DisplayModeData *data;
    DEVMODE devmode;

    devmode.dmSize = sizeof(devmode);
    devmode.dmDriverExtra = 0;
    if (!EnumDisplaySettingsW(deviceName, index, &devmode)) {
        return SDL_FALSE;
    }

    data = (SDL_DisplayModeData *)SDL_malloc(sizeof(*data));
    if (data == NULL) {
        return SDL_FALSE;
    }

    SDL_zerop(mode);
    mode->driverdata = data;
    data->DeviceMode = devmode;

    mode->format = SDL_PIXELFORMAT_UNKNOWN;
    mode->pixel_w = data->DeviceMode.dmPelsWidth;
    mode->pixel_h = data->DeviceMode.dmPelsHeight;
    mode->refresh_rate = WIN_GetRefreshRate(&data->DeviceMode);

    if (index == ENUM_CURRENT_SETTINGS && videodata->GetDpiForMonitor) {
        UINT hdpi_uint, vdpi_uint;
        if (videodata->GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &hdpi_uint, &vdpi_uint) == S_OK) {
            mode->display_scale = hdpi_uint / 96.0f;
        }
    }

    /* Fill in the mode information */
    WIN_UpdateDisplayMode(_this, deviceName, index, mode);

    if (orientation) {
        *orientation = WIN_GetDisplayOrientation(&devmode);
    }

    return SDL_TRUE;
}

/* The win32 API calls in this function require Windows Vista or later. */
/* *INDENT-OFF* */ /* clang-format off */
typedef LONG (WINAPI *SDL_WIN32PROC_GetDisplayConfigBufferSizes)(UINT32 flags, UINT32* numPathArrayElements, UINT32* numModeInfoArrayElements);
typedef LONG (WINAPI *SDL_WIN32PROC_QueryDisplayConfig)(UINT32 flags, UINT32* numPathArrayElements, DISPLAYCONFIG_PATH_INFO* pathArray, UINT32* numModeInfoArrayElements, DISPLAYCONFIG_MODE_INFO* modeInfoArray, DISPLAYCONFIG_TOPOLOGY_ID* currentTopologyId);
typedef LONG (WINAPI *SDL_WIN32PROC_DisplayConfigGetDeviceInfo)(DISPLAYCONFIG_DEVICE_INFO_HEADER* requestPacket);
/* *INDENT-ON* */ /* clang-format on */

static char *WIN_GetDisplayNameVista(const WCHAR *deviceName)
{
    void *dll;
    SDL_WIN32PROC_GetDisplayConfigBufferSizes pGetDisplayConfigBufferSizes;
    SDL_WIN32PROC_QueryDisplayConfig pQueryDisplayConfig;
    SDL_WIN32PROC_DisplayConfigGetDeviceInfo pDisplayConfigGetDeviceInfo;
    DISPLAYCONFIG_PATH_INFO *paths = NULL;
    DISPLAYCONFIG_MODE_INFO *modes = NULL;
    char *retval = NULL;
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    UINT32 i;
    LONG rc;

    dll = SDL_LoadObject("USER32.DLL");
    if (dll == NULL) {
        return NULL;
    }

    pGetDisplayConfigBufferSizes = (SDL_WIN32PROC_GetDisplayConfigBufferSizes)SDL_LoadFunction(dll, "GetDisplayConfigBufferSizes");
    pQueryDisplayConfig = (SDL_WIN32PROC_QueryDisplayConfig)SDL_LoadFunction(dll, "QueryDisplayConfig");
    pDisplayConfigGetDeviceInfo = (SDL_WIN32PROC_DisplayConfigGetDeviceInfo)SDL_LoadFunction(dll, "DisplayConfigGetDeviceInfo");

    if (pGetDisplayConfigBufferSizes == NULL || pQueryDisplayConfig == NULL || pDisplayConfigGetDeviceInfo == NULL) {
        goto WIN_GetDisplayNameVista_failed;
    }

    do {
        rc = pGetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
        if (rc != ERROR_SUCCESS) {
            goto WIN_GetDisplayNameVista_failed;
        }

        SDL_free(paths);
        SDL_free(modes);

        paths = (DISPLAYCONFIG_PATH_INFO *)SDL_malloc(sizeof(DISPLAYCONFIG_PATH_INFO) * pathCount);
        modes = (DISPLAYCONFIG_MODE_INFO *)SDL_malloc(sizeof(DISPLAYCONFIG_MODE_INFO) * modeCount);
        if ((paths == NULL) || (modes == NULL)) {
            goto WIN_GetDisplayNameVista_failed;
        }

        rc = pQueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths, &modeCount, modes, 0);
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
            rc = pDisplayConfigGetDeviceInfo(&sourceName.header);
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
            rc = pDisplayConfigGetDeviceInfo(&targetName.header);
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
    SDL_UnloadObject(dll);
    return retval;

WIN_GetDisplayNameVista_failed:
    SDL_free(retval);
    SDL_free(paths);
    SDL_free(modes);
    SDL_UnloadObject(dll);
    return NULL;
}

static void WIN_AddDisplay(_THIS, HMONITOR hMonitor, const MONITORINFOEXW *info, int *display_index, SDL_bool send_event)
{
    int i, index = *display_index;
    SDL_DisplayID displayID;
    SDL_VideoDisplay display;
    SDL_DisplayData *displaydata;
    SDL_DisplayMode mode;
    SDL_DisplayOrientation orientation;

#ifdef DEBUG_MODES
    SDL_Log("Display: %s\n", WIN_StringToUTF8W(info->szDevice));
#endif

    if (!WIN_GetDisplayMode(_this, hMonitor, info->szDevice, ENUM_CURRENT_SETTINGS, &mode, &orientation)) {
        return;
    }

    // Prevent adding duplicate displays. Do this after we know the display is
    // ready to be added to allow any displays that we can't fully query to be
    // removed
    for (i = 0; i < _this->num_displays; ++i) {
        SDL_DisplayData *driverdata = _this->displays[i].driverdata;
        if (SDL_wcscmp(driverdata->DeviceName, info->szDevice) == 0) {
            SDL_bool moved = (index != i);
            SDL_bool changed_bounds = SDL_FALSE;

            if (moved) {
                SDL_VideoDisplay tmp;

                SDL_assert(index < _this->num_displays);
                SDL_memcpy(&tmp, &_this->displays[index], sizeof(tmp));
                SDL_memcpy(&_this->displays[index], &_this->displays[i], sizeof(tmp));
                SDL_memcpy(&_this->displays[i], &tmp, sizeof(tmp));
                i = index;
            }

            driverdata->MonitorHandle = hMonitor;
            driverdata->IsValid = SDL_TRUE;

            if (!_this->setting_display_mode) {
                SDL_VideoDisplay *existing_display = &_this->displays[i];
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
                SDL_SendDisplayEvent(existing_display, SDL_EVENT_DISPLAY_ORIENTATION, orientation);
            }
            goto done;
        }
    }

    displaydata = (SDL_DisplayData *)SDL_calloc(1, sizeof(*displaydata));
    if (displaydata == NULL) {
        return;
    }
    SDL_memcpy(displaydata->DeviceName, info->szDevice, sizeof(displaydata->DeviceName));
    displaydata->MonitorHandle = hMonitor;
    displaydata->IsValid = SDL_TRUE;

    SDL_zero(display);
    display.name = WIN_GetDisplayNameVista(info->szDevice);
    if (display.name == NULL) {
        DISPLAY_DEVICEW device;
        SDL_zero(device);
        device.cb = sizeof(device);
        if (EnumDisplayDevicesW(info->szDevice, 0, &device, 0)) {
            display.name = WIN_StringToUTF8W(device.DeviceString);
        }
    }

    display.desktop_mode = mode;
    display.orientation = orientation;
    display.device = _this;
    display.driverdata = displaydata;
    WIN_GetDisplayBounds(_this, &display, &displaydata->bounds);
    displayID = SDL_AddVideoDisplay(&display, send_event);
    SDL_assert(SDL_GetDisplayIndex(displayID) == *display_index);
    SDL_free(display.name);

done:
    *display_index += 1;
}

typedef struct _WIN_AddDisplaysData
{
    SDL_VideoDevice *video_device;
    int display_index;
    SDL_bool send_event;
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
            WIN_AddDisplay(data->video_device, hMonitor, &info, &data->display_index, data->send_event);
        }
    }

    // continue enumeration
    return TRUE;
}

static void WIN_AddDisplays(_THIS, SDL_bool send_event)
{
    WIN_AddDisplaysData callback_data;
    callback_data.video_device = _this;
    callback_data.display_index = 0;
    callback_data.send_event = send_event;

    callback_data.want_primary = SDL_TRUE;
    EnumDisplayMonitors(NULL, NULL, WIN_AddDisplaysCallback, (LPARAM)&callback_data);

    callback_data.want_primary = SDL_FALSE;
    EnumDisplayMonitors(NULL, NULL, WIN_AddDisplaysCallback, (LPARAM)&callback_data);
}

int WIN_InitModes(_THIS)
{
    WIN_AddDisplays(_this, SDL_FALSE);

    if (_this->num_displays == 0) {
        return SDL_SetError("No displays available");
    }
    return 0;
}

/**
 * Convert the monitor rect and work rect from pixels to the SDL coordinate system (monitor origins are in pixels,
 * monitor size in DPI-scaled points).
 *
 * No-op if DPI scaling is not enabled.
 */
static void WIN_MonitorInfoToSDL(const SDL_VideoData *videodata, HMONITOR monitor, MONITORINFO *info)
{
    UINT xdpi, ydpi;

    if (!videodata->dpi_scaling_enabled) {
        return;
    }

    /* Check for Windows < 8.1*/
    if (!videodata->GetDpiForMonitor) {
        return;
    }
    if (videodata->GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi) != S_OK) {
        /* Shouldn't happen? */
        return;
    }

    /* Convert monitor size to points, leaving the monitor position in pixels */
    info->rcMonitor.right = info->rcMonitor.left + MulDiv(info->rcMonitor.right - info->rcMonitor.left, 96, xdpi);
    info->rcMonitor.bottom = info->rcMonitor.top + MulDiv(info->rcMonitor.bottom - info->rcMonitor.top, 96, ydpi);

    /* Convert monitor work rect to points */
    info->rcWork.left = info->rcMonitor.left + MulDiv(info->rcWork.left - info->rcMonitor.left, 96, xdpi);
    info->rcWork.right = info->rcMonitor.left + MulDiv(info->rcWork.right - info->rcMonitor.left, 96, xdpi);
    info->rcWork.top = info->rcMonitor.top + MulDiv(info->rcWork.top - info->rcMonitor.top, 96, ydpi);
    info->rcWork.bottom = info->rcMonitor.top + MulDiv(info->rcWork.bottom - info->rcMonitor.top, 96, ydpi);
}

int WIN_GetDisplayBounds(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    const SDL_DisplayData *data = display->driverdata;
    const SDL_VideoData *videodata = display->device->driverdata;
    MONITORINFO minfo;
    BOOL rc;

    SDL_zero(minfo);
    minfo.cbSize = sizeof(MONITORINFO);
    rc = GetMonitorInfo(data->MonitorHandle, &minfo);

    if (!rc) {
        return SDL_SetError("Couldn't find monitor data");
    }

    WIN_MonitorInfoToSDL(videodata, data->MonitorHandle, &minfo);
    rect->x = minfo.rcMonitor.left;
    rect->y = minfo.rcMonitor.top;
    rect->w = minfo.rcMonitor.right - minfo.rcMonitor.left;
    rect->h = minfo.rcMonitor.bottom - minfo.rcMonitor.top;

    return 0;
}

static int WIN_GetDisplayDPI(SDL_DisplayID displayID, int *dpi)
{
    const SDL_VideoDisplay *display = SDL_GetVideoDisplay(displayID);
    const SDL_DisplayData *displaydata = display ? display->driverdata : NULL;
    const SDL_VideoData *videodata = display ? display->device->driverdata : NULL;

    *dpi = 0;

    if (videodata && videodata->GetDpiForMonitor) {
        /* Windows 8.1 + codepath */
        UINT xdpi, ydpi;
        videodata->GetDpiForMonitor(displaydata->MonitorHandle, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
        *dpi = (int)xdpi; /* xdpi and hdpi are the same value */
    }
    if (*dpi == 0) {
        /* Window 8.0 and below: same DPI for all monitors */
        HDC hdc = GetDC(NULL);
        if (hdc) {
            *dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(NULL, hdc);
        }
    }
    if (*dpi == 0) {
        return SDL_SetError("Couldn't get display DPI");
    }
    return 0;
}

int WIN_GetDisplayUsableBounds(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    const SDL_DisplayData *data = display->driverdata;
    const SDL_VideoData *videodata = display->device->driverdata;
    MONITORINFO minfo;
    BOOL rc;

    SDL_zero(minfo);
    minfo.cbSize = sizeof(MONITORINFO);
    rc = GetMonitorInfo(data->MonitorHandle, &minfo);

    if (!rc) {
        return SDL_SetError("Couldn't find monitor data");
    }

    WIN_MonitorInfoToSDL(videodata, data->MonitorHandle, &minfo);
    rect->x = minfo.rcWork.left;
    rect->y = minfo.rcWork.top;
    rect->w = minfo.rcWork.right - minfo.rcWork.left;
    rect->h = minfo.rcWork.bottom - minfo.rcWork.top;

    return 0;
}

/**
 * Convert a point from the SDL coordinate system (monitor origins are in pixels,
 * offset within a monitor in DPI-scaled points) to Windows virtual screen coordinates (pixels).
 *
 * No-op if DPI scaling is not enabled (returns 96 dpi).
 *
 * Returns the DPI of the monitor that was closest to x, y and used for the conversion.
 */
void WIN_ScreenPointFromSDL(int *x, int *y, int *dpiOut)
{
    POINT pt = { 0, 0 };
    WIN_ScreenPointFromSDLFloat((float)*x, (float)*y, &pt.x, &pt.y, dpiOut);
    *x = pt.x;
    *y = pt.y;
}

void WIN_ScreenPointFromSDLFloat(float x, float y, LONG *xOut, LONG *yOut, int *dpiOut)
{
    const SDL_VideoDevice *videodevice = SDL_GetVideoDevice();
    const SDL_VideoData *videodata;
    SDL_DisplayID displayID;
    SDL_Rect bounds;
    int dpi;
    SDL_Point point;

    point.x = (int)x;
    point.y = (int)y;

    if (dpiOut) {
        *dpiOut = 96;
    }

    if (videodevice == NULL || !videodevice->driverdata) {
        goto passthrough;
    }

    videodata = videodevice->driverdata;
    if (!videodata->dpi_scaling_enabled) {
        goto passthrough;
    }

    /* Can't use MonitorFromPoint for this because we currently have SDL coordinates, not pixels */
    displayID = SDL_GetDisplayForPoint(&point);
    if (displayID == 0) {
        goto passthrough;
    }

    if (SDL_GetDisplayBounds(displayID, &bounds) < 0 || WIN_GetDisplayDPI(displayID, &dpi) < 0) {
        goto passthrough;
    }

    if (dpiOut) {
        *dpiOut = dpi;
    }

    /* Undo the DPI-scaling within the monitor bounds to convert back to pixels */
    *xOut = bounds.x + SDL_lroundf(((x - bounds.x) * dpi) / 96.0f);
    *yOut = bounds.y + SDL_lroundf(((y - bounds.y) * dpi) / 96.0f);

#ifdef HIGHDPI_DEBUG_VERBOSE
    SDL_Log("WIN_ScreenPointFromSDL: (%g, %g) points -> (%d x %d) pixels, using %g DPI monitor",
            x, y, *xOut, *yOut, ddpi);
#endif
    return;

passthrough:
    *xOut = SDL_lroundf(x);
    *yOut = SDL_lroundf(y);
}

/**
 * Convert a point from Windows virtual screen coordinates (pixels) to the SDL
 * coordinate system (monitor origins are in pixels, offset within a monitor in DPI-scaled points).
 *
 * No-op if DPI scaling is not enabled.
 */
void WIN_ScreenPointToSDL(int *x, int *y)
{
    SDL_FPoint pt;
    WIN_ScreenPointToSDLFloat(*x, *y, &pt.x, &pt.y);
    *x = SDL_lroundf(pt.x);
    *y = SDL_lroundf(pt.y);
}

void WIN_ScreenPointToSDLFloat(LONG x, LONG y, float *xOut, float *yOut)
{
    const SDL_VideoDevice *videodevice = SDL_GetVideoDevice();
    const SDL_VideoData *videodata;
    POINT point;
    HMONITOR monitor;
    int i;
    SDL_DisplayID displayID;
    SDL_Rect bounds;
    int dpi;

    if (videodevice == NULL || !videodevice->driverdata) {
        return;
    }

    videodata = videodevice->driverdata;
    if (!videodata->dpi_scaling_enabled) {
        *xOut = (float)x;
        *yOut = (float)y;
        return;
    }

    point.x = x;
    point.y = y;
    monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);

    /* Search for the corresponding SDL monitor */
    displayID = 0;
    for (i = 0; i < videodevice->num_displays; ++i) {
        SDL_DisplayData *driverdata = videodevice->displays[i].driverdata;
        if (driverdata->MonitorHandle == monitor) {
            displayID = videodevice->displays[i].id;
        }
    }
    if (displayID == 0) {
        return;
    }

    /* Get SDL display properties */
    if (SDL_GetDisplayBounds(displayID, &bounds) < 0 || WIN_GetDisplayDPI(displayID, &dpi) < 0) {
        return;
    }

    /* Convert the point's offset within the monitor from pixels to DPI-scaled points */
    *xOut = (float)bounds.x + ((float)(x - bounds.x) * 96.0f) / dpi;
    *yOut = (float)bounds.y + ((float)(y - bounds.y) * 96.0f) / dpi;

#ifdef HIGHDPI_DEBUG_VERBOSE
    SDL_Log("WIN_ScreenPointToSDL: (%d, %d) pixels -> (%g x %g) points, using %g DPI monitor",
            x, y, *xOut, *yOut, ddpi);
#endif
}

int WIN_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
    SDL_DisplayData *data = display->driverdata;
    DWORD i;
    SDL_DisplayMode mode;

    for (i = 0;; ++i) {
        if (!WIN_GetDisplayMode(_this, data->MonitorHandle, data->DeviceName, i, &mode, NULL)) {
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
static void WIN_LogMonitor(_THIS, HMONITOR mon)
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

int WIN_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    SDL_DisplayData *displaydata = display->driverdata;
    SDL_DisplayModeData *data = mode->driverdata;
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

void WIN_RefreshDisplays(_THIS)
{
    int i;

    // Mark all displays as potentially invalid to detect
    // entries that have actually been removed
    for (i = 0; i < _this->num_displays; ++i) {
        SDL_DisplayData *driverdata = _this->displays[i].driverdata;
        driverdata->IsValid = SDL_FALSE;
    }

    // Enumerate displays to add any new ones and mark still
    // connected entries as valid
    WIN_AddDisplays(_this, SDL_TRUE);

    // Delete any entries still marked as invalid, iterate
    // in reverse as each delete takes effect immediately
    for (i = _this->num_displays - 1; i >= 0; --i) {
        SDL_VideoDisplay *display = &_this->displays[i];
        SDL_DisplayData *driverdata = display->driverdata;
        if (driverdata->IsValid == SDL_FALSE) {
            SDL_DelVideoDisplay(display->id, SDL_TRUE);
        }
    }
}

void WIN_QuitModes(_THIS)
{
    /* All fullscreen windows should have restored modes by now */
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */
