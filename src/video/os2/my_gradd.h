/*
gradd.h structures and constants -- only the ones used by SDL_os2vman.c.

Based on public knowledge from around the internet including pages from
http://www.osfree.org and http://www.edm2.com
*/

#ifndef my_gradd_h_
#define my_gradd_h_

typedef struct _INITPROCOUT {
  ULONG     ulLength;         /*  Length of the INITPROCOUT data structure, in bytes. */
  ULONG     ulVRAMVirt;       /*  32-bit virtual address of VRAM. */
} INITPROCOUT;
typedef INITPROCOUT *PINITPROCOUT;

#define RC_SUCCESS 0

typedef ULONG GID;
typedef ULONG (_System FNVMIENTRY) (
  GID       gid, ULONG ulFunction,
  PVOID     pIn,
  PVOID     pOut /* PINITPROCOUT */
);

#define VMI_CMD_INITPROC 1
#define VMI_CMD_TERMPROC 3
#define VMI_CMD_BITBLT 8
#define VMI_CMD_REQUESTHW 14
#define VMI_CMD_QUERYCURRENTMODE 0x1001

typedef struct _BMAPINFO {
  ULONG     ulLength;         /* Length of the BMAPINFO data structure, in bytes. */
  ULONG     ulType;           /* Description of the Blt. */
  ULONG     ulWidth;          /* Width in pels of the bit map. */
  ULONG     ulHeight;         /* Height in pels of the bit map. */
  ULONG     ulBpp;            /* Number of bits per pel/color depth. */
  ULONG     ulBytesPerLine;   /* Number of aligned bytes per line. */
  PBYTE     pBits;            /* Pointer to bit-map bits. */
} BMAPINFO;
typedef BMAPINFO *PBMAPINFO;

#define BMAP_VRAM 0
#define BMAP_MEMORY 1

typedef struct _BLTRECT {
  ULONG     ulXOrg;           /* X origin of the destination Blt. */
  ULONG     ulYOrg;           /* Y origin of the destination Blt. */
  ULONG     ulXExt;           /* X extent of the BitBlt. */
  ULONG     ulYExt;           /* Y extent of the BitBlt. */
} BLTRECT;
typedef BLTRECT *PBLTRECT;

typedef struct _BITBLTINFO {
  ULONG     ulLength;         /* Length of the BITBLTINFO data structure, in bytes. */
  ULONG     ulBltFlags;       /* Flags for rendering of rasterized data. */
  ULONG     cBlits;           /* Count of Blts to be performed. */
  ULONG     ulROP;            /* Raster operation. */
  ULONG     ulMonoBackROP;    /* Background mix if B_APPLY_BACK_ROP is set. */
  ULONG     ulSrcFGColor;     /* Monochrome source Foreground color. */
  ULONG     ulSrcBGColor;     /* Monochrome source Background color and transparent color. */
  ULONG     ulPatFGColor;     /* Monochrome pattern Foreground color. */
  ULONG     ulPatBGColor;     /* Monochrome pattern Background color. */
  PBYTE     abColors;         /* Pointer to color translation table. */
  PBMAPINFO pSrcBmapInfo;     /* Pointer to source bit map (BMAPINFO) */
  PBMAPINFO pDstBmapInfo;     /* Pointer to destination bit map (BMAPINFO). */
  PBMAPINFO pPatBmapInfo;     /* Pointer to pattern bit map (BMAPINFO). */
  PPOINTL   aptlSrcOrg;       /* Pointer to array of source origin POINTLs. */
  PPOINTL   aptlPatOrg;       /* Pointer to array of pattern origin POINTLs. */
  PBLTRECT  abrDst;           /* Pointer to array of Blt rects. */
  PRECTL    prclSrcBounds;    /* Pointer to source bounding rect of source Blts. */
  PRECTL    prclDstBounds;    /* Pointer to destination bounding rect of destination Blts. */
} BITBLTINFO;
typedef BITBLTINFO *PBITBLTINFO;

#define BF_DEFAULT_STATE 0x0
#define BF_ROP_INCL_SRC (0x01 << 2)
#define BF_PAT_HOLLOW   (0x01 << 8)

typedef struct _GDDMODEINFO {
  ULONG     ulLength;         /* Size of the GDDMODEINFO data structure, in bytes. */
  ULONG     ulModeId;         /* ID used to make SETMODE request. */
  ULONG     ulBpp;            /* Number of colors (bpp). */
  ULONG     ulHorizResolution;/* Number of horizontal pels. */
  ULONG     ulVertResolution; /* Number of vertical scan lines. */
  ULONG     ulRefreshRate;    /* Refresh rate in Hz. */
  PBYTE     pbVRAMPhys;       /* Physical address of VRAM. */
  ULONG     ulApertureSize;   /* Size of VRAM, in bytes. */
  ULONG     ulScanLineSize;   /* Size of one scan line, in bytes. */

  ULONG     fccColorEncoding, ulTotalVRAMSize, cColors;
} GDDMODEINFO;
typedef GDDMODEINFO *PGDDMODEINFO;

typedef struct _HWREQIN {
  ULONG     ulLength;         /* Size of the HWREQIN data structure, in bytes. */
  ULONG     ulFlags;          /* Request option flags. */
  ULONG     cScrChangeRects;  /* Count of screen rectangles affected by HWREQIN. */
  PRECTL    arectlScreen;     /* Array of screen rectangles affected by HWREQIN. */
} HWREQIN;
typedef HWREQIN *PHWREQIN;

#define REQUEST_HW 0x01

#endif /* my_gradd_h_ */
