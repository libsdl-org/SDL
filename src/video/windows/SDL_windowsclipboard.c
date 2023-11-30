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
#include "SDL_windowswindow.h"
#include "../SDL_clipboard_c.h"
#include "../../events/SDL_clipboardevents_c.h"

#ifdef UNICODE
#define TEXT_FORMAT CF_UNICODETEXT
#else
#define TEXT_FORMAT CF_TEXT
#endif

#define IMAGE_FORMAT CF_DIB
#define IMAGE_MIME_TYPE "image/bmp"
#define BFT_BITMAP 0x4d42 /* 'BM' */

/* Assume we can directly read and write BMP fields without byte swapping */
SDL_COMPILE_TIME_ASSERT(verify_byte_order, SDL_BYTEORDER == SDL_LIL_ENDIAN);

static BOOL WIN_OpenClipboard(SDL_VideoDevice *_this)
{
    /* Retry to open the clipboard in case another application has it open */
    const int MAX_ATTEMPTS = 3;
    int attempt;
    HWND hwnd = NULL;

    if (_this->windows) {
        hwnd = _this->windows->driverdata->hwnd;
    }
    for (attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        if (OpenClipboard(hwnd)) {
            return TRUE;
        }
        SDL_Delay(10);
    }
    return FALSE;
}

static void WIN_CloseClipboard(void)
{
    CloseClipboard();
}

static HANDLE WIN_ConvertBMPtoDIB(const void *bmp, size_t bmp_size)
{
    HANDLE hMem = NULL;

    if (bmp && bmp_size > sizeof(BITMAPFILEHEADER) && ((BITMAPFILEHEADER *)bmp)->bfType == BFT_BITMAP) {
        BITMAPFILEHEADER *pbfh = (BITMAPFILEHEADER *)bmp;
        BITMAPINFOHEADER *pbih = (BITMAPINFOHEADER *)((Uint8 *)bmp + sizeof(BITMAPFILEHEADER));
        size_t bih_size = pbih->biSize + pbih->biClrUsed * sizeof(RGBQUAD);
        size_t pixels_size = pbih->biSizeImage;

        if (pbfh->bfOffBits >= (sizeof(BITMAPFILEHEADER) + bih_size) &&
            (pbfh->bfOffBits + pixels_size) <= bmp_size) {
            const Uint8 *pixels = (const Uint8 *)bmp + pbfh->bfOffBits;
            size_t dib_size = bih_size + pixels_size;
            hMem = GlobalAlloc(GMEM_MOVEABLE, dib_size);
            if (hMem) {
                LPVOID dst = GlobalLock(hMem);
                if (dst) {
                    SDL_memcpy(dst, pbih, bih_size);
                    SDL_memcpy((Uint8 *)dst + bih_size, pixels, pixels_size);
                    GlobalUnlock(hMem);
                } else {
                    WIN_SetError("GlobalLock()");
                    GlobalFree(hMem);
                    hMem = NULL;
                }
            } else {
                SDL_OutOfMemory();
            }
        } else {
            SDL_SetError("Invalid BMP data");
        }
    } else {
        SDL_SetError("Invalid BMP data");
    }
    return hMem;
}

static void *WIN_ConvertDIBtoBMP(HANDLE hMem, size_t *size)
{
    void *bmp = NULL;
    size_t mem_size = GlobalSize(hMem);

    if (mem_size > sizeof(BITMAPINFOHEADER)) {
        LPVOID dib = GlobalLock(hMem);
        if (dib) {
            BITMAPINFOHEADER *pbih = (BITMAPINFOHEADER *)dib;
            size_t bih_size = pbih->biSize + pbih->biClrUsed * sizeof(RGBQUAD);
            size_t dib_size = bih_size + pbih->biSizeImage;
            if (dib_size <= mem_size) {
                size_t bmp_size = sizeof(BITMAPFILEHEADER) + dib_size;
                bmp = SDL_malloc(bmp_size);
                if (bmp) {
                    BITMAPFILEHEADER *pbfh = (BITMAPFILEHEADER *)bmp;
                    pbfh->bfType = BFT_BITMAP;
                    pbfh->bfSize = (DWORD)bmp_size;
                    pbfh->bfReserved1 = 0;
                    pbfh->bfReserved2 = 0;
                    pbfh->bfOffBits = (DWORD)(sizeof(BITMAPFILEHEADER) + bih_size);
                    SDL_memcpy((Uint8 *)bmp + sizeof(BITMAPFILEHEADER), dib, dib_size);
                    *size = bmp_size;
                }
            } else {
                SDL_SetError("Invalid BMP data");
            }
            GlobalUnlock(hMem);
        } else {
            WIN_SetError("GlobalLock()");
        }
    } else {
        SDL_SetError("Invalid BMP data");
    }
    return bmp;
}

static int WIN_SetClipboardImage(SDL_VideoDevice *_this)
{
    HANDLE hMem;
    size_t clipboard_data_size;
    const void *clipboard_data;
    int result = 0;

    clipboard_data = _this->clipboard_callback(_this->clipboard_userdata, IMAGE_MIME_TYPE, &clipboard_data_size);
    hMem = WIN_ConvertBMPtoDIB(clipboard_data, clipboard_data_size);
    if (hMem) {
        /* Save the image to the clipboard */
        if (!SetClipboardData(IMAGE_FORMAT, hMem)) {
            result = WIN_SetError("Couldn't set clipboard data");
        }
    } else {
        /* WIN_ConvertBMPtoDIB() set the error */
        result = -1;
    }
    return result;
}

static int WIN_SetClipboardText(SDL_VideoDevice *_this, const char *mime_type)
{
    HANDLE hMem;
    size_t clipboard_data_size;
    const void *clipboard_data;
    int result = 0;

    clipboard_data = _this->clipboard_callback(_this->clipboard_userdata, mime_type, &clipboard_data_size);
    if (clipboard_data && clipboard_data_size > 0) {
        SIZE_T i, size;
        LPTSTR tstr = (WCHAR *)SDL_iconv_string("UTF-16LE", "UTF-8", (const char *)clipboard_data, clipboard_data_size);
        if (!tstr) {
            return SDL_SetError("Couldn't convert text from UTF-8");
        }

        /* Find out the size of the data */
        for (size = 0, i = 0; tstr[i]; ++i, ++size) {
            if (tstr[i] == '\n' && (i == 0 || tstr[i - 1] != '\r')) {
                /* We're going to insert a carriage return */
                ++size;
            }
        }
        size = (size + 1) * sizeof(*tstr);

        /* Save the data to the clipboard */
        hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hMem) {
            LPTSTR dst = (LPTSTR)GlobalLock(hMem);
            if (dst) {
                /* Copy the text over, adding carriage returns as necessary */
                for (i = 0; tstr[i]; ++i) {
                    if (tstr[i] == '\n' && (i == 0 || tstr[i - 1] != '\r')) {
                        *dst++ = '\r';
                    }
                    *dst++ = tstr[i];
                }
                *dst = 0;
                GlobalUnlock(hMem);
            }

            if (!SetClipboardData(TEXT_FORMAT, hMem)) {
                result = WIN_SetError("Couldn't set clipboard data");
            }
        } else {
            result = SDL_OutOfMemory();
        }
        SDL_free(tstr);
    }
    return result;
}

int WIN_SetClipboardData(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->driverdata;
    size_t i;
    int result = 0;

    /* I investigated delayed clipboard rendering, and at least with text and image
     * formats you have to use an output window, not SDL_HelperWindow, and the system
     * requests them being rendered immediately, so there isn't any benefit.
     */

    if (WIN_OpenClipboard(_this)) {
        EmptyClipboard();

        /* Set the clipboard text */
        for (i = 0; i < _this->num_clipboard_mime_types; ++i) {
            const char *mime_type = _this->clipboard_mime_types[i];

            if (SDL_IsTextMimeType(mime_type)) {
                if (WIN_SetClipboardText(_this, mime_type) < 0) {
                    result = -1;
                }
                /* Only set the first clipboard text */
                break;
            }
        }

        /* Set the clipboard image */
        for (i = 0; i < _this->num_clipboard_mime_types; ++i) {
            const char *mime_type = _this->clipboard_mime_types[i];

            if (SDL_strcmp(mime_type, IMAGE_MIME_TYPE) == 0) {
                if (WIN_SetClipboardImage(_this) < 0) {
                    result = -1;
                }
                break;
            }
        }

        data->clipboard_count = GetClipboardSequenceNumber();
        WIN_CloseClipboard();
    } else {
        result = WIN_SetError("Couldn't open clipboard");
    }
    return result;
}

void *WIN_GetClipboardData(SDL_VideoDevice *_this, const char *mime_type, size_t *size)
{
    void *data = NULL;

    if (SDL_IsTextMimeType(mime_type)) {
        char *text = NULL;

        if (IsClipboardFormatAvailable(TEXT_FORMAT)) {
            if (WIN_OpenClipboard(_this)) {
                HANDLE hMem;
                LPTSTR tstr;

                hMem = GetClipboardData(TEXT_FORMAT);
                if (hMem) {
                    tstr = (LPTSTR)GlobalLock(hMem);
                    if (tstr) {
                        text = WIN_StringToUTF8(tstr);
                        GlobalUnlock(hMem);
                    } else {
                        WIN_SetError("Couldn't lock clipboard data");
                    }
                } else {
                    WIN_SetError("Couldn't get clipboard data");
                }
                WIN_CloseClipboard();
            }
        }
        if (!text) {
            text = SDL_strdup("");
        }
        data = text;
        *size = SDL_strlen(text);

    } else if (SDL_strcmp(mime_type, IMAGE_MIME_TYPE) == 0) {
        if (IsClipboardFormatAvailable(IMAGE_FORMAT)) {
            if (WIN_OpenClipboard(_this)) {
                HANDLE hMem;

                hMem = GetClipboardData(IMAGE_FORMAT);
                if (hMem) {
                    data = WIN_ConvertDIBtoBMP(hMem, size);
                } else {
                    WIN_SetError("Couldn't get clipboard data");
                }
                WIN_CloseClipboard();
            }
        }
    } else {
        data = SDL_GetInternalClipboardData(_this, mime_type, size);
    }
    return data;
}

SDL_bool WIN_HasClipboardData(SDL_VideoDevice *_this, const char *mime_type)
{
    if (SDL_IsTextMimeType(mime_type)) {
        if (IsClipboardFormatAvailable(TEXT_FORMAT)) {
            return SDL_TRUE;
        }
    } else if (SDL_strcmp(mime_type, IMAGE_MIME_TYPE) == 0) {
        if (IsClipboardFormatAvailable(IMAGE_FORMAT)) {
            return SDL_TRUE;
        }
    } else {
        if (SDL_HasInternalClipboardData(_this, mime_type)) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

void WIN_CheckClipboardUpdate(struct SDL_VideoData *data)
{
    const DWORD count = GetClipboardSequenceNumber();
    if (count != data->clipboard_count) {
        if (data->clipboard_count) {
            SDL_SendClipboardUpdate();
        }
        data->clipboard_count = count;
    }
}

#endif /* SDL_VIDEO_DRIVER_WINDOWS */
