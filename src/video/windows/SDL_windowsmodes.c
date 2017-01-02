/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_WINDOWS

#include "SDL_windowsvideo.h"
#include "../../../include/SDL_assert.h"

/* Windows CE compatibility */
#ifndef CDS_FULLSCREEN
#define CDS_FULLSCREEN 0
#endif

typedef struct _WIN_GetMonitorDPIData {
    SDL_VideoData *vid_data;
    SDL_DisplayMode *mode;
    SDL_DisplayModeData *mode_data;
} WIN_GetMonitorDPIData;

static BOOL CALLBACK
WIN_GetMonitorDPI(HMONITOR hMonitor,
                  HDC      hdcMonitor,
                  LPRECT   lprcMonitor,
                  LPARAM   dwData)
{
    WIN_GetMonitorDPIData *data = (WIN_GetMonitorDPIData*) dwData;
    UINT hdpi, vdpi;

    if (data->vid_data->GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &hdpi, &vdpi) == S_OK &&
        hdpi > 0 &&
        vdpi > 0) {
        float hsize, vsize;
        
        data->mode_data->HorzDPI = (float)hdpi;
        data->mode_data->VertDPI = (float)vdpi;

        // Figure out the monitor size and compute the diagonal DPI.
        hsize = data->mode->w / data->mode_data->HorzDPI;
        vsize = data->mode->h / data->mode_data->VertDPI;
        
        data->mode_data->DiagDPI = SDL_ComputeDiagonalDPI( data->mode->w,
                                                           data->mode->h,
                                                           hsize,
                                                           vsize );

        // We can only handle one DPI per display mode so end the enumeration.
        return FALSE;
    }

    // We didn't get DPI information so keep going.
    return TRUE;
}

static void
WIN_UpdateDisplayMode(_THIS, LPCTSTR deviceName, DWORD index, SDL_DisplayMode * mode)
{
    SDL_VideoData *vid_data = (SDL_VideoData *) _this->driverdata;
    SDL_DisplayModeData *data = (SDL_DisplayModeData *) mode->driverdata;
    HDC hdc;

    data->DeviceMode.dmFields =
        (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY |
         DM_DISPLAYFLAGS);

    if (index == ENUM_CURRENT_SETTINGS
        && (hdc = CreateDC(deviceName, NULL, NULL, NULL)) != NULL) {
        char bmi_data[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
        LPBITMAPINFO bmi;
        HBITMAP hbm;
        int logical_width = GetDeviceCaps( hdc, HORZRES );
        int logical_height = GetDeviceCaps( hdc, VERTRES );

        data->ScaleX = (float)logical_width / data->DeviceMode.dmPelsWidth;
        data->ScaleY = (float)logical_height / data->DeviceMode.dmPelsHeight;
        mode->w = logical_width;
        mode->h = logical_height;

        // WIN_GetMonitorDPI needs mode->w and mode->h
        // so only call after those are set.
        if (vid_data->GetDpiForMonitor) {
            WIN_GetMonitorDPIData dpi_data;
            RECT monitor_rect;

            dpi_data.vid_data = vid_data;
            dpi_data.mode = mode;
            dpi_data.mode_data = data;
            monitor_rect.left = data->DeviceMode.dmPosition.x;
            monitor_rect.top = data->DeviceMode.dmPosition.y;
            monitor_rect.right = monitor_rect.left + 1;
            monitor_rect.bottom = monitor_rect.top + 1;
            EnumDisplayMonitors(NULL, &monitor_rect, WIN_GetMonitorDPI, (LPARAM)&dpi_data);
        } else {
            // We don't have the Windows 8.1 routine so just
            // get system DPI.
            data->HorzDPI = (float)GetDeviceCaps( hdc, LOGPIXELSX );
            data->VertDPI = (float)GetDeviceCaps( hdc, LOGPIXELSY );
            if (data->HorzDPI == data->VertDPI) {
                data->DiagDPI = data->HorzDPI;
            } else {
                data->DiagDPI = SDL_ComputeDiagonalDPI( mode->w,
                                                        mode->h,
                                                        (float)GetDeviceCaps( hdc, HORZSIZE ) / 25.4f,
                                                        (float)GetDeviceCaps( hdc, VERTSIZE ) / 25.4f );
            }
        }
        
        SDL_zero(bmi_data);
        bmi = (LPBITMAPINFO) bmi_data;
        bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

        hbm = CreateCompatibleBitmap(hdc, 1, 1);
        GetDIBits(hdc, hbm, 0, 1, NULL, bmi, DIB_RGB_COLORS);
        GetDIBits(hdc, hbm, 0, 1, NULL, bmi, DIB_RGB_COLORS);
        DeleteObject(hbm);
        DeleteDC(hdc);
        if (bmi->bmiHeader.biCompression == BI_BITFIELDS) {
            switch (*(Uint32 *) bmi->bmiColors) {
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

static SDL_bool
WIN_GetDisplayMode(_THIS, LPCTSTR deviceName, DWORD index, SDL_DisplayMode * mode)
{
    SDL_DisplayModeData *data;
    DEVMODE devmode;

    devmode.dmSize = sizeof(devmode);
    devmode.dmDriverExtra = 0;
    if (!EnumDisplaySettings(deviceName, index, &devmode)) {
        return SDL_FALSE;
    }

    data = (SDL_DisplayModeData *) SDL_malloc(sizeof(*data));
    if (!data) {
        return SDL_FALSE;
    }

    mode->driverdata = data;
    data->DeviceMode = devmode;

    /* Default basic information */
    data->ScaleX = 1.0f;
    data->ScaleY = 1.0f;
    data->DiagDPI = 0.0f;
    data->HorzDPI = 0.0f;
    data->VertDPI = 0.0f;

    mode->format = SDL_PIXELFORMAT_UNKNOWN;
    mode->w = data->DeviceMode.dmPelsWidth;
    mode->h = data->DeviceMode.dmPelsHeight;
    mode->refresh_rate = data->DeviceMode.dmDisplayFrequency;

    /* Fill in the mode information */
    WIN_UpdateDisplayMode(_this, deviceName, index, mode);
    return SDL_TRUE;
}

static SDL_bool
WIN_AddDisplay(_THIS, LPTSTR DeviceName)
{
    SDL_VideoDisplay display;
    SDL_DisplayData *displaydata;
    SDL_DisplayMode mode;
    DISPLAY_DEVICE device;

#ifdef DEBUG_MODES
    printf("Display: %s\n", WIN_StringToUTF8(DeviceName));
#endif
    if (!WIN_GetDisplayMode(_this, DeviceName, ENUM_CURRENT_SETTINGS, &mode)) {
        return SDL_FALSE;
    }

    displaydata = (SDL_DisplayData *) SDL_malloc(sizeof(*displaydata));
    if (!displaydata) {
        return SDL_FALSE;
    }
    SDL_memcpy(displaydata->DeviceName, DeviceName,
               sizeof(displaydata->DeviceName));

    SDL_zero(display);
    device.cb = sizeof(device);
    if (EnumDisplayDevices(DeviceName, 0, &device, 0)) {
        display.name = WIN_StringToUTF8(device.DeviceString);
    }
    display.desktop_mode = mode;
    display.current_mode = mode;
    display.driverdata = displaydata;
    SDL_AddVideoDisplay(&display);
    SDL_free(display.name);
    return SDL_TRUE;
}

int
WIN_InitModes(_THIS)
{
    int pass;
    DWORD i, j, count;
    DISPLAY_DEVICE device;

    device.cb = sizeof(device);

    /* Get the primary display in the first pass */
    for (pass = 0; pass < 2; ++pass) {
        for (i = 0; ; ++i) {
            TCHAR DeviceName[32];

            if (!EnumDisplayDevices(NULL, i, &device, 0)) {
                break;
            }
            if (!(device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
                continue;
            }
            if (pass == 0) {
                if (!(device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) {
                    continue;
                }
            } else {
                if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                    continue;
                }
            }
            SDL_memcpy(DeviceName, device.DeviceName, sizeof(DeviceName));
#ifdef DEBUG_MODES
            printf("Device: %s\n", WIN_StringToUTF8(DeviceName));
#endif
            count = 0;
            for (j = 0; ; ++j) {
                if (!EnumDisplayDevices(DeviceName, j, &device, 0)) {
                    break;
                }
                if (!(device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
                    continue;
                }
                if (pass == 0) {
                    if (!(device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) {
                        continue;
                    }
                } else {
                    if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
                        continue;
                    }
                }
                count += WIN_AddDisplay(_this, device.DeviceName);
            }
            if (count == 0) {
                WIN_AddDisplay(_this, DeviceName);
            }
        }
    }
    if (_this->num_displays == 0) {
        return SDL_SetError("No displays available");
    }
    return 0;
}

int
WIN_GetDisplayBounds(_THIS, SDL_VideoDisplay * display, SDL_Rect * rect)
{
    SDL_DisplayModeData *data = (SDL_DisplayModeData *) display->current_mode.driverdata;

    rect->x = (int)SDL_ceil(data->DeviceMode.dmPosition.x * data->ScaleX);
    rect->y = (int)SDL_ceil(data->DeviceMode.dmPosition.y * data->ScaleY);
    rect->w = (int)SDL_ceil(data->DeviceMode.dmPelsWidth * data->ScaleX);
    rect->h = (int)SDL_ceil(data->DeviceMode.dmPelsHeight * data->ScaleY);

    return 0;
}

int
WIN_GetDisplayDPI(_THIS, SDL_VideoDisplay * display, float * ddpi, float * hdpi, float * vdpi)
{
    SDL_DisplayModeData *data = (SDL_DisplayModeData *) display->current_mode.driverdata;

    if (ddpi) {
        *ddpi = data->DiagDPI;
    }
    if (hdpi) {
        *hdpi = data->HorzDPI;
    }
    if (vdpi) {
        *vdpi = data->VertDPI;
    }

    return data->DiagDPI != 0.0f ? 0 : SDL_SetError("Couldn't get DPI");
}

int
WIN_GetDisplayUsableBounds(_THIS, SDL_VideoDisplay * display, SDL_Rect * rect)
{
    const SDL_DisplayModeData *data = (const SDL_DisplayModeData *) display->current_mode.driverdata;
    const DEVMODE *pDevMode = &data->DeviceMode;
    POINT pt = {
        /* !!! FIXME: no scale, right? */
        (LONG) (pDevMode->dmPosition.x + (pDevMode->dmPelsWidth / 2)),
        (LONG) (pDevMode->dmPosition.y + (pDevMode->dmPelsHeight / 2))
    };
    HMONITOR hmon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
    MONITORINFO minfo;
    const RECT *work;
    BOOL rc = FALSE;

    SDL_assert(hmon != NULL);

    if (hmon != NULL) {
        SDL_zero(minfo);
        minfo.cbSize = sizeof (MONITORINFO);
        rc = GetMonitorInfo(hmon, &minfo);
        SDL_assert(rc);
    }

    if (!rc) {
        return SDL_SetError("Couldn't find monitor data");
    }

    work = &minfo.rcWork;
    rect->x = (int)SDL_ceil(work->left * data->ScaleX);
    rect->y = (int)SDL_ceil(work->top * data->ScaleY);
    rect->w = (int)SDL_ceil((work->right - work->left) * data->ScaleX);
    rect->h = (int)SDL_ceil((work->bottom - work->top) * data->ScaleY);

    return 0;
}

void
WIN_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{
    SDL_DisplayData *data = (SDL_DisplayData *) display->driverdata;
    DWORD i;
    SDL_DisplayMode mode;

    for (i = 0;; ++i) {
        if (!WIN_GetDisplayMode(_this, data->DeviceName, i, &mode)) {
            break;
        }
        if (SDL_ISPIXELFORMAT_INDEXED(mode.format)) {
            /* We don't support palettized modes now */
            SDL_free(mode.driverdata);
            continue;
        }
        if (mode.format != SDL_PIXELFORMAT_UNKNOWN) {
            if (!SDL_AddDisplayMode(display, &mode)) {
                SDL_free(mode.driverdata);
            }
        } else {
            SDL_free(mode.driverdata);
        }
    }
}

int
WIN_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    SDL_DisplayData *displaydata = (SDL_DisplayData *) display->driverdata;
    SDL_DisplayModeData *data = (SDL_DisplayModeData *) mode->driverdata;
    LONG status;

    if (mode->driverdata == display->desktop_mode.driverdata) {
        status = ChangeDisplaySettingsEx(displaydata->DeviceName, NULL, NULL, CDS_FULLSCREEN, NULL);
    } else {
        status = ChangeDisplaySettingsEx(displaydata->DeviceName, &data->DeviceMode, NULL, CDS_FULLSCREEN, NULL);
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
    EnumDisplaySettings(displaydata->DeviceName, ENUM_CURRENT_SETTINGS, &data->DeviceMode);
    WIN_UpdateDisplayMode(_this, displaydata->DeviceName, ENUM_CURRENT_SETTINGS, mode);
    return 0;
}

void
WIN_QuitModes(_THIS)
{
    /* All fullscreen windows should have restored modes by now */
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */

/* vi: set ts=4 sw=4 expandtab: */
