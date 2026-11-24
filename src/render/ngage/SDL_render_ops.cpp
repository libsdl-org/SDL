/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_render_ops.hpp"
#include <3dtypes.h>

void ApplyColorMod(void *dest, void *source, int pitch, int width, int height, SDL_FColor color)
{
    TUint16 *src_pixels = static_cast<TUint16 *>(source);
    TUint16 *dst_pixels = static_cast<TUint16 *>(dest);

    // Fast path: no color modulation (white color).
    if (color.r == 1.0f && color.g == 1.0f && color.b == 1.0f) {
        if (dest != source) {
            for (int y = 0; y < height; ++y) {
                TUint16 *src_row = src_pixels + (y * pitch / 2);
                TUint16 *dst_row = dst_pixels + (y * pitch / 2);
                for (int x = 0; x < width; ++x) {
                    dst_row[x] = src_row[x];
                }
            }
        }
        return;
    }

    TFixed rf = Real2Fix(color.r);
    TFixed gf = Real2Fix(color.g);
    TFixed bf = Real2Fix(color.b);

    int pitch_offset = pitch / 2;

    for (int y = 0; y < height; ++y) {
        int row_offset = y * pitch_offset;
        for (int x = 0; x < width; ++x) {
            int idx = row_offset + x;
            TUint16 pixel = src_pixels[idx];
            TUint8 r = (pixel & 0xF800) >> 8;
            TUint8 g = (pixel & 0x07E0) >> 3;
            TUint8 b = (pixel & 0x001F) << 3;
            r = FixMul(r, rf);
            g = FixMul(g, gf);
            b = FixMul(b, bf);
            dst_pixels[idx] = (r << 8) | (g << 3) | (b >> 3);
        }
    }
}

void BlitWithAlphaKey(CFbsBitmap *dst, int dst_x, int dst_y, const void *src, int src_width, int src_height, int src_stride_u16)
{
    // Write only non-transparent (alpha != 0) source pixels into the destination bitmap.
    // Pixels with alpha nibble == 0 are treated as color-key (transparent) and skipped.
    TUint16 *dst_data = static_cast<TUint16 *>(static_cast<void *>(dst->DataAddress()));
    const TUint16 *src_pixels = static_cast<const TUint16 *>(src);
    TSize dst_size = dst->SizeInPixels();
    int dst_stride_u16 = CFbsBitmap::ScanLineLength(dst_size.iWidth, EColor4K) / 2;

    for (int y = 0; y < src_height; ++y) {
        int dy = dst_y + y;
        if (dy < 0 || dy >= dst_size.iHeight) {
            continue;
        }
        const TUint16 *src_row = src_pixels + y * src_stride_u16;
        TUint16 *dst_row = dst_data + dy * dst_stride_u16;
        for (int x = 0; x < src_width; ++x) {
            int dx = dst_x + x;
            if (dx < 0 || dx >= dst_size.iWidth) {
                continue;
            }
            TUint16 p = src_row[x];
            if (p & 0xF000) {
                // Strip the alpha nibble and write the RGB444 pixel.
                dst_row[dx] = p & 0x0FFF;
            }
        }
    }
}

void ApplyFlip(void *dest, void *source, int pitch, int width, int height, SDL_FlipMode flip)
{
    TUint16 *src_pixels = static_cast<TUint16 *>(source);
    TUint16 *dst_pixels = static_cast<TUint16 *>(dest);

    // Fast path: no flip.
    if (flip == SDL_FLIP_NONE) {
        if (dest != source) {
            for (int y = 0; y < height; ++y) {
                TUint16 *src_row = src_pixels + (y * pitch / 2);
                TUint16 *dst_row = dst_pixels + (y * pitch / 2);
                for (int x = 0; x < width; ++x) {
                    dst_row[x] = src_row[x];
                }
            }
        }
        return;
    }

    int pitch_offset = pitch / 2;

    // Fast path: horizontal flip only.
    if (flip == SDL_FLIP_HORIZONTAL) {
        int width_minus_1 = width - 1;
        for (int y = 0; y < height; ++y) {
            int row_offset = y * pitch_offset;
            if (dest == source) {
                for (int x = 0; x < width / 2; ++x) {
                    TUint16 tmp = src_pixels[row_offset + x];
                    src_pixels[row_offset + x] = src_pixels[row_offset + (width_minus_1 - x)];
                    src_pixels[row_offset + (width_minus_1 - x)] = tmp;
                }
            } else {
                for (int x = 0; x < width; ++x) {
                    dst_pixels[row_offset + x] = src_pixels[row_offset + (width_minus_1 - x)];
                }
            }
        }
        return;
    }

    // Fast path: vertical flip only.
    if (flip == SDL_FLIP_VERTICAL) {
        int height_minus_1 = height - 1;
        for (int y = 0; y < height; ++y) {
            int dst_row_offset = y * pitch_offset;
            int src_row_offset = (height_minus_1 - y) * pitch_offset;
            for (int x = 0; x < width; ++x) {
                dst_pixels[dst_row_offset + x] = src_pixels[src_row_offset + x];
            }
        }
        return;
    }

    // Both horizontal and vertical flip
    int width_minus_1 = width - 1;
    int height_minus_1 = height - 1;
    if (dest == source) {
        // Swap pixels across the center point.
        for (int y = 0; y < (height + 1) / 2; ++y) {
            int top_offset = y * pitch_offset;
            int bot_offset = (height_minus_1 - y) * pitch_offset;
            int x_limit = (y == height_minus_1 - y) ? width / 2 : width;
            for (int x = 0; x < x_limit; ++x) {
                TUint16 tmp = src_pixels[top_offset + x];
                src_pixels[top_offset + x] = src_pixels[bot_offset + (width_minus_1 - x)];
                src_pixels[bot_offset + (width_minus_1 - x)] = tmp;
            }
        }
    } else {
        for (int y = 0; y < height; ++y) {
            int dst_row_offset = y * pitch_offset;
            int src_row_offset = (height_minus_1 - y) * pitch_offset;
            for (int x = 0; x < width; ++x) {
                dst_pixels[dst_row_offset + x] = src_pixels[src_row_offset + (width_minus_1 - x)];
            }
        }
    }
}

void ApplyRotation(void *dest, void *source, int pitch, int width, int height, TFixed center_x, TFixed center_y, TFixed angle)
{
    TUint16 *src_pixels = static_cast<TUint16 *>(source);
    TUint16 *dst_pixels = static_cast<TUint16 *>(dest);

    // Fast path: no rotation.
    if (angle == 0) {
        if (dest != source) {
            int pitch_offset = pitch / 2;
            for (int y = 0; y < height; ++y) {
                TUint16 *src_row = src_pixels + (y * pitch_offset);
                TUint16 *dst_row = dst_pixels + (y * pitch_offset);
                for (int x = 0; x < width; ++x) {
                    dst_row[x] = src_row[x];
                }
            }
        }
        return;
    }

    // Fast paths for 90-degree rotations
    TFixed angle_90 = Int2Fix(90);
    TFixed angle_180 = Int2Fix(180);
    TFixed angle_270 = Int2Fix(270);
    TFixed angle_360 = Int2Fix(360);

    // Normalize angle to 0-360 range
    TFixed normalized_angle = angle;
    while (normalized_angle < 0) {
        normalized_angle += angle_360;
    }
    while (normalized_angle >= angle_360) {
        normalized_angle -= angle_360;
    }

    int pitch_offset = pitch / 2;

    // Fast path: 90-degree rotation (clockwise).
    if (normalized_angle == angle_90) {
        TFixed center_x_int = Fix2Int(center_x);
        TFixed center_y_int = Fix2Int(center_y);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Translate to origin.
                int tx = x - center_x_int;
                int ty = y - center_y_int;
                // Rotate 90 degrees clockwise: (x, y) -> (y, -x).
                int rx = ty;
                int ry = -tx;
                // Translate back.
                int src_x = rx + center_x_int;
                int src_y = ry + center_y_int;
                if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                    dst_pixels[y * pitch_offset + x] = src_pixels[src_y * pitch_offset + src_x];
                } else {
                    dst_pixels[y * pitch_offset + x] = 0;
                }
            }
        }
        return;
    }

    // Fast path: 180-degree rotation.
    if (normalized_angle == angle_180) {
        TFixed center_x_int = Fix2Int(center_x);
        TFixed center_y_int = Fix2Int(center_y);
        for (int y = 0; y < height; ++y) {
            int dst_row_offset = y * pitch_offset;
            for (int x = 0; x < width; ++x) {
                // Translate to origin
                int tx = x - center_x_int;
                int ty = y - center_y_int;
                // Rotate 180 degrees: (x, y) -> (-x, -y)
                int rx = -tx;
                int ry = -ty;
                // Translate back
                int src_x = rx + center_x_int;
                int src_y = ry + center_y_int;
                if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                    dst_pixels[dst_row_offset + x] = src_pixels[src_y * pitch_offset + src_x];
                } else {
                    dst_pixels[dst_row_offset + x] = 0;
                }
            }
        }
        return;
    }

    // Fast path: 270-degree rotation (clockwise).
    if (normalized_angle == angle_270) {
        TFixed center_x_int = Fix2Int(center_x);
        TFixed center_y_int = Fix2Int(center_y);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Translate to origin.
                int tx = x - center_x_int;
                int ty = y - center_y_int;
                // Rotate 270 degrees clockwise (or 90 counter-clockwise): (x, y) -> (-y, x).
                int rx = -ty;
                int ry = tx;
                // Translate back.
                int src_x = rx + center_x_int;
                int src_y = ry + center_y_int;
                if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                    dst_pixels[y * pitch_offset + x] = src_pixels[src_y * pitch_offset + src_x];
                } else {
                    dst_pixels[y * pitch_offset + x] = 0;
                }
            }
        }
        return;
    }

    TFixed cos_angle = 0;
    TFixed sin_angle = 0;
    FixSinCos(angle, sin_angle, cos_angle);

    // Pre-calculate the translation of center to origin.
    TFixed neg_center_x = -center_x;
    TFixed neg_center_y = -center_y;

    for (int y = 0; y < height; ++y) {
        int dst_row_offset = y * pitch_offset;
        TFixed y_fixed = Int2Fix(y) + neg_center_y;

        // Pre-calculate these values for the entire row.
        TFixed cos_mul_ty = FixMul(y_fixed, cos_angle);
        TFixed sin_mul_ty = FixMul(y_fixed, sin_angle);

        // Starting position for the row (x=0).
        // rotated_x = cos(angle) * (0 - center_x) + sin(angle) * (y - center_y) + center_x
        // rotated_y = cos(angle) * (y - center_y) - sin(angle) * (0 - center_x) + center_y
        TFixed rotated_x = sin_mul_ty + center_x + FixMul(neg_center_x, cos_angle);
        TFixed rotated_y = cos_mul_ty + center_y - FixMul(neg_center_x, sin_angle);

        for (int x = 0; x < width; ++x) {
            // Convert to integer coordinates.
            int final_x = Fix2Int(rotated_x);
            int final_y = Fix2Int(rotated_y);

            // Check bounds and copy pixel.
            if (final_x >= 0 && final_x < width && final_y >= 0 && final_y < height) {
                dst_pixels[dst_row_offset + x] = src_pixels[final_y * pitch_offset + final_x];
            } else {
                dst_pixels[dst_row_offset + x] = 0;
            }

            // Increment to next pixel (add rotation matrix column).
            rotated_x += cos_angle;
            rotated_y -= sin_angle;
        }
    }
}

void ApplyScale(void *dest, void *source, int pitch, int width, int height, TFixed center_x, TFixed center_y, TFixed scale_x, TFixed scale_y)
{
    TUint16 *src_pixels = static_cast<TUint16 *>(source);
    TUint16 *dst_pixels = static_cast<TUint16 *>(dest);

    TFixed one_fixed = Int2Fix(1);

    // Fast path: no scaling (1.0x scale).
    if (scale_x == one_fixed && scale_y == one_fixed) {
        if (dest != source) {
            for (int y = 0; y < height; ++y) {
                TUint16 *src_row = src_pixels + (y * pitch / 2);
                TUint16 *dst_row = dst_pixels + (y * pitch / 2);
                for (int x = 0; x < width; ++x) {
                    dst_row[x] = src_row[x];
                }
            }
        }
        return;
    }

    int pitch_offset = pitch / 2;

    for (int y = 0; y < height; ++y) {
        int dst_row_offset = y * pitch_offset;
        TFixed y_fixed = Int2Fix(y);
        TFixed translated_y = y_fixed - center_y;
        TFixed scaled_y = FixDiv(translated_y, scale_y);

        for (int x = 0; x < width; ++x) {
            // Translate point to origin.
            TFixed translated_x = Int2Fix(x) - center_x;

            // Scale point.
            TFixed scaled_x = FixDiv(translated_x, scale_x);

            // Translate point back.
            int final_x = Fix2Int(scaled_x + center_x);
            int final_y = Fix2Int(scaled_y + center_y);

            // Check bounds.
            if (final_x >= 0 && final_x < width && final_y >= 0 && final_y < height) {
                dst_pixels[dst_row_offset + x] = src_pixels[final_y * pitch_offset + final_x];
            } else {
                dst_pixels[dst_row_offset + x] = 0;
            }
        }
    }
}
