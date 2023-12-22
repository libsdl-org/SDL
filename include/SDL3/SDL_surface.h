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

/**
 *  \file SDL_surface.h
 *
 *  Header file for ::SDL_Surface definition and management functions.
 */

#ifndef SDL_surface_h_
#define SDL_surface_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_rwops.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \name Surface flags
 *
 *  These are the currently supported flags for the ::SDL_Surface.
 *
 *  \internal
 *  Used internally (read-only).
 */
/* @{ */
#define SDL_SWSURFACE               0           /**< Just here for compatibility */
#define SDL_PREALLOC                0x00000001  /**< Surface uses preallocated memory */
#define SDL_RLEACCEL                0x00000002  /**< Surface is RLE encoded */
#define SDL_DONTFREE                0x00000004  /**< Surface is referenced internally */
#define SDL_SIMD_ALIGNED            0x00000008  /**< Surface uses aligned memory */
#define SDL_SURFACE_USES_PROPERTIES 0x00000010  /**< Surface uses properties */
/* @} *//* Surface flags */

/**
 *  Evaluates to true if the surface needs to be locked before access.
 */
#define SDL_MUSTLOCK(S) (((S)->flags & SDL_RLEACCEL) != 0)

typedef struct SDL_BlitMap SDL_BlitMap;  /* this is an opaque type. */

/**
 * The scaling mode
 */
typedef enum
{
    SDL_SCALEMODE_NEAREST, /**< nearest pixel sampling */
    SDL_SCALEMODE_LINEAR,  /**< linear filtering */
    SDL_SCALEMODE_BEST     /**< anisotropic filtering */
} SDL_ScaleMode;


/**
 * A collection of pixels used in software blitting.
 *
 * Pixels are arranged in memory in rows, with the top row first.
 * Each row occupies an amount of memory given by the pitch (sometimes
 * known as the row stride in non-SDL APIs).
 *
 * Within each row, pixels are arranged from left to right until the
 * width is reached.
 * Each pixel occupies a number of bits appropriate for its format, with
 * most formats representing each pixel as one or more whole bytes
 * (in some indexed formats, instead multiple pixels are packed into
 * each byte), and a byte order given by the format.
 * After encoding all pixels, any remaining bytes to reach the pitch are
 * used as padding to reach a desired alignment, and have undefined contents.
 *
 * \note  This structure should be treated as read-only, except for \c pixels,
 *        which, if not NULL, contains the raw pixel data for the surface.
 * \sa SDL_CreateSurfaceFrom
 */
typedef struct SDL_Surface
{
    Uint32 flags;               /**< Read-only */
    SDL_PixelFormat *format;    /**< Read-only */
    int w, h;                   /**< Read-only */
    int pitch;                  /**< Read-only */
    void *pixels;               /**< Read-write */

    /** Application data associated with the surface */
    union {
        void *reserved;         /**< For ABI compatibility only, do not use */
        SDL_PropertiesID props; /**< Read-only */
    };

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
 * The type of function used for surface blitting functions.
 */
typedef int (SDLCALL *SDL_blit) (struct SDL_Surface *src, const SDL_Rect *srcrect,
                                 struct SDL_Surface *dst, const SDL_Rect *dstrect);

/**
 * The formula used for converting between YUV and RGB
 */
typedef enum
{
    SDL_YUV_CONVERSION_JPEG,        /**< Full range JPEG */
    SDL_YUV_CONVERSION_BT601,       /**< BT.601 (the default) */
    SDL_YUV_CONVERSION_BT709,       /**< BT.709 */
    SDL_YUV_CONVERSION_AUTOMATIC    /**< BT.601 for SD content, BT.709 for HD content */
} SDL_YUV_CONVERSION_MODE;

/**
 * Allocate a new RGB surface with a specific pixel format.
 *
 * \param width the width of the surface
 * \param height the height of the surface
 * \param format the SDL_PixelFormatEnum for the new surface's pixel format.
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateSurfaceFrom
 * \sa SDL_DestroySurface
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_CreateSurface
    (int width, int height, Uint32 format);

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
 * \param pixels a pointer to existing pixel data
 * \param width the width of the surface
 * \param height the height of the surface
 * \param pitch the pitch of the surface in bytes
 * \param format the SDL_PixelFormatEnum for the new surface's pixel format.
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateSurface
 * \sa SDL_DestroySurface
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_CreateSurfaceFrom
    (void *pixels, int width, int height, int pitch, Uint32 format);

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
 * \sa SDL_LoadBMP
 * \sa SDL_LoadBMP_RW
 */
extern DECLSPEC void SDLCALL SDL_DestroySurface(SDL_Surface *surface);

/**
 * Get the properties associated with a surface.
 *
 * \param surface the SDL_Surface structure to query
 * \returns a valid property ID on success or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetProperty
 * \sa SDL_SetProperty
 */
extern DECLSPEC SDL_PropertiesID SDLCALL SDL_GetSurfaceProperties(SDL_Surface *surface);

/**
 * Set the palette used by a surface.
 *
 * A single palette can be shared with many surfaces.
 *
 * \param surface the SDL_Surface structure to update
 * \param palette the SDL_Palette structure to use
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_SetSurfacePalette(SDL_Surface *surface,
                                                  SDL_Palette *palette);

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
 * \param surface the SDL_Surface structure to be locked
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_MUSTLOCK
 * \sa SDL_UnlockSurface
 */
extern DECLSPEC int SDLCALL SDL_LockSurface(SDL_Surface *surface);

/**
 * Release a surface after directly accessing the pixels.
 *
 * \param surface the SDL_Surface structure to be unlocked
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockSurface
 */
extern DECLSPEC void SDLCALL SDL_UnlockSurface(SDL_Surface *surface);

/**
 * Load a BMP image from a seekable SDL data stream.
 *
 * The new surface should be freed with SDL_DestroySurface(). Not doing so
 * will result in a memory leak.
 *
 * \param src the data stream for the surface
 * \param freesrc if SDL_TRUE, calls SDL_RWclose() on `src` before returning,
 *                even in the case of an error
 * \returns a pointer to a new SDL_Surface structure or NULL if there was an
 *          error; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroySurface
 * \sa SDL_LoadBMP
 * \sa SDL_SaveBMP_RW
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_LoadBMP_RW(SDL_RWops *src, SDL_bool freesrc);

/**
 * Load a BMP image from a file.
 *
 * The new surface should be freed with SDL_DestroySurface(). Not doing so
 * will result in a memory leak.
 *
 * \param file the BMP file to load
 * \returns a pointer to a new SDL_Surface structure or NULL if there was an
 *          error; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroySurface
 * \sa SDL_LoadBMP_RW
 * \sa SDL_SaveBMP
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_LoadBMP(const char *file);

/**
 * Save a surface to a seekable SDL data stream in BMP format.
 *
 * Surfaces with a 24-bit, 32-bit and paletted 8-bit format get saved in the
 * BMP directly. Other RGB formats with 8-bit or higher get converted to a
 * 24-bit surface or, if they have an alpha mask or a colorkey, to a 32-bit
 * surface before they are saved. YUV and paletted 1-bit and 4-bit formats are
 * not supported.
 *
 * \param surface the SDL_Surface structure containing the image to be saved
 * \param dst a data stream to save to
 * \param freedst if SDL_TRUE, calls SDL_RWclose() on `dst` before returning,
 *                even in the case of an error
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LoadBMP_RW
 * \sa SDL_SaveBMP
 */
extern DECLSPEC int SDLCALL SDL_SaveBMP_RW(SDL_Surface *surface, SDL_RWops *dst, SDL_bool freedst);

/**
 * Save a surface to a file.
 *
 * Surfaces with a 24-bit, 32-bit and paletted 8-bit format get saved in the
 * BMP directly. Other RGB formats with 8-bit or higher get converted to a
 * 24-bit surface or, if they have an alpha mask or a colorkey, to a 32-bit
 * surface before they are saved. YUV and paletted 1-bit and 4-bit formats are
 * not supported.
 *
 * \param surface the SDL_Surface structure containing the image to be saved
 * \param file a file to save to
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LoadBMP
 * \sa SDL_SaveBMP_RW
 */
extern DECLSPEC int SDLCALL SDL_SaveBMP(SDL_Surface *surface, const char *file);

/**
 * Set the RLE acceleration hint for a surface.
 *
 * If RLE is enabled, color key and alpha blending blits are much faster, but
 * the surface must be locked before directly accessing the pixels.
 *
 * \param surface the SDL_Surface structure to optimize
 * \param flag 0 to disable, non-zero to enable RLE acceleration
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 * \sa SDL_LockSurface
 * \sa SDL_UnlockSurface
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceRLE(SDL_Surface *surface,
                                              int flag);

/**
 * Returns whether the surface is RLE enabled
 *
 * It is safe to pass a NULL `surface` here; it will return SDL_FALSE.
 *
 * \param surface the SDL_Surface structure to query
 * \returns SDL_TRUE if the surface is RLE enabled, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetSurfaceRLE
 */
extern DECLSPEC SDL_bool SDLCALL SDL_SurfaceHasRLE(SDL_Surface *surface);

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
 * \param surface the SDL_Surface structure to update
 * \param flag SDL_TRUE to enable color key, SDL_FALSE to disable color key
 * \param key the transparent pixel
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 * \sa SDL_GetSurfaceColorKey
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceColorKey(SDL_Surface *surface,
                                            int flag, Uint32 key);

/**
 * Returns whether the surface has a color key
 *
 * It is safe to pass a NULL `surface` here; it will return SDL_FALSE.
 *
 * \param surface the SDL_Surface structure to query
 * \returns SDL_TRUE if the surface has a color key, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetSurfaceColorKey
 * \sa SDL_GetSurfaceColorKey
 */
extern DECLSPEC SDL_bool SDLCALL SDL_SurfaceHasColorKey(SDL_Surface *surface);

/**
 * Get the color key (transparent pixel) for a surface.
 *
 * The color key is a pixel of the format used by the surface, as generated by
 * SDL_MapRGB().
 *
 * If the surface doesn't have color key enabled this function returns -1.
 *
 * \param surface the SDL_Surface structure to query
 * \param key a pointer filled in with the transparent pixel
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 * \sa SDL_SetSurfaceColorKey
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceColorKey(SDL_Surface *surface,
                                            Uint32 *key);

/**
 * Set an additional color value multiplied into blit operations.
 *
 * When this surface is blitted, during the blit operation each source color
 * channel is modulated by the appropriate color value according to the
 * following formula:
 *
 * `srcC = srcC * (color / 255)`
 *
 * \param surface the SDL_Surface structure to update
 * \param r the red color value multiplied into blit operations
 * \param g the green color value multiplied into blit operations
 * \param b the blue color value multiplied into blit operations
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceColorMod
 * \sa SDL_SetSurfaceAlphaMod
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceColorMod(SDL_Surface *surface,
                                                   Uint8 r, Uint8 g, Uint8 b);


/**
 * Get the additional color value multiplied into blit operations.
 *
 * \param surface the SDL_Surface structure to query
 * \param r a pointer filled in with the current red color value
 * \param g a pointer filled in with the current green color value
 * \param b a pointer filled in with the current blue color value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceAlphaMod
 * \sa SDL_SetSurfaceColorMod
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceColorMod(SDL_Surface *surface,
                                                   Uint8 *r, Uint8 *g,
                                                   Uint8 *b);

/**
 * Set an additional alpha value used in blit operations.
 *
 * When this surface is blitted, during the blit operation the source alpha
 * value is modulated by this alpha value according to the following formula:
 *
 * `srcA = srcA * (alpha / 255)`
 *
 * \param surface the SDL_Surface structure to update
 * \param alpha the alpha value multiplied into blit operations
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceAlphaMod
 * \sa SDL_SetSurfaceColorMod
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceAlphaMod(SDL_Surface *surface,
                                                   Uint8 alpha);

/**
 * Get the additional alpha value used in blit operations.
 *
 * \param surface the SDL_Surface structure to query
 * \param alpha a pointer filled in with the current alpha value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceColorMod
 * \sa SDL_SetSurfaceAlphaMod
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceAlphaMod(SDL_Surface *surface,
                                                   Uint8 *alpha);

/**
 * Set the blend mode used for blit operations.
 *
 * To copy a surface to another surface (or texture) without blending with the
 * existing data, the blendmode of the SOURCE surface should be set to
 * `SDL_BLENDMODE_NONE`.
 *
 * \param surface the SDL_Surface structure to update
 * \param blendMode the SDL_BlendMode to use for blit blending
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSurfaceBlendMode
 */
extern DECLSPEC int SDLCALL SDL_SetSurfaceBlendMode(SDL_Surface *surface,
                                                    SDL_BlendMode blendMode);

/**
 * Get the blend mode used for blit operations.
 *
 * \param surface the SDL_Surface structure to query
 * \param blendMode a pointer filled in with the current SDL_BlendMode
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetSurfaceBlendMode
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceBlendMode(SDL_Surface *surface,
                                                    SDL_BlendMode *blendMode);

/**
 * Set the clipping rectangle for a surface.
 *
 * When `surface` is the destination of a blit, only the area within the clip
 * rectangle is drawn into.
 *
 * Note that blits are automatically clipped to the edges of the source and
 * destination surfaces.
 *
 * \param surface the SDL_Surface structure to be clipped
 * \param rect the SDL_Rect structure representing the clipping rectangle, or
 *             NULL to disable clipping
 * \returns SDL_TRUE if the rectangle intersects the surface, otherwise
 *          SDL_FALSE and blits will be completely clipped.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 * \sa SDL_GetSurfaceClipRect
 */
extern DECLSPEC SDL_bool SDLCALL SDL_SetSurfaceClipRect(SDL_Surface *surface,
                                                 const SDL_Rect *rect);

/**
 * Get the clipping rectangle for a surface.
 *
 * When `surface` is the destination of a blit, only the area within the clip
 * rectangle is drawn into.
 *
 * \param surface the SDL_Surface structure representing the surface to be
 *                clipped
 * \param rect an SDL_Rect structure filled in with the clipping rectangle for
 *             the surface
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 * \sa SDL_SetSurfaceClipRect
 */
extern DECLSPEC int SDLCALL SDL_GetSurfaceClipRect(SDL_Surface *surface,
                                             SDL_Rect *rect);

/*
 * Creates a new surface identical to the existing surface.
 *
 * The returned surface should be freed with SDL_DestroySurface().
 *
 * \param surface the surface to duplicate.
 * \returns a copy of the surface, or NULL on failure; call SDL_GetError() for
 *          more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_DuplicateSurface(SDL_Surface *surface);

/**
 * Copy an existing surface to a new surface of the specified format.
 *
 * This function is used to optimize images for faster *repeat* blitting. This
 * is accomplished by converting the original and storing the result as a new
 * surface. The new, optimized surface can then be used as the source for
 * future blits, making them faster.
 *
 * \param surface the existing SDL_Surface structure to convert
 * \param format the SDL_PixelFormat structure that the new surface is
 *               optimized for
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreatePixelFormat
 * \sa SDL_ConvertSurfaceFormat
 * \sa SDL_CreateSurface
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_ConvertSurface(SDL_Surface *surface,
                                                        const SDL_PixelFormat *format);

/**
 * Copy an existing surface to a new surface of the specified format enum.
 *
 * This function operates just like SDL_ConvertSurface(), but accepts an
 * SDL_PixelFormatEnum value instead of an SDL_PixelFormat structure. As such,
 * it might be easier to call but it doesn't have access to palette
 * information for the destination surface, in case that would be important.
 *
 * \param surface the existing SDL_Surface structure to convert
 * \param pixel_format the SDL_PixelFormatEnum that the new surface is
 *                     optimized for
 * \returns the new SDL_Surface structure that is created or NULL if it fails;
 *          call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreatePixelFormat
 * \sa SDL_ConvertSurface
 * \sa SDL_CreateSurface
 */
extern DECLSPEC SDL_Surface *SDLCALL SDL_ConvertSurfaceFormat(SDL_Surface *surface,
                                                              Uint32 pixel_format);

/**
 * Copy a block of pixels of one format to another format.
 *
 * \param width the width of the block to copy, in pixels
 * \param height the height of the block to copy, in pixels
 * \param src_format an SDL_PixelFormatEnum value of the `src` pixels format
 * \param src a pointer to the source pixels
 * \param src_pitch the pitch of the source pixels, in bytes
 * \param dst_format an SDL_PixelFormatEnum value of the `dst` pixels format
 * \param dst a pointer to be filled in with new pixel data
 * \param dst_pitch the pitch of the destination pixels, in bytes
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_ConvertPixels(int width, int height,
                                              Uint32 src_format,
                                              const void *src, int src_pitch,
                                              Uint32 dst_format,
                                              void *dst, int dst_pitch);

/**
 * Premultiply the alpha on a block of pixels.
 *
 * This is safe to use with src == dst, but not for other overlapping areas.
 *
 * This function is currently only implemented for SDL_PIXELFORMAT_ARGB8888.
 *
 * \param width the width of the block to convert, in pixels
 * \param height the height of the block to convert, in pixels
 * \param src_format an SDL_PixelFormatEnum value of the `src` pixels format
 * \param src a pointer to the source pixels
 * \param src_pitch the pitch of the source pixels, in bytes
 * \param dst_format an SDL_PixelFormatEnum value of the `dst` pixels format
 * \param dst a pointer to be filled in with premultiplied pixel data
 * \param dst_pitch the pitch of the destination pixels, in bytes
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_PremultiplyAlpha(int width, int height,
                                                 Uint32 src_format,
                                                 const void *src, int src_pitch,
                                                 Uint32 dst_format,
                                                 void *dst, int dst_pitch);

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
 * \param dst the SDL_Surface structure that is the drawing target
 * \param rect the SDL_Rect structure representing the rectangle to fill, or
 *             NULL to fill the entire surface
 * \param color the color to fill with
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_FillSurfaceRects
 */
extern DECLSPEC int SDLCALL SDL_FillSurfaceRect
    (SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);

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
 * \param dst the SDL_Surface structure that is the drawing target
 * \param rects an array of SDL_Rects representing the rectangles to fill.
 * \param count the number of rectangles in the array
 * \param color the color to fill with
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_FillSurfaceRect
 */
extern DECLSPEC int SDLCALL SDL_FillSurfaceRects
    (SDL_Surface *dst, const SDL_Rect *rects, int count, Uint32 color);

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
 * ```c
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
 * \param src the SDL_Surface structure to be copied from
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied, or NULL to copy the entire surface
 * \param dst the SDL_Surface structure that is the blit target
 * \param dstrect the SDL_Rect structure representing the x and y position in
 *                the destination surface. On input the width and height are
 *                ignored (taken from srcrect), and on output this is filled
 *                in with the actual rectangle used after clipping.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurfaceScaled
 */
extern DECLSPEC int SDLCALL SDL_BlitSurface
    (SDL_Surface *src, const SDL_Rect *srcrect,
     SDL_Surface *dst, SDL_Rect *dstrect);

/**
 * Perform low-level surface blitting only.
 *
 * This is a semi-private blit function and it performs low-level surface
 * blitting, assuming the input rectangles have already been clipped.
 *
 * \param src the SDL_Surface structure to be copied from
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied, or NULL to copy the entire surface
 * \param dst the SDL_Surface structure that is the blit target
 * \param dstrect the SDL_Rect structure representing the target rectangle in
 *                the destination surface
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 */
extern DECLSPEC int SDLCALL SDL_BlitSurfaceUnchecked
    (SDL_Surface *src, const SDL_Rect *srcrect,
     SDL_Surface *dst, const SDL_Rect *dstrect);

/**
 * Perform stretch blit between two surfaces of the same format.
 *
 * Using SDL_SCALEMODE_NEAREST: fast, low quality. Using SDL_SCALEMODE_LINEAR:
 * bilinear scaling, slower, better quality, only 32BPP.
 *
 * \param src the SDL_Surface structure to be copied from
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied
 * \param dst the SDL_Surface structure that is the blit target
 * \param dstrect the SDL_Rect structure representing the target rectangle in
 *                the destination surface
 * \param scaleMode scale algorithm to be used
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurfaceScaled
 */
extern DECLSPEC int SDLCALL SDL_SoftStretch(SDL_Surface *src,
                                            const SDL_Rect *srcrect,
                                            SDL_Surface *dst,
                                            const SDL_Rect *dstrect,
                                            SDL_ScaleMode scaleMode);

/**
 * Perform a scaled surface copy to a destination surface.
 *
 * \param src the SDL_Surface structure to be copied from
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied
 * \param dst the SDL_Surface structure that is the blit target
 * \param dstrect the SDL_Rect structure representing the target rectangle in
 *                the destination surface, filled with the actual rectangle
 *                used after clipping
 * \param scaleMode scale algorithm to be used
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurface
 */
extern DECLSPEC int SDLCALL SDL_BlitSurfaceScaled(SDL_Surface *src,
                                                  const SDL_Rect *srcrect,
                                                  SDL_Surface *dst,
                                                  SDL_Rect *dstrect,
                                                  SDL_ScaleMode scaleMode);

/**
 * Perform low-level surface scaled blitting only.
 *
 * This is a semi-private function and it performs low-level surface blitting,
 * assuming the input rectangles have already been clipped.
 *
 * \param src the SDL_Surface structure to be copied from
 * \param srcrect the SDL_Rect structure representing the rectangle to be
 *                copied
 * \param dst the SDL_Surface structure that is the blit target
 * \param dstrect the SDL_Rect structure representing the target rectangle in
 *                the destination surface
 * \param scaleMode scale algorithm to be used
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_BlitSurfaceScaled
 */
extern DECLSPEC int SDLCALL SDL_BlitSurfaceUncheckedScaled(SDL_Surface *src,
                                                           const SDL_Rect *srcrect,
                                                           SDL_Surface *dst,
                                                           const SDL_Rect *dstrect,
                                                           SDL_ScaleMode scaleMode);

/**
 * Set the YUV conversion mode
 *
 * \param mode YUV conversion mode
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC void SDLCALL SDL_SetYUVConversionMode(SDL_YUV_CONVERSION_MODE mode);

/**
 * Get the YUV conversion mode
 *
 * \returns YUV conversion mode
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_YUV_CONVERSION_MODE SDLCALL SDL_GetYUVConversionMode(void);

/**
 * Get the YUV conversion mode, returning the correct mode for the resolution
 * when the current conversion mode is SDL_YUV_CONVERSION_AUTOMATIC
 *
 * \param width width
 * \param height height
 * \returns YUV conversion mode
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_YUV_CONVERSION_MODE SDLCALL SDL_GetYUVConversionModeForResolution(int width, int height);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_surface_h_ */
