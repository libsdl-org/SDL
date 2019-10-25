/*
 * Copyright (c) 2016 RISC OS Open Ltd
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/* Prevent the stack from becoming executable */
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif

	.text
	.arch armv6
	.object_arch armv4
	.arm
	.altmacro
	.p2align 2

#include "pixman-arm-asm.h"
#include "pixman-arm-simd-asm.h"

/* A head macro should do all processing which results in an output of up to
 * 16 bytes, as far as the final load instruction. The corresponding tail macro
 * should complete the processing of the up-to-16 bytes. The calling macro will
 * sometimes choose to insert a preload or a decrement of X between them.
 *   cond           ARM condition code for code block
 *   numbytes       Number of output bytes that should be generated this time
 *   firstreg       First WK register in which to place output
 *   unaligned_src  Whether to use non-wordaligned loads of source image
 *   unaligned_mask Whether to use non-wordaligned loads of mask image
 *   preload        If outputting 16 bytes causes 64 bytes to be read, whether an extra preload should be output
 */

/******************************************************************************/

/* This differs from the over_8888_8888 routine in Pixman in that the destination
 * alpha component is always left unchanged, and RGB components are not
 * premultiplied by alpha. It differs from BlitRGBtoRGBPixelAlpha in that
 * renormalisation is done by multiplying by 257/256 (with rounding) rather than
 * simply shifting right by 8 bits - removing the need to special-case alpha=0xff.
 */

.macro RGBtoRGBPixelAlpha_init
        line_saved_regs STRIDE_S, ORIG_W
        mov     MASK, #0x80
.endm

.macro RGBtoRGBPixelAlpha_1pixel_translucent  s, d, tmp0, tmp1, tmp2, tmp3, half
        uxtb    tmp3, s
        uxtb    tmp0, d
        sub     tmp0, tmp3, tmp0
        uxtb    tmp3, s, ror #16
        uxtb    tmp1, d, ror #16
        sub     tmp1, tmp3, tmp1
        uxtb    tmp3, s, ror #8
        mov     s, s, lsr #24
        uxtb    tmp2, d, ror #8
        sub     tmp2, tmp3, tmp2
        smlabb  tmp0, tmp0, s, half
        smlabb  tmp1, tmp1, s, half
        smlabb  tmp2, tmp2, s, half
        add     tmp0, tmp0, asr #8
        add     tmp1, tmp1, asr #8
        add     tmp2, tmp2, asr #8
        pkhbt   tmp0, tmp0, tmp1, lsl #16
        and     tmp2, tmp2, #0xff00
        uxtb16  tmp0, tmp0, ror #8
        orr     tmp0, tmp0, tmp2
        uadd8   d, d, tmp0
.endm

.macro RGBtoRGBPixelAlpha_1pixel_opaque  s, d
        and     d, d, #0xff000000
        bic     s, s, #0xff000000
        orr     d, d, s
.endm

.macro RGBtoRGBPixelAlpha_process_head  cond, numbytes, firstreg, unaligned_src, unaligned_mask, preload
 .if numbytes == 16
        ldm     SRC!, {WK0, WK1}
        ldm     SRC!, {STRIDE_S, STRIDE_M}
        ldrd    WK2, WK3, [DST], #16
        orr     SCRATCH, WK0, WK1
        and     ORIG_W, WK0, WK1
        orr     SCRATCH, SCRATCH, STRIDE_S
        and     ORIG_W, ORIG_W, STRIDE_S
        orr     SCRATCH, SCRATCH, STRIDE_M
        and     ORIG_W, ORIG_W, STRIDE_M
        tst     SCRATCH, #0xff000000
 .elseif numbytes == 8
        ldm     SRC!, {WK0, WK1}
        ldm     DST!, {WK2, WK3}
        orr     SCRATCH, WK0, WK1
        and     ORIG_W, WK0, WK1
        tst     SCRATCH, #0xff000000
 .else // numbytes == 4
        ldr     WK0, [SRC], #4
        ldr     WK2, [DST], #4
        tst     WK0, #0xff000000
 .endif
.endm

.macro RGBtoRGBPixelAlpha_process_tail  cond, numbytes, firstreg
        beq     20f @ all transparent
 .if numbytes == 16
        cmp     ORIG_W, #0xff000000
        bhs     10f @ all opaque
        RGBtoRGBPixelAlpha_1pixel_translucent WK0, WK2, STRIDE_S, STRIDE_M, SCRATCH, ORIG_W, MASK
        RGBtoRGBPixelAlpha_1pixel_translucent WK1, WK3, STRIDE_S, STRIDE_M, SCRATCH, ORIG_W, MASK
        strd    WK2, WK3, [DST, #-16]
        ldrd    WK0, WK1, [SRC, #-8]
        ldrd    WK2, WK3, [DST, #-8]
        RGBtoRGBPixelAlpha_1pixel_translucent WK0, WK2, STRIDE_S, STRIDE_M, SCRATCH, ORIG_W, MASK
        RGBtoRGBPixelAlpha_1pixel_translucent WK1, WK3, STRIDE_S, STRIDE_M, SCRATCH, ORIG_W, MASK
        b       19f
10:     RGBtoRGBPixelAlpha_1pixel_opaque WK0, WK2
        RGBtoRGBPixelAlpha_1pixel_opaque WK1, WK3
        strd    WK2, WK3, [DST, #-16]
        ldrd    WK0, WK1, [SRC, #-8]
        ldrd    WK2, WK3, [DST, #-8]
        RGBtoRGBPixelAlpha_1pixel_opaque WK0, WK2
        RGBtoRGBPixelAlpha_1pixel_opaque WK1, WK3
19:     strd    WK2, WK3, [DST, #-8]
 .elseif numbytes == 8
        cmp     ORIG_W, #0xff000000
        bhs     10f @ all opaque
        RGBtoRGBPixelAlpha_1pixel_translucent WK0, WK2, STRIDE_S, STRIDE_M, SCRATCH, ORIG_W, MASK
        RGBtoRGBPixelAlpha_1pixel_translucent WK1, WK3, STRIDE_S, STRIDE_M, SCRATCH, ORIG_W, MASK
        b       19f
10:     RGBtoRGBPixelAlpha_1pixel_opaque WK0, WK2
        RGBtoRGBPixelAlpha_1pixel_opaque WK1, WK3
19:     strd    WK2, WK3, [DST, #-8]
 .else // numbytes == 4
        cmp     WK0, #0xff000000
        bhs     10f @ opaque
        RGBtoRGBPixelAlpha_1pixel_translucent WK0, WK2, STRIDE_S, STRIDE_M, SCRATCH, ORIG_W, MASK
        b       19f
10:     RGBtoRGBPixelAlpha_1pixel_opaque WK0, WK2
19:     str     WK2, [DST, #-4]
 .endif
20:
.endm

generate_composite_function \
    BlitRGBtoRGBPixelAlphaARMSIMDAsm, 32, 0, 32, \
    FLAG_DST_READWRITE | FLAG_BRANCH_OVER | FLAG_PROCESS_CORRUPTS_PSR | FLAG_PROCESS_DOES_STORE | FLAG_SPILL_LINE_VARS | FLAG_PROCESS_CORRUPTS_WK0, \
    2, /* prefetch distance */ \
    RGBtoRGBPixelAlpha_init, \
    nop_macro, /* newline */ \
    nop_macro, /* cleanup */ \
    RGBtoRGBPixelAlpha_process_head, \
    RGBtoRGBPixelAlpha_process_tail

/******************************************************************************/
