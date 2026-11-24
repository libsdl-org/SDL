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

#ifndef SDL_dos_h_
#define SDL_dos_h_

#include <sys/farptr.h>

// our MS-DOS port depends on the "fat DS" trick. djgpp docs try to warn you
// away from this, but if it was good enough for Quake 1, it's good enough
// for us!
#include <sys/nearptr.h>

// We are obviously a 32-bit protected mode (DPMI) program.
#include <dpmi.h>

// this is djgpp-specific.
#include <go32.h>

// this is DOS PC stuff, like interrupts and Intel i/o ports.
#include <pc.h>

// 8259 PIC (Programmable Interrupt Controller) ports and commands
#define PIC1_COMMAND 0x20 // master PIC command port
#define PIC1_DATA    0x21 // master PIC data (mask) port
#define PIC2_COMMAND 0xA0 // slave PIC command port
#define PIC2_DATA    0xA1 // slave PIC data (mask) port
#define PIC_EOI      0x20 // end-of-interrupt command

// Lock a range of code so it won't be paged out during interrupts.
// Usage: DOS_LockCode(function_name, function_end_label)
// The function_end_label must be defined immediately after the function.
#define DOS_LockCode(start, end) \
    _go32_dpmi_lock_code((void *)(start), (char *)(end) - (char *)(start))

// Lock a range of data so it won't be paged out during interrupts.
#define DOS_LockData(var, size) \
    _go32_dpmi_lock_data((void *)&(var), (size))

// Lock a single variable.
#define DOS_LockVariable(var) \
    DOS_LockData(var, sizeof(var))

// Set up for C function definitions, even when using C++
#ifdef __cplusplus
extern "C" {
#endif

// This uses the "fat DS" trick to convert a physical address to a valid
//  C pointer usable from protected mode.
SDL_FORCE_INLINE void *DOS_PhysicalToLinear(const Uint32 physical)
{
    __djgpp_nearptr_enable(); // We need to re-enable this for large applications to work.
    return (void *)(physical + __djgpp_conventional_base);
}

SDL_FORCE_INLINE Uint32 DOS_LinearToPhysical(void *linear)
{
    return ((Uint32)linear) - __djgpp_conventional_base;
}

SDL_FORCE_INLINE int DOS_IRQToVector(int irq)
{
    return irq + ((irq > 7) ? 104 : 8);
}

SDL_FORCE_INLINE void DOS_DisableInterrupts(void)
{
    __asm__ __volatile__("cli\n");
}

SDL_FORCE_INLINE void DOS_EnableInterrupts(void)
{
    __asm__ __volatile__("sti\n");
}

// Grab a single byte from a segment:offset.
SDL_FORCE_INLINE Uint8 DOS_PeekUint8(const Uint32 segoffset)
{
    return (Uint8)_farpeekb(_dos_ds, ((segoffset & 0xFFFF0000) >> 12) + (segoffset & 0xFFFF));
}

// Grab a single 16-bit word from a segment:offset.
SDL_FORCE_INLINE Uint16 DOS_PeekUint16(const Uint32 segoffset)
{
    return (Uint16)_farpeekw(_dos_ds, ((segoffset & 0xFFFF0000) >> 12) + (segoffset & 0xFFFF));
}

// Grab a single 32-bit dword from a segment:offset.
SDL_FORCE_INLINE Uint32 DOS_PeekUint32(const Uint32 segoffset)
{
    return (Uint32)_farpeekl(_dos_ds, ((segoffset & 0xFFFF0000) >> 12) + (segoffset & 0xFFFF));
}

SDL_FORCE_INLINE void DOS_EndOfInterrupt(int irq)
{
    if (irq > 7) {
        outportb(PIC2_COMMAND, PIC_EOI);
    }
    outportb(PIC1_COMMAND, PIC_EOI);
}

// Allocate memory under the 640k line; various real mode services and DMA transfers need this.
//  malloc() returns data above 640k because we're a protected mode, 32-bit process, so this is
//  only for specific needs.
extern void *DOS_AllocateConventionalMemory(const int len, _go32_dpmi_seginfo *seginfo);

// Allocate conventional memory suitable for DMA transfers.
extern void *DOS_AllocateDMAMemory(const int len, _go32_dpmi_seginfo *seginfo);

// Free conventional (or DMA, which _is_ conventional) memory.
extern void DOS_FreeConventionalMemory(_go32_dpmi_seginfo *seginfo);

// Get a SDL_malloc'd copy of a null-terminated string located at a real-mode segment:offset. This makes no promises about character encoding.
char *DOS_GetFarPtrCString(const Uint32 segoffset);

typedef void (*DOS_InterruptHookFn)(void);
typedef struct DOS_InterruptHook
{
    DOS_InterruptHookFn fn;
    int irq;
    int interrupt_vector; // this is the _vector_ number, not the IRQ number!
    _go32_dpmi_seginfo irq_handler_seginfo;
    _go32_dpmi_seginfo original_irq_handler_seginfo;

} DOS_InterruptHook;

void DOS_HookInterrupt(int irq, DOS_InterruptHookFn fn, DOS_InterruptHook *hook); // `irq` is the IRQ number, not the interrupt vector number!
void DOS_UnhookInterrupt(DOS_InterruptHook *hook, bool disable_interrupt);

// Ends C function definitions when using C++
#ifdef __cplusplus
}
#endif

#endif // SDL_dos_h_
