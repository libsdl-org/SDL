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

/**
 * # CategorySurface
 *
 * SDL_Surface definition and management functions.
 */

#ifndef SDL_surface_h_
#define SDL_surface_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_iostream.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * The flags on an SDL_Surface.
 *
 * These are generally meant to be considered read-only.
 *
 * \since This datatype is available since SDL 3.0.0.
 */
typedef Uint32 SDL_SurfaceFlags;

#define SDL_PREALLOC                0x00000001u /**< Surface uses preallocated memory */
#define SDL_RLEACCEL                0x00000002u /**< Surface is RLE encoded */
#define SDL_DONTFREE                0x00000004u /**< Surface is referenced internally */
#define SDL_SIMD_ALIGNED            0x00000008u /**< Surface uses aligned memory */
#define SDL_SURFACE_USES_PROPERTIES 0x00000010u /**< Surface uses properties */

/**
 * Evaluates to true if the surface needs to be locked before access.
 *
 * \since This macro is available since SDL 3.0.0.
 */
#define SDL_MUSTLOCK(S) (((S)->flags & SDL_RLEACCEL) != 0)

/**
 * An opaque type used in SDL_Surface.
 *
 * This is used by SDL to keep track of how blit operations should work
 * internally; it is not for use directly by applications.
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_BlitMap SDL_BlitMap;

/**
 * The scaling mode.
 *
 * \since This enum is available since SDL 3.0.0.
 */
typedef enum SDL_ScaleMode
{
    SDL_SCALEMODE_NEAREST, /**< nearest pixel sampling */
    SDL_SCALEMODE_LINEAR,  /**< linear filtering */
    SDL_SCALEMODE_BEST     /**< anisotropic filtering */
} SDL_ScaleMode;

/**
 * The flip mode.
 *
 * \since This enum is available since SDL 3.0.0.
 */
typedef enum SDL_FlipMode
{
    SDL_FLIP_NONE,          /**< Do not flip */
    SDL_FLIP_HORIZONTAL,    /**< flip horizontally */
    SDL_FLIP_VERTICAL       /**< flip vertically */
} SDL_FlipMode;

/**
 * A collection of pixels used in software blitting.
 *
 * Pixels are arranged in memory in rows, with the top row first. Each row
 * occupies an amount of memory given by the pitch (sometimes known as the row
 * stride in non-SDL APIs).
 *
 * Within each row, pixels are arranged from left to right until the width is
 * reached. Each pixel occupies a number of bits appropriate for its format,
 * with most formats representing each pixel as one or more whole bytes (in
 * some indexed formats, instead multiple pixels are packed into each byte),
 * and a byte order given by the format. After encoding all pixels, any
 * remaining bytes to reach the pitch are used as padding to reach a desired
 * alignment, and have undefined contents.
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_Surface
{
    SDL_SurfaceFlags flags;     /**< Read-only */
    SDL_PixelFormat *format;    /**< Read-only */
    int w, h;                   /**< Read-only */
    int pitch;                  /**< Read-only */
    void *pixels;               /**< Read-write */

    void *reserved;             /**< Private */

    /** information needed for surfaces requiring locks */
    int locked;                 /**< Read-only */

    /** list of BlitMap that hold a reference to this surface */
    void *list_blitmap;         /**< Private */

    /** clipping information */
    SDL_Rect clip_rect;         /**< Read-only */

    /** info for fast blit mapping to other surfaces */
    SDL_BlitMap *map;           /**< Private */

    /** Reference count -- used when freeing surface */
    int refcount;               /**< Read-mostly */
} SDL_Surface;


/**
 * Allocate a new RGB surface with a specific pixel format.
 *
 * \param width the width of the surface.
 * \param height the height of the surface.
 * \param format the SDL_PixelFormatEnum for the new surface's pixel format.
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateSurfaceFrom
 * \sa SDL_DestroySurface
 */
extern SDL_DECLSPEC SDL_Surface *SDLCALL SDL_CreateSurface(int width, int height, SDL_PixelFormatEnum format);

/**
 * Allocate a new RGB surface with a specific pixel format and existing pixel
 * data.
 *
 * No copy is made of the pixel data. Pixel data is not managed automatically;
 * you must free the surface before you free the pixel data.
 *
 * Pitch is the offset in bytes from one row of pixels to the next, e.g.
 * `width*4` for `SDL_PIXELFORMAT_RGBA8888`.
 *
 * You may pass NULL for pixels and 0 for pitch to create a surface that you
 * will fill in with valid values later.
 *
 * \param pixels a pointer to existing pixel data.
 * \param width the width of the surface.
 * \param height the height of the surface.
 * \param pitch the number of bytes between each row, including padding.
 * \param format the SDL_PixelFormatEnum for the new surface's pixel format.
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateSurface
 * \sa SDL_DestroySurface
 */
extern SDL_DECLSPEC SDL_Surface *SDLCALL SDL_CreateSurfaceFrom(void *pixels, int width, int height, int pitch, SDL_PixelFormatEnum format);

/**
 * Free an RGB surface.
 *
 * It is safe to pass NULL to this function.
 *
 * \param surface the SDL_Surface to free.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateSurface
 * \sa SDL_CreateSurfaceFrom
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroySurface(SDL_Surface *surface);

/**
 * Get the properties associated with a surface.
 *
 * The following properties are understood by SDL:
 *
 * - `SDL_PROP_SURFACE_COLORSPACE_NUMBER`: an SDL_ColorSpace value describing
 *   the surface colorspace, defaults to SDL_COLORSPACE_SRGB_LINEAR for
 *   floating point formats, SDL_COLORSPACE_HDR10 for 10-bit formats,
 *   SDL_COLORSPACE_SRGB for other RGB surfaces and SDL_COLORSPACE_BT709_FULL
 *   for YUV surfaces.
 * - `SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT`: for HDR10 and floating point
 *   surfaces, this defines the value of 100% diffuse white, with higher
 *   values being displayed in the High Dynamic Range headroom. This defaults
 *   to 203 for HDR10 surfaces and 1.0 for floating point surfaces.
 * - `SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT`: for HDR10 and floating point
 *   surfaces, this defines the maximum dynamic range used by the content, in
 *   terms of the SDR white point. This defaults to 0.0, which disables tone
 *   mapping.
 * - `SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING`: the tone mapping operator
 *   used when compressing from a surface with high dynamic range to another
 *   with lower dynamic range. Currently this supports "chrome", which uses
 *   the same tone mapping that Chrome uses for HDR content, the form "*=N",
 *   where N is a floating point scale factor applied in linear space, and
 *   "none", which disables tone mapping. This defaults to "chrome".
 *
 * \param surface the SDL_Surface structure to query.
 * \returns a valid property ID on success or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetProperty
 * \sa SDL_SetProperty
 */
extern SDL_DECLSPEC SDL_PropertiesID SDLCALL SDL_GetSurfaceProperties(SDL_Surface *surface);

#define SDL_PROP_SURFACE_COLORSPACE_NUMBER                  "SDL.surface.colorspace"
#define SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT              "SDL.surface.SDR_white_point"
#define SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT                 "SDL.surface.HDR_headroom"
#define SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING            "SDL.surface.tonemap"

/**
 * Set the colorspace used by a surface.
 *
 * Setting the colorspace doesn't change the pixels, only how they are
 * interpreted in color operations.
 *
 * \param surface the SDL_Surface structure to update.
 * \param colorspace an SDL_ColorSpace value describing the surface
 *                   colorspace.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetSurfaceColorspace(SDL_Surface *surface, SDL_Colorspace colorspace);

/**
 * Get the colorspace used by a surface.
 *
 * The colorspace defaults to SDL_COLORSPACE_SRGB_LINEAR for floating point
 * formats, SDL_COLORSPACE_HDR10 for 10-bit formats, SDL_COLORSPACE_SRGB for
 * other RGB surfaces and SDL_COLORSPACE_BT709_FULL for YUV textures.
 *
 * \param surface the SDL_Surface structure to query.
 * \param colorspace a pointer filled in with an SDL_ColorSpace value
 *                   describing the surface colorspace.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetSurfaceColorspace(SDL_Surface *surface, SDL_Colorspace *colorspace);

/**
 * Set the palette used by a surface.
 *
 * A single palette can be shared with many surfaces.
 *
 * \param surface the SDL_Surface structure to update.
 * \param palette the SDL_Palette structure to use.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetSurfacePalette(SDL_Surface *surface, SDL_Palette *palette);

/**
 * Set up a surface for directly accessing the pixels.
 *
 * Between calls to SDL_LockSurface() / SDL_UnlockSurface(), you can write to
 * and read from `surface->pixels`, using the pixel format stored in
 * `surface->format`. Once you are done accessing the surface, you should use
 * SDL_UnlockSurface() to release it.
 *
 * Not all surfaces require locking. If `SDL_MUSTLOCK(surface)` evaluates to
 * 0, then you can read and write to the surface at any time, and the pixel
 * format of the surface will not change.
 *
 * \param surface the SDL_Surface structure to be locked.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_MUSTLOCK
 * \sa SDL_UnlockSurface
 */
extern SDL_DECLSPEC int SDLCALL SDL_LockSurface(SDL_Surface *surface);

/**
 * Release a surface after directly accessing the pixels.
 *
 * \param surface the SDL_Surface structure to be unlocked.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockSurface
 */
extern SDL_DECLSPEC void SDLCALL SDL_UnlockSurface(SDL_Surface *surface);

/**
 * Load a BMP image from a seekable SDL data stream.
 *
 * The new surface should be freed with SDL_DestroySurface(). Not doing so
 * will result in a memory leak.
 *
 * \param src the data stream for the surface.
 * \param closeio if SDL_TRUE, calls SDL_CloseIO() on `src` before returning,
 *                even in the case of an error.
 * \returns a pointer to a new SDL_Surface structure or NULL if there was an
 *          error; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroySurface
 * \sa SDL_LoadBMP
 * \sa SDL_SaveBMP_IO
 */
extern SDL_DECLSPEC SDL_Surface *SDLCALL SDL_LoadBMP_IO(SDL_IOStream *src, SDL_bool closeio);

/**
 * Load a BMP image from a file.
 *
 * The new surface should be freed with SDL_DestroySurface(). Not doing so
 * will result in a memory leak.
 *
 * \param file the BMP file to load.
 * \returns a pointer to a new SDL_Surface structure or NULL if there was an
 *          error; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroySurface
 * \sa SDL_LoadBMP_IO
 * \sa SDL_SaveBMP
 */
extern SDL_DECLSPEC SDL_Surface *SDLCALL SDL_LoadBMP(const char *file);

/**
 * Save a surface to a seekable SDL data stream in BMP format.
 *
 * Surfaces with a 24-bit, 32-bit and paletted 8-bit format get saved in the
 * BMP directly. Other RGB formats with 8-bit or higher get converted to a
 * 24-bit surface or, if they have an alpha mask or a colorkey, to a 32-bit
 * surface before they are saved. YUV and paletted 1-bit and 4-bit formats are
 * not supported.
 *
 * \param surface the SDL_Surface structure containing the image to be saved.
 * \param dst a data stream to save to.
 * \param closeio if SDL_TRUE, calls SDL_CloseIO() on `dst` before returning,
 *                even in the case of an error.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LoadBMP_IO
 * \sa SDL_SaveBMP
 */
extern SDL_DECLSPEC int SDLCALL SDL_SaveBMP_IO(SDL_Surface *surface, SDL_IOStream *dst, SDL_bool closeio);

/**
 * Save a surface to a file.
 *
 * Surfaces with a 24-bit, 32-bit and paletted 8-bit format get saved in the
 * BMP directly. Other RGB formats with 8-bit or higher get converted to a
 * 24-bit surface or, if they have an alpha mask or a colorkey, to a 32-bit
 * surface before they are saved. YUV and paletted 1-bit and 4-bit formats are
 * not supported.
 *
 * \param surface the SDL_Surface structure containing the image to be saved.
 * \param file a file to save to.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LoadBMP
 * \sa SDL_SaveBMP_IO
 */
extern SDL_DECLSPEC int SDLCALL SDL_SaveBMP(SDL_Surface *surface, const char *file);

/**
 * Set the RLE acceleration hint for a surface.
 *
 * If RLE is enabled, color key and alpha blending blits are much faster, but
 * the surface must be locked before directly accessing the pixels.
 *
 * \param surface the SDL_Surface structure to optimize.
 * \param flag 0 to disable, non-zero to enable RLE acceleration.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 * \sa SDL_LockSurface
 * \sa SDL_UnlockSurface
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetSurfaceRLE(SDL_Surface *surface, int flag);

/**
 * Returns whether the surface is RLE enabled.
 *
 * It is safe to pass a NULL `surface` here; it will return SDL_FALSE.
 *
 * \param surface the SDL_Surface structure to query.
 * \returns SDL_TRUE if the surface is RLE enabled, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetSurfaceRLE
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_SurfaceHasRLE(SDL_Surface *surface);

/**
 * Set the color key (transparent pixel) in a surface.
 *
 * The color key defines a pixel value that will be treated as transparent in
 * a blit. For example, one can use this to specify that cyan pixels should be
 * considered transparent, and therefore not rendered.
 *
 * It is a pixel of the format used by the surface, as generated by
 * SDL_MapRGB().
 *
 * RLE acceleration can substantially speed up blitting of images with large
 * horizontal runs of transparent pixels. See SDL_SetSurfaceRLE() for details.
 *
 * \param surface the SDL_Surface structure to update.
 * \param flag SDL_TRUE to enable color key, SDL_FALSE to disable color key.
 * \param key the transparent pixel.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceColorKey
 * \sa SDL_SurfaceHasColorKey
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetSurfaceColorKey(SDL_Surface *surface, int flag, Uint32 key);

/**
 * Returns whether the surface has a color key.
 *
 * It is safe to pass a NULL `surface` here; it will return SDL_FALSE.
 *
 * \param surface the SDL_Surface structure to query.
 * \returns SDL_TRUE if the surface has a color key, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetSurfaceColorKey
 * \sa SDL_GetSurfaceColorKey
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_SurfaceHasColorKey(SDL_Surface *surface);

/**
 * Get the color key (transparent pixel) for a surface.
 *
 * The color key is a pixel of the format used by the surface, as generated by
 * SDL_MapRGB().
 *
 * If the surface doesn't have color key enabled this function returns -1.
 *
 * \param surface the SDL_Surface structure to query.
 * \param key a pointer filled in with the transparent pixel.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetSurfaceColorKey
 * \sa SDL_SurfaceHasColorKey
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetSurfaceColorKey(SDL_Surface *surface, Uint32 *key);

/**
 * Set an additional color value multiplied into blit operations.
 *
 * When this surface is blitted, during the blit operation each source color
 * channel is modulated by the appropriate color value according to the
 * following formula:
 *
 * `srcC = srcC * (color / 255)`
 *
 * \param surface the SDL_Surface structure to update.
 * \param r the red color value multiplied into blit operations.
 * \param g the green color value multiplied into blit operations.
 * \param b the blue color value multiplied into blit operations.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceColorMod
 * \sa SDL_SetSurfaceAlphaMod
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetSurfaceColorMod(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b);


/**
 * Get the additional color value multiplied into blit operations.
 *
 * \param surface the SDL_Surface structure to query.
 * \param r a pointer filled in with the current red color value.
 * \param g a pointer filled in with the current green color value.
 * \param b a pointer filled in with the current blue color value.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceAlphaMod
 * \sa SDL_SetSurfaceColorMod
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetSurfaceColorMod(SDL_Surface *surface, Uint8 *r, Uint8 *g, Uint8 *b);

/**
 * Set an additional alpha value used in blit operations.
 *
 * When this surface is blitted, during the blit operation the source alpha
 * value is modulated by this alpha value according to the following formula:
 *
 * `srcA = srcA * (alpha / 255)`
 *
 * \param surface the SDL_Surface structure to update.
 * \param alpha the alpha value multiplied into blit operations.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceAlphaMod
 * \sa SDL_SetSurfaceColorMod
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 alpha);

/**
 * Get the additional alpha value used in blit operations.
 *
 * \param surface the SDL_Surface structure to query.
 * \param alpha a pointer filled in with the current alpha value.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceColorMod
 * \sa SDL_SetSurfaceAlphaMod
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetSurfaceAlphaMod(SDL_Surface *surface, Uint8 *alpha);

/**
 * Set the blend mode used for blit operations.
 *
 * To copy a surface to another surface (or texture) without blending with the
 * existing data, the blendmode of the SOURCE surface should be set to
 * `SDL_BLENDMODE_NONE`.
 *
 * \param surface the SDL_Surface structure to update.
 * \param blendMode the SDL_BlendMode to use for blit blending.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceBlendMode
 */
extern SDL_DECLSPEC int SDLCALL SDL_SetSurfaceBlendMode(SDL_Surface *surface, SDL_BlendMode blendMode);

/**
 * Get the blend mode used for blit operations.
 *
 * \param surface the SDL_Surface structure to query.
 * \param blendMode a pointer filled in with the current SDL_BlendMode.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetSurfaceBlendMode
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetSurfaceBlendMode(SDL_Surface *surface, SDL_BlendMode *blendMode);

/**
 * Set the clipping rectangle for a surface.
 *
 * When `surface` is the destination of a blit, only the area within the clip
 * rectangle is drawn into.
 *
 * Note that blits are automatically clipped to the edges of the source and
 * destination surfaces.
 *
 * \param surface the SDL_Surface structure to be clipped.
 * \param rect the SDL_Rect structure representing the clipping rectangle, or
 *             NULL to disable clipping.
 * \returns SDL_TRUE if the rectangle intersects the surface, otherwise
 *          SDL_FALSE and blits will be completely clipped.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceClipRect
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_SetSurfaceClipRect(SDL_Surface *surface, const SDL_Rect *rect);

/**
 * Get the clipping rectangle for a surface.
 *
 * When `surface` is the destination of a blit, only the area within the clip
 * rectangle is drawn into.
 *
 * \param surface the SDL_Surface structure representing the surface to be
 *                clipped.
 * \param rect an SDL_Rect structure filled in with the clipping rectangle for
 *             the surface.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetSurfaceClipRect
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetSurfaceClipRect(SDL_Surface *surface, SDL_Rect *rect);

/**
 * Flip a surface vertically or horizontally.
 *
 * \param surface the surface to flip.
 * \param flip the direction to flip.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL SDL_FlipSurface(SDL_Surface *surface, SDL_FlipMode flip);

/**
 * Creates a new surface identical to the existing surface.
 *
 * The returned surface should be freed with SDL_DestroySurface().
 *
 * \param surface the surface to duplicate.
 * \returns a copy of the surface, or NULL on failure; call SDL_GetError() for
 *          more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroySurface
 */
extern SDL_DECLSPEC SDL_Surface *SDLCALL SDL_DuplicateSurface(SDL_Surface *surface);

/**
 * Copy an existing surface to a new surface of the specified format.
 *
 * This function is used to optimize images for faster *repeat* blitting. This
 * is accomplished by converting the original and storing the result as a new
 * surface. The new, optimized surface can then be used as the source for
 * future blits, making them faster.
 *
 * \param surface the existing SDL_Surface structure to convert.
 * \param format the SDL_PixelFormat structure that the new surface is
 *               optimized for.
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ConvertSurfaceFormat
 * \sa SDL_ConvertSurfaceFormatAndColorspace
 * \sa SDL_CreatePixelFormat
 * \sa SDL_DestroySurface
 */
extern SDL_DECLSPEC SDL_Surface *SDLCALL SDL_ConvertSurface(SDL_Surface *surface, const SDL_PixelFormat *format);

/**
 * Copy an existing surface to a new surface of the specified format.
 *
 * This function operates just like SDL_ConvertSurface(), but accepts an
 * SDL_PixelFormatEnum value instead of an SDL_PixelFormat structure. As such,
 * it might be easier to call but it doesn't have access to palette
 * information for the destination surface, in case that would be important.
 *
 * \param surface the existing SDL_Surface structure to convert.
 * \param pixel_format the new pixel format.
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ConvertSurface
 * \sa SDL_ConvertSurfaceFormatAndColorspace
 * \sa SDL_DestroySurface
 */
extern SDL_DECLSPEC SDL_Surface *SDLCALL SDL_ConvertSurfaceFormat(SDL_Surface *surface, SDL_PixelFormatEnum pixel_format);

/**
 * Copy an existing surface to a new surface of the specified format and
 * colorspace.
 *
 * This function converts an existing surface to a new format and colorspace
 * and returns the new surface. This will perform any pixel format and
 * colorspace conversion needed.
 *
 * \param surface the existing SDL_Surface structure to convert.
 * \param pixel_format the new pixel format.
 * \param colorspace the new colorspace.
 * \param props an SDL_PropertiesID with additional color properties, or 0.
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ConvertSurface
 * \sa SDL_ConvertSurfaceFormat
 * \sa SDL_DestroySurface
 */
extern SDL_DECLSPEC SDL_Surface *SDLCALL SDL_ConvertSurfaceFormatAndColorspace(SDL_Surface *surface, SDL_PixelFormatEnum pixel_format, SDL_Colorspace colorspace, SDL_PropertiesID props);

/**
 * Copy a block of pixels of one format to another format.
 *
 * \param width the width of the block to copy, in pixels.
 * \param height the height of the block to copy, in pixels.
 * \param src_format an SDL_PixelFormatEnum value of the `src` pixels format.
 * \param src a pointer to the source pixels.
 * \param src_pitch the pitch of the source pixels, in bytes.
 * \param dst_format an SDL_PixelFormatEnum value of the `dst` pixels format.
 * \param dst a pointer to be filled in with new pixel data.
 * \param dst_pitch the pitch of the destination pixels, in bytes.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ConvertPixelsAndColorspace
 */
extern SDL_DECLSPEC int SDLCALL SDL_ConvertPixels(int width, int height, SDL_PixelFormatEnum src_format, const void *src, int src_pitch, SDL_PixelFormatEnum dst_format, void *dst, int dst_pitch);

/**
 * Copy a block of pixels of one format and colorspace to another format and
 * colorspace.
 *
 * \param width the width of the block to copy, in pixels.
 * \param height the height of the block to copy, in pixels.
 * \param src_format an SDL_PixelFormatEnum value of the `src` pixels format.
 * \param src_colorspace an SDL_ColorSpace value describing the colorspace of
 *                       the `src` pixels.
 * \param src_properties an SDL_PropertiesID with additional source color
 *                       properties, or 0.
 * \param src a pointer to the source pixels.
 * \param src_pitch the pitch of the source pixels, in bytes.
 * \param dst_format an SDL_PixelFormatEnum value of the `dst` pixels format.
 * \param dst_colorspace an SDL_ColorSpace value describing the colorspace of
 *                       the `dst` pixels.
 * \param dst_properties an SDL_PropertiesID with additional destination color
 *                       properties, or 0.
 * \param dst a pointer to be filled in with new pixel data.
 * \param dst_pitch the pitch of the destination pixels, in bytes.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ConvertPixels
 */
extern SDL_DECLSPEC int SDLCALL SDL_ConvertPixelsAndColorspace(int width, int height, SDL_PixelFormatEnum src_format, SDL_Colorspace src_colorspace, SDL_PropertiesID src_properties, const void *src, int src_pitch, SDL_PixelFormatEnum dst_format, SDL_Colorspace dst_colorspace, SDL_PropertiesID dst_properties, void *dst, int dst_pitch);

/**
 * Premultiply the alpha on a block of pixels.
 *
 * This is safe to use with src == dst, but not for other overlapping areas.
 *
 * This function is currently only implemented for SDL_PIXELFORMAT_ARGB8888.
 *
 * \param width the width of the block to convert, in pixels.
 * \param height the height of the block to convert, in pixels.
 * \param src_format an SDL_PixelFormatEnum value of the `src` pixels format.
 * \param src a pointer to the source pixels.
 * \param src_pitch the pitch of the source pixels, in bytes.
 * \param dst_format an SDL_PixelFormatEnum value of the `dst` pixels format.
 * \param dst a pointer to be filled in with premultiplied pixel data.
 * \param dst_pitch the pitch of the destination pixels, in bytes.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL SDL_PremultiplyAlpha(int width, int height, SDL_PixelFormatEnum src_format, const void *src, int src_pitch, SDL_PixelFormatEnum dst_format, void *dst, int dst_pitch);

/**
 * Perform a fast fill of a rectangle with a specific color.
 *
 * `color` should be a pixel of the format used by the surface, and can be
 * generated by SDL_MapRGB() or SDL_MapRGBA(). If the color value contains an
 * alpha component then the destination is simply filled with that alpha
 * information, no blending takes place.
 *
 * If there is a clip rectangle set on the destination (set via
 * SDL_SetSurfaceClipRect()), then this function will fill based on the
 * intersection of the clip rectangle and `rect`.
 *
 * \param dst the SDL_Surface structure that is the drawing target.
 * \param rect the SDL_Rect structure representing the rectangle to fill, or
 *             NULL to fill the entire surface.
 * \param color the color to fill with.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_FillSurfaceRects
 */
extern SDL_DECLSPEC int SDLCALL SDL_FillSurfaceRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);

/**
 * Perform a fast fill of a set of rectangles with a specific color.
 *
 * `color` should be a pixel of the format used by the surface, and can be
 * generated by SDL_MapRGB() or SDL_MapRGBA(). If the color value contains an
 * alpha component then the destination is simply filled with that alpha
 * information, no blending takes place.
 *
 * If there is a clip rectangle set on the destination (set via
 * SDL_SetSurfaceClipRect()), then this function will fill based on the
 * intersection of the clip rectangle and `rect`.
 *
 * \param dst the SDL_Surface structure that is the drawing target.
 * \param rects an array of SDL_Rects representing the rectangles to fill.
 * \param count the number of rectangles in the array.
 * \param color the color to fill with.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_FillSurfaceRect
 */
extern SDL_DECLSPEC int SDLCALL SDL_FillSurfaceRects(SDL_Surface *dst, const SDL_Rect *rects, int count, Uint32 color);

/**
 * Performs a fast blit from the source surface to the destination surface.
 *
 * This assumes that the source and destination rectangles are the same size.
 * If either `srcrect` or `dstrect` are NULL, the entire surface (`src` or
 * `dst`) is copied. The final blit rectangles are saved in `srcrect` and
 * `dstrect` after all clipping is performed.
 *
 * The blit function should not be called on a locked surface.
 *
 * The blit semantics for surfaces with and without blending and colorkey are
 * defined as follows:
 *
 * ```
 *    RGBA->RGB:
 *      Source surface blend mode set to SDL_BLENDMODE_BLEND:
 *       alpha-blend (using the source alpha-channel and per-surface alpha)
 *       SDL_SRCCOLORKEY ignored.
 *     Source surface blend mode set to SDL_BLENDMODE_NONE:
 *       copy RGB.
 *       if SDL_SRCCOLORKEY set, only copy the pixels matching the
 *       RGB values of the source color key, ignoring alpha in the
 *       comparison.
 *
 *   RGB->RGBA:
 *     Source surface blend mode set to SDL_BLENDMODE_BLEND:
 *       alpha-blend (using the source per-surface alpha)
 *     Source surface blend mode set to SDL_BLENDMODE_NONE:
 *       copy RGB, set destination alpha to source per-surface alpha value.
 *     both:
 *       if SDL_SRCCOLORKEY set, only copy the pixels matching the
 *       source color key.
 *
 *   RGBA->RGBA:
 *     Source surface blend mode set to SDL_BLENDMODE_BLEND:
 *       alpha-blend (using the source alpha-channel and per-surface alpha)
 *       SDL_SRCCOLORKEY ignored.
 *     Source surface blend mode set to SDL_BLENDMODE_NONE:
 *       copy all of RGBA to the destination.
 *       if SDL_SRCCOLORKEY set, only copy the pixels matching the
 *       RGB values of the source color key, ignoring alpha in the
 *       comparison.
 *
 *   RGB->RGB:
 *     Source surface blend mode set to SDL_BLENDMODE_BLEND:
 *       alpha-blend (using the source per-surface alpha)
 *     Source surface blend mode set to SDL_BLENDMODE_NONE:
 *       copy RGB.
 *     both:
 *       if SDL_SRCCOLORKEY set, only copy the pixels matching the
 *       source color key.
 * ```
 *
 * \param src the SDL_Surface structure to be copied from.
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied, or NULL to copy the entire surface.
 * \param dst the SDL_Surface structure that is the blit target.
 * \param dstrect the SDL_Rect structure representing the x and y position in
 *                the destination surface. On input the width and height are
 *                ignored (taken from srcrect), and on output this is filled
 *                in with the actual rectangle used after clipping.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety The same destination surface should not be used from two
 *               threads at once. It is safe to use the same source surface
 *               from multiple threads.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurfaceScaled
 */
extern SDL_DECLSPEC int SDLCALL SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);

/**
 * Perform low-level surface blitting only.
 *
 * This is a semi-private blit function and it performs low-level surface
 * blitting, assuming the input rectangles have already been clipped.
 *
 * \param src the SDL_Surface structure to be copied from.
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied, or NULL to copy the entire surface.
 * \param dst the SDL_Surface structure that is the blit target.
 * \param dstrect the SDL_Rect structure representing the target rectangle in
 *                the destination surface.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety The same destination surface should not be used from two
 *               threads at once. It is safe to use the same source surface
 *               from multiple threads.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 */
extern SDL_DECLSPEC int SDLCALL SDL_BlitSurfaceUnchecked(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);

/**
 * Perform stretch blit between two surfaces of the same format.
 *
 * Using SDL_SCALEMODE_NEAREST: fast, low quality. Using SDL_SCALEMODE_LINEAR:
 * bilinear scaling, slower, better quality, only 32BPP.
 *
 * \param src the SDL_Surface structure to be copied from.
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied.
 * \param dst the SDL_Surface structure that is the blit target.
 * \param dstrect the SDL_Rect structure representing the target rectangle in
 *                the destination surface.
 * \param scaleMode scale algorithm to be used.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurfaceScaled
 */
extern SDL_DECLSPEC int SDLCALL SDL_SoftStretch(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect, SDL_ScaleMode scaleMode);

/**
 * Perform a scaled blit to a destination surface, which may be of a different
 * format.
 *
 * \param src the SDL_Surface structure to be copied from.
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied.
 * \param dst the SDL_Surface structure that is the blit target.
 * \param dstrect the SDL_Rect structure representing the target rectangle in
 *                the destination surface, filled with the actual rectangle
 *                used after clipping.
 * \param scaleMode the SDL_ScaleMode to be used.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety The same destination surface should not be used from two
 *               threads at once. It is safe to use the same source surface
 *               from multiple threads.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 */
extern SDL_DECLSPEC int SDLCALL SDL_BlitSurfaceScaled(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect, SDL_ScaleMode scaleMode);

/**
 * Perform low-level surface scaled blitting only.
 *
 * This is a semi-private function and it performs low-level surface blitting,
 * assuming the input rectangles have already been clipped.
 *
 * \param src the SDL_Surface structure to be copied from.
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied.
 * \param dst the SDL_Surface structure that is the blit target.
 * \param dstrect the SDL_Rect structure representing the target rectangle in
 *                the destination surface.
 * \param scaleMode scale algorithm to be used.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety The same destination surface should not be used from two
 *               threads at once. It is safe to use the same source surface
 *               from multiple threads.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurfaceScaled
 */
extern SDL_DECLSPEC int SDLCALL SDL_BlitSurfaceUncheckedScaled(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect, SDL_ScaleMode scaleMode);

/**
 * Retrieves a single pixel from a surface.
 *
 * This function prioritizes correctness over speed: it is suitable for unit
 * tests, but is not intended for use in a game engine.
 *
 * Like SDL_GetRGBA, this uses the entire 0..255 range when converting color
 * components from pixel formats with less than 8 bits per RGB component.
 *
 * \param surface the surface to read.
 * \param x the horizontal coordinate, 0 <= x < width.
 * \param y the vertical coordinate, 0 <= y < height.
 * \param r a pointer filled in with the red channel, 0-255, or NULL to ignore
 *          this channel.
 * \param g a pointer filled in with the green channel, 0-255, or NULL to
 *          ignore this channel.
 * \param b a pointer filled in with the blue channel, 0-255, or NULL to
 *          ignore this channel.
 * \param a a pointer filled in with the alpha channel, 0-255, or NULL to
 *          ignore this channel.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL SDL_ReadSurfacePixel(SDL_Surface *surface, int x, int y, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_surface_h_ */
