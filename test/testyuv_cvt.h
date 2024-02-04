/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* These functions are designed for testing correctness, not for speed */

typedef enum
{
    YUV_CONVERSION_JPEG,        /**< Full range JPEG */
    YUV_CONVERSION_BT601,       /**< BT.601 (the default) */
    YUV_CONVERSION_BT709,       /**< BT.709 */
    YUV_CONVERSION_AUTOMATIC    /**< BT.601 for SD content, BT.709 for HD content */
} YUV_CONVERSION_MODE;

extern void SetYUVConversionMode(YUV_CONVERSION_MODE mode);
extern YUV_CONVERSION_MODE GetYUVConversionModeForResolution(int width, int height);
extern SDL_bool ConvertRGBtoYUV(Uint32 format, Uint8 *src, int pitch, Uint8 *out, int w, int h, YUV_CONVERSION_MODE mode, int monochrome, int luminance);
extern int CalculateYUVPitch(Uint32 format, int width);
