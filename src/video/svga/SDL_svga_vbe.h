/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_svga_vbe_h_
#define SDL_svga_vbe_h_

#include "../SDL_sysvideo.h"

typedef Uint16 VBEMode;

#define VBE_MODE_LIST_END 0xFFFF

typedef struct VBEFarPtr {
    Uint16 offset;
    Uint16 segment;
} __attribute__ ((packed)) VBEFarPtr;

SDL_COMPILE_TIME_ASSERT(VBEFarPtr, sizeof(VBEFarPtr) == 4);

typedef struct VBEVersion {
    Uint8 minor;
    Uint8 major;
} __attribute__ ((packed)) VBEVersion;

SDL_COMPILE_TIME_ASSERT(VBEVersion, sizeof(VBEVersion) == 2);

typedef struct VBEInfo
{
    char vbe_signature[4];          /* "VESA" 4 byte signature */
    VBEVersion vbe_version;         /* VBE version number */
    VBEFarPtr oem_string_ptr;       /* Pointer to OEM string */
    Uint32 capabilities;            /* Capabilities of video card */
    VBEFarPtr video_mode_ptr;       /* Pointer to supported modes */
    Uint16 total_memory;            /* Number of 64kb memory blocks */

    /* VBE 2.0 and above: */
    Uint16 oem_software_rev;        /* OEM Software revision number */
    VBEFarPtr oem_vendor_name_ptr;  /* Pointer to vendor name string */
    VBEFarPtr oem_product_name_ptr; /* Pointer to product name string */
    VBEFarPtr oem_product_rev_ptr;  /* Pointer to product revision string */
    Uint8 reserved[222];            /* VBE implementation scratch data */
    char oem_data[256];             /* Data for OEM strings */
} __attribute__ ((packed)) VBEInfo;

SDL_COMPILE_TIME_ASSERT(VBEInfo, sizeof(VBEInfo) == 512);

typedef struct VBEModeInfo
{
    Uint16 mode_attributes;       /* Mode attributes */
    Uint8 win_a_attributes;       /* Window A attributes */
    Uint8 win_b_attributes;       /* Window B attributes */
    Uint16 win_granularity;       /* Window granularity */
    Uint16 win_size;              /* Window size */
    Uint16 win_a_segment;         /* Window A start segment */
    Uint16 win_b_segment;         /* Window B start segment */
    VBEFarPtr win_func_ptr;       /* Pointer to window function */
    Uint16 bytes_per_scan_line;   /* Bytes per scan line */

    /* VBE 1.2 and above: */
    Uint16 x_resolution;          /* Horizontal resolution in pixels or chars */
    Uint16 y_resolution;          /* Vertical resolution in pixels or chars */
    Uint8 x_char_size;            /* Character cell width in pixels */
    Uint8 y_char_size;            /* Character cell height in pixels */
    Uint8 number_of_planes;       /* Number of memory planes */
    Uint8 bits_per_pixel;         /* Bits per pixel */
    Uint8 number_of_banks;        /* Number of banks */
    Uint8 memory_model;           /* Memory model type */
    Uint8 bank_size;              /* Bank size in KiB */
    Uint8 number_of_image_pages;  /* Number of images */
    Uint8 reserved;               /* Reserved for page function */

    /* Direct color fields (required for direct/6 and YUV/7 memory models) */
    Uint8 red_mask_size;          /* Size of direct color red mask in bits */
    Uint8 red_field_position;     /* Bit position of lsb of red mask */
    Uint8 green_mask_size;        /* Size of direct color green mask in bits */
    Uint8 green_field_position;   /* Bit position of lsb of green mask */
    Uint8 blue_mask_size;         /* Size of direct color blue mask in bits */
    Uint8 blue_field_position;    /* Bit position of lsb of blue mask */
    Uint8 rsvd_mask_size;         /* Size of direct color reserved mask in bits */
    Uint8 rsvd_field_position;    /* Bit position of lsb of reserved mask */
    Uint8 direct_color_mode_info; /* Direct color mode attributes */

    /* VBE 2.0 and above: */
    VBEFarPtr phys_base_ptr;      /* Physical address for flat frame buffer */
    Uint32 off_screen_mem_offset; /* Offset to start of off screen memory */
    Uint16 off_screen_mem_size;   /* Amount of off screen memory in KiB */

    /* VBE 3.0 and above: */
    Uint16 lin_bytes_per_scan_line;  /* Bytes per scan line for linear modes */
    Uint8 bnk_number_of_image_pages; /* Number of images for banked modes */
    Uint8 lin_number_of_image_pages; /* Number of images for linear modes */
    Uint8 lin_red_mask_size;         /* Size of direct color red mask (linear modes) */
    Uint8 lin_red_field_position;    /* Bit position of lsb of red mask (linear modes) */
    Uint8 lin_green_mask_size;       /* Size of direct color green mask (linear modes) */
    Uint8 lin_green_field_position;  /* Bit position of lsb of green mask (linear modes) */
    Uint8 lin_blue_mask_size;        /* Size of direct color blue mask (linear modes) */
    Uint8 lin_blue_field_position;   /* Bit position of lsb of blue mask (linear modes) */
    Uint8 lin_rsvd_mask_size;        /* Size of direct color reserved mask (linear modes) */
    Uint8 lin_rsvd_field_position;   /* Bit position of lsb of reserved mask (linear modes) */
    Uint32 max_pixel_clock;          /* Maximum pixel clock (in Hz) for graphics mode */

    Uint8 reserved_end[190];
} __attribute__ ((packed)) VBEModeInfo;

SDL_COMPILE_TIME_ASSERT(VBEModeInfo, sizeof(VBEModeInfo) == 256);

/* Mode attribute bit flags */
#define VBE_MODE_ATTR_HARDWARE_SUPPORT 0x0001
#define VBE_MODE_ATTR_TTY_BIOS_SUPPORT 0x0004
#define VBE_MODE_ATTR_COLOR_MODE       0x0008
#define VBE_MODE_ATTR_GRAPHICS_MODE    0x0010
#define VBE_MODE_ATTR_NO_VGA_COMPAT    0x0020
#define VBE_MODE_ATTR_NO_WINDOWED_MEM  0x0040
#define VBE_MODE_ATTR_LINEAR_MEM_AVAIL 0x0080

extern int SVGA_GetVBEInfo(VBEInfo * info);
extern VBEMode SVGA_GetVBEModeAtIndex(const VBEInfo * info, int index);
extern int SVGA_GetVBEModeInfo(VBEMode mode, VBEModeInfo * info);
extern int SVGA_GetCurrentVBEMode(VBEMode * mode, VBEModeInfo * info);

#endif /* SDL_svga_vbe_h_ */

/* vi: set ts=4 sw=4 expandtab: */
