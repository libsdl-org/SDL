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

#ifdef SDL_AUDIO_DRIVER_DOS_SOUNDBLASTER

#include "../../core/dos/SDL_dos.h"
#include "../../core/dos/SDL_dos_scheduler.h"
#include "SDL_dosaudio_sb.h"

// Set to 1 to force 8-bit mono (pre-SB16) code path even on SB16 hardware.
// Useful for testing in DOSBox which always emulates an SB16 (DSP 4.x).
#define FORCE_SB_8BIT 0

static int soundblaster_base_port = -1;
static int soundblaster_irq = -1;
static int soundblaster_dma_channel = -1;
static int soundblaster_highdma_channel = -1;
static int soundblaster_version = -1;
static int soundblaster_version_minor = -1;
static bool soundblaster_is_sb16 = false; // false when FORCE_SB_8BIT or DSP < 4
static Uint8 soundblaster_silence_value = 0;

static void ResetSoundBlasterDSP(void)
{
    // reset the DSP.
    const int reset_port = soundblaster_base_port + 0x6;
    outportb(reset_port, 1);
    SDL_DelayPrecise(3000); // wait at least 3 microseconds for hardware to see it.
    outportb(reset_port, 0);
}

static bool ReadSoundBlasterReady(void)
{
    const int ready_port = soundblaster_base_port + 0xE;
    return ((inportb(ready_port) & (1 << 7)) != 0);
}

static void WriteSoundBlasterDSP(const Uint8 val)
{
    const int port = soundblaster_base_port + 0xC;
    int timeout = 100000;
    while ((inportb(port) & (1 << 7)) && --timeout > 0) { /* spin until ready or timeout */
    }
    outportb(port, val);
}

static Uint8 ReadSoundBlasterDSP(void)
{
    const int query_port = soundblaster_base_port + 0xA;
    int timeout = 100000;
    while (!ReadSoundBlasterReady() && --timeout > 0) { /* spin until ready or timeout */
    }
    return (Uint8)inportb(query_port);
}

// The ISR copies audio from a pre-allocated ring buffer directly into the
// DMA half-buffer. The SDL audio thread fills the ring buffer cooperatively
// (using the full SDL pipeline with all its allocations and mutexes), and
// the ISR just does a memcpy (no SDL calls, no DPMI, no allocator).

// Number of DMA half-buffers that fit in the ring. Must be a power of two.
// 4 chunks is ~45 ms at 44100 Hz, enough headroom for 22 fps frame times.
#define RING_BUFFER_CHUNKS 4

// All of the following statics are memory-locked, making them safe to access
// from the ISR without risking a page fault or DPMI re-entrance.

// ISR-cached copies of device state (avoids chasing heap pointers in IRQ context).
static volatile int isr_irq_ack_port = 0;

// ISR-visible ring buffer state (all memory-locked).
static volatile int isr_ring_read = 0;
static volatile int isr_ring_write = 0;
static int isr_ring_size = 0;         // ring_size (power-of-2 bytes)
static int isr_ring_mask = 0;         // ring_size - 1
static int isr_chunk_size = 0;        // one DMA half-buffer, in bytes
static Uint8 *isr_ring_buffer = NULL; // the ring itself (allocated and locked)
static Uint8 *isr_dma_buffer = NULL;  // pointer to the DMA double-buffer
static int isr_dma_halfdma = 0;       // half the DMA buffer size, in bytes
static int isr_dma_channel = 0;
static bool isr_is_16bit = false;
static Uint8 isr_silence_value = 0;

// Copy `len` bytes from the ring buffer at position `pos` into `dst`,
// handling the power-of-2 wrap. All pointers are memory-locked.
static void RingCopyOut(Uint8 *dst, int pos, int len)
{
    const int mask = isr_ring_mask;
    const int start = pos & mask;
    const int first = (start + len <= isr_ring_size) ? len : (isr_ring_size - start);
    SDL_memcpy(dst, isr_ring_buffer + start, first);
    if (first < len) {
        SDL_memcpy(dst + first, isr_ring_buffer, len - first);
    }
}
static void RingCopyOut_End(void) {}

// Determine which DMA half-buffer the hardware is NOT currently playing
// (i.e. the one we should fill). Uses ISR-cached statics so we don't
// chase any heap pointers.
static Uint8 *ISR_GetDMAHalf(void)
{
    int count;
    if (isr_is_16bit) {
        outportb(0xD8, 0x00);
        count = (int)inportb(0xC0 + (isr_dma_channel - 4) * 4 + 2);
        count += (int)inportb(0xC0 + (isr_dma_channel - 4) * 4 + 2) << 8;
        return isr_dma_buffer + (count < (isr_dma_halfdma / 2) ? 0 : isr_dma_halfdma);
    } else {
        outportb(0x0C, 0x00);
        count = (int)inportb(isr_dma_channel * 2 + 1);
        count += (int)inportb(isr_dma_channel * 2 + 1) << 8;
        return isr_dma_buffer + (count < isr_dma_halfdma ? 0 : isr_dma_halfdma);
    }
}
static void ISR_GetDMAHalf_End(void) {}

// The IRQ handler. Copies one chunk from the ring buffer into the DMA
// half-buffer that the hardware isn't currently playing. If the ring is
// empty it fills with silence (no stutter, just a brief gap).
//
// This function touches ONLY memory-locked data and does ONLY port I/O and
// memcpy. No DPMI, no malloc, no mutex, no FPU.
static void SoundBlasterIRQHandler(void)
{
    // Acknowledge hardware first.
    inportb(isr_irq_ack_port);
    DOS_EndOfInterrupt(soundblaster_irq);

    Uint8 *dma_dst = ISR_GetDMAHalf();

    // How many bytes are available in the ring?
    const int avail = isr_ring_write - isr_ring_read; // both are monotonic

    if (avail >= isr_chunk_size) {
        RingCopyOut(dma_dst, isr_ring_read, isr_chunk_size);
        isr_ring_read += isr_chunk_size;
    } else {
        // Ring underrun: fill with silence so we don't replay stale audio.
        SDL_memset(dma_dst, isr_silence_value, isr_chunk_size);
    }
}
static void SoundBlasterIRQHandler_End(void) {}

// Wait until the ring buffer has room for one more chunk.
// The audio thread keeps yielding so the game's main thread can run while
// we wait. Because the ISR is steadily draining the ring, this only blocks
// when the ring is completely full, which is a good problem to have.
static bool DOSSOUNDBLASTER_WaitDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    const int size = hidden->ring_size;

    for (;;) {
        // Available space = ring_size - (write - read).
        // ring_write is ours (audio thread only), ring_read is advanced by
        // the ISR. Read the ISR's copy so we see the latest drain position.
        const int used = hidden->ring_write - isr_ring_read;
        if ((size - used) >= hidden->chunk_size) {
            return true; // room for at least one chunk
        }
        DOS_Yield();
    }
}

static bool DOSSOUNDBLASTER_OpenDevice(SDL_AudioDevice *device)
{
    const bool is_sb16 = soundblaster_is_sb16;

    if (is_sb16) {
        // SB16 (DSP >= 4): 16-bit stereo signed
        device->spec.format = SDL_AUDIO_S16LE;
        device->spec.channels = 2;
    } else if (soundblaster_version >= 3) {
        // SB Pro (DSP 3.x): 8-bit stereo unsigned.
        // Max 22050 Hz in stereo (hardware interleaves L/R at double the rate).
        device->spec.format = SDL_AUDIO_U8;
        device->spec.channels = 2;
    } else {
        // SB 2.0 (DSP 2.x) and SB 1.x: 8-bit mono unsigned.
        device->spec.format = SDL_AUDIO_U8;
        device->spec.channels = 1;
    }

    // Accept whatever frequency SDL3's audio layer passes in. For SB16 (DSP >= 4)
    // the hardware supports 5000–44100 Hz via DSP command 0x41. For pre-SB16,
    // clamp to hardware limits:
    //   SB 1.x: max ~23 kHz mono
    //   SB 2.0 (DSP 2.x): max 44100 Hz mono (high-speed), ~23 kHz normal
    //   SB Pro (DSP 3.x): max 22050 Hz stereo, max 44100 Hz mono
    if (!is_sb16 && device->spec.freq > 22050) {
        device->spec.freq = 22050; // clamp to safe max for pre-SB16
    }
    device->sample_frames = SDL_GetDefaultSampleFramesFromFreq(device->spec.freq);

    // Calculate the final parameters for this audio specification
    SDL_UpdatedAudioDeviceFormat(device);

    SDL_Log("SOUNDBLASTER: Opening at %d Hz, %d channels, format 0x%X, %d sample frames (DSP %d.%d, %s)",
            device->spec.freq, device->spec.channels, device->spec.format, device->sample_frames,
            soundblaster_version, soundblaster_version_minor, is_sb16 ? "SB16" : "pre-SB16");

    if (device->buffer_size > (32 * 1024)) {
        return SDL_SetError("Buffer size is too large (choose smaller audio format and/or fewer sample frames)"); // DMA buffer has to fit in 64K segment, so buffer_size has to be half that, as we double it.
    }

    // Initialize all variables that we clean on shutdown
    struct SDL_PrivateAudioData *hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!hidden) {
        return false;
    }

    device->hidden = hidden;
    hidden->is_16bit = is_sb16;

    ResetSoundBlasterDSP();

    // allocate conventional memory for the DMA buffer.
    hidden->dma_channel = is_sb16 ? soundblaster_highdma_channel : soundblaster_dma_channel;
    if (hidden->dma_channel < 0) {
        SDL_free(hidden);
        return SDL_SetError("No %s DMA channel configured in BLASTER environment variable",
                            is_sb16 ? "high (16-bit)" : "low (8-bit)");
    }
    hidden->dma_buflen = device->buffer_size * 2;
    hidden->dma_buffer = (Uint8 *)DOS_AllocateDMAMemory(hidden->dma_buflen, &hidden->dma_seginfo);
    if (!hidden->dma_buffer) {
        return SDL_SetError("Couldn't allocate Sound Blaster DMA buffer!");
    }

    SDL_Log("SOUNDBLASTER: Allocated %d bytes of conventional memory at segment %d (ptr=%p)", (int)hidden->dma_buflen, (int)hidden->dma_seginfo.rm_segment, hidden->dma_buffer);

    // silence the DMA buffer to start
    SDL_memset(hidden->dma_buffer, soundblaster_silence_value, hidden->dma_buflen);

    // set up DMA controller.
    const Uint32 physical = DOS_LinearToPhysical(hidden->dma_buffer);
    const Uint8 physical_page = (physical >> 16) & 0xFF;

    if (is_sb16) {
        // High DMA (16-bit, channels 5-7): ports in 0xC0-0xDF range, counts in words.
        const int dma_words = (hidden->dma_buflen / 2) - 1;
        outportb(0xD4, 0x04 | hidden->dma_channel);                                           // mask the DMA channel
        outportb(0xD6, 0x58 | (hidden->dma_channel - 4));                                     // mode: single, read, auto-init
        static const int high_page_ports[] = { 0, 0, 0, 0, 0, 0x8B, 0x89, 0x8A };             // DMA page register ports for channels 5-7
        outportb(high_page_ports[hidden->dma_channel], physical_page);                        // page to transfer
        outportb(0xD8, 0x00);                                                                 // clear the flip-flop
        outportb(0xC0 + (hidden->dma_channel - 4) * 4, (Uint8)((physical >> 1) & 0xFF));      // offset low (word address)
        outportb(0xC0 + (hidden->dma_channel - 4) * 4, (Uint8)((physical >> 9) & 0xFF));      // offset high
        outportb(0xD8, 0x00);                                                                 // clear the flip-flop
        outportb(0xC0 + (hidden->dma_channel - 4) * 4 + 2, (Uint8)(dma_words & 0xFF));        // count low
        outportb(0xC0 + (hidden->dma_channel - 4) * 4 + 2, (Uint8)((dma_words >> 8) & 0xFF)); // count high
        outportb(0xD4, hidden->dma_channel & ~4);                                             // unmask the DMA channel
    } else {
        // Low DMA (8-bit, channels 0-3): ports in 0x00-0x0F range, counts in bytes.
        static const int page_ports[] = { 0x87, 0x83, 0x81, 0x82 }; // DMA page register ports for channels 0-3 (yes, they're out of order — that's how the IBM PC DMA controller works)
        const int dma_bytes = hidden->dma_buflen - 1;
        outportb(0x0A, 0x04 | hidden->dma_channel);                              // mask the DMA channel
        outportb(0x0B, 0x58 | hidden->dma_channel);                              // mode: single, read, auto-init
        outportb(page_ports[hidden->dma_channel], physical_page);                // page to transfer
        outportb(0x0C, 0x00);                                                    // clear the flip-flop
        outportb(hidden->dma_channel * 2, (Uint8)(physical & 0xFF));             // offset low (byte address)
        outportb(hidden->dma_channel * 2, (Uint8)((physical >> 8) & 0xFF));      // offset high
        outportb(0x0C, 0x00);                                                    // clear the flip-flop
        outportb(hidden->dma_channel * 2 + 1, (Uint8)(dma_bytes & 0xFF));        // count low
        outportb(hidden->dma_channel * 2 + 1, (Uint8)((dma_bytes >> 8) & 0xFF)); // count high
        outportb(0x0A, hidden->dma_channel);                                     // unmask the DMA channel (just the channel number, no bit 2)
    }

    // Cache the IRQ ack port so the ISR doesn't chase pointers.
    isr_irq_ack_port = is_sb16 ? (soundblaster_base_port + 0x0F) : (soundblaster_base_port + 0x0E);

    // Set up the IRQ-driven ring buffer.
    hidden->chunk_size = device->buffer_size; // one DMA half-buffer

    // Ring size must be a power of two and hold RING_BUFFER_CHUNKS chunks.
    hidden->ring_size = hidden->chunk_size * RING_BUFFER_CHUNKS;
    // Ensure power-of-two (chunk_size itself comes from SDL and may not be).
    {
        int rs = hidden->ring_size;
        rs--;
        rs |= rs >> 1;
        rs |= rs >> 2;
        rs |= rs >> 4;
        rs |= rs >> 8;
        rs |= rs >> 16;
        rs++;
        hidden->ring_size = rs;
    }

    hidden->ring_buffer = (Uint8 *)SDL_calloc(1, hidden->ring_size);
    if (!hidden->ring_buffer) {
        return SDL_SetError("Couldn't allocate ring buffer for IRQ-driven audio");
    }
    hidden->staging_buffer = (Uint8 *)SDL_calloc(1, hidden->chunk_size);
    if (!hidden->staging_buffer) {
        return SDL_SetError("Couldn't allocate staging buffer for IRQ-driven audio");
    }

    hidden->ring_read = 0;
    hidden->ring_write = 0;

    // Populate ISR-visible statics (all will be memory-locked below).
    isr_ring_buffer = hidden->ring_buffer;
    isr_ring_read = 0;
    isr_ring_write = 0;
    isr_ring_size = hidden->ring_size;
    isr_ring_mask = hidden->ring_size - 1;
    isr_chunk_size = hidden->chunk_size;
    isr_dma_buffer = hidden->dma_buffer;
    isr_dma_halfdma = hidden->dma_buflen / 2;
    isr_dma_channel = hidden->dma_channel;
    isr_is_16bit = is_sb16;
    isr_silence_value = soundblaster_silence_value;

    // Lock all ISR code and data to prevent page faults during interrupts.
    DOS_LockCode(SoundBlasterIRQHandler, SoundBlasterIRQHandler_End);
    DOS_LockCode(RingCopyOut, RingCopyOut_End);
    DOS_LockCode(ISR_GetDMAHalf, ISR_GetDMAHalf_End);
    DOS_LockData(*hidden->ring_buffer, hidden->ring_size);
    DOS_LockVariable(isr_ring_read);
    DOS_LockVariable(isr_ring_write);
    DOS_LockVariable(isr_ring_size);
    DOS_LockVariable(isr_ring_mask);
    DOS_LockVariable(isr_chunk_size);
    DOS_LockVariable(isr_ring_buffer);
    DOS_LockVariable(isr_dma_buffer);
    DOS_LockVariable(isr_dma_halfdma);
    DOS_LockVariable(isr_dma_channel);
    DOS_LockVariable(isr_is_16bit);
    DOS_LockVariable(isr_silence_value);
    DOS_LockVariable(isr_irq_ack_port);
    DOS_LockVariable(soundblaster_irq);

    DOS_HookInterrupt(soundblaster_irq, SoundBlasterIRQHandler, &hidden->interrupt_hook);

    WriteSoundBlasterDSP(0xD1); // turn on the speaker
    // The speaker-on command takes up to 112 ms to complete on real hardware.
    // Poll the DSP write status port (bit 7 clears when the DSP is ready);
    // in practice — and always in DOSBox — it completes almost instantly.
    {
        const int status_port = soundblaster_base_port + 0xC;
        const Uint64 deadline = SDL_GetTicksNS() + SDL_MS_TO_NS(112);
        while ((inportb(status_port) & 0x80) && (SDL_GetTicksNS() < deadline)) {
            SDL_DelayPrecise(SDL_US_TO_NS(100)); // brief yield between polls
        }
    }

    if (is_sb16) {
        // SB16 (DSP >= 4): set output sample rate directly
        WriteSoundBlasterDSP(0x41); // set output sampling rate
        WriteSoundBlasterDSP((Uint8)(device->spec.freq >> 8));
        WriteSoundBlasterDSP((Uint8)(device->spec.freq & 0xFF));

        // start 16-bit auto-initialize DMA mode
        // half the total buffer per transfer, then convert to samples (divide by 2 because they are 16-bits each).
        const int block_size = ((hidden->dma_buflen / 2) / sizeof(Sint16)) - 1; // one less than samples to be transferred.
        WriteSoundBlasterDSP(0xB6);                                             // 16-bit output, auto-init, FIFO on
        WriteSoundBlasterDSP(0x30);                                             // 16-bit stereo signed PCM
        WriteSoundBlasterDSP((Uint8)(block_size & 0xFF));
        WriteSoundBlasterDSP((Uint8)(block_size >> 8));
    } else {
        // Pre-SB16 (DSP < 4): set sample rate via Time Constant
        // Time Constant = 256 - (1000000 / (channels * freq))
        // In stereo mode the SB Pro interleaves L/R samples, so the effective
        // hardware rate is channels * freq.
        const int effective_rate = device->spec.channels * device->spec.freq;
        const Uint8 time_constant = (Uint8)(256 - (1000000 / effective_rate));
        WriteSoundBlasterDSP(0x40); // set time constant
        WriteSoundBlasterDSP(time_constant);

        // SB Pro (DSP 3.x): enable or disable stereo via mixer register 0x0E
        if (soundblaster_version >= 3) {
            const int mixer_addr = soundblaster_base_port + 0x04;
            const int mixer_data = soundblaster_base_port + 0x05;
            outportb(mixer_addr, 0x0E); // select output/stereo register
            if (device->spec.channels == 2) {
                outportb(mixer_data, inportb(mixer_data) | 0x02); // set bit 1 = stereo
            } else {
                outportb(mixer_data, inportb(mixer_data) & ~0x02); // clear bit 1 = mono
            }
        }

        // start 8-bit auto-initialize DMA mode
        // block_size is in bytes for 8-bit, and it's the half-buffer size minus 1
        const int block_size = (hidden->dma_buflen / 2) - 1;
        WriteSoundBlasterDSP(0x48); // set DSP block transfer size
        WriteSoundBlasterDSP((Uint8)(block_size & 0xFF));
        WriteSoundBlasterDSP((Uint8)(block_size >> 8));
        // NOTE: DSP 1.x does not support auto-init (0x1C). Those cards are extremely
        // rare and would need single-cycle transfers re-triggered from the ISR.
        // For now we use 0x1C anyway and hope for the best on DSP 1.x hardware.
        WriteSoundBlasterDSP(0x1C); // 8-bit auto-init DMA playback
    }

    SDL_Log("SoundBlaster opened!");
    return true;
}

// Return the staging buffer. The SDL audio pipeline writes the mixed audio
// here; PlayDevice then copies it into the ring buffer.
static Uint8 *DOSSOUNDBLASTER_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    (void)buffer_size; // unchanged, always one chunk
    return hidden->staging_buffer;
}

// Commit the staging buffer into the ring buffer.
// Called by SDL's audio thread after it has written a full chunk.
static bool DOSSOUNDBLASTER_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buffer_size)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    const int mask = hidden->ring_size - 1;
    const int pos = hidden->ring_write & mask;
    const int first = (pos + buffer_size <= hidden->ring_size) ? buffer_size : (hidden->ring_size - pos);

    SDL_memcpy(hidden->ring_buffer + pos, buffer, first);
    if (first < buffer_size) {
        SDL_memcpy(hidden->ring_buffer, buffer + first, buffer_size - first);
    }

    // Advance the write cursor. Interrupts are disabled around the store so
    // the ISR never sees a torn write (not strictly necessary on x86 for an
    // aligned int, but let's be safe).
    DOS_DisableInterrupts();
    hidden->ring_write += buffer_size;
    isr_ring_write = hidden->ring_write;
    DOS_EnableInterrupts();

    return true;
}

static void DOSSOUNDBLASTER_CloseDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    if (hidden) {
        // Disable PCM.
        if (hidden->is_16bit) {
            WriteSoundBlasterDSP(0xDA); // exit 16-bit auto-init DMA
            WriteSoundBlasterDSP(0xD3); // turn off the speaker
        } else {
            WriteSoundBlasterDSP(0xD0); // halt 8-bit DMA
            WriteSoundBlasterDSP(0xDA); // exit auto-init DMA
            WriteSoundBlasterDSP(0xD3); // turn off the speaker

            // SB Pro: reset stereo bit in mixer register 0x0E
            if (soundblaster_version >= 3) {
                const int mixer_addr = soundblaster_base_port + 0x04;
                const int mixer_data = soundblaster_base_port + 0x05;
                outportb(mixer_addr, 0x0E);
                outportb(mixer_data, inportb(mixer_data) & ~0x02); // clear stereo bit
            }
        }

        DOS_UnhookInterrupt(&hidden->interrupt_hook, true);

        // disable DMA — mask the appropriate DMA channel.
        if (hidden->dma_buffer) {
            if (hidden->is_16bit) {
                outportb(0xD4, 0x04 | hidden->dma_channel); // mask high DMA channel (channels 5-7)
            } else {
                outportb(0x0A, 0x04 | hidden->dma_channel); // mask low DMA channel (channels 0-3)
            }
            DOS_FreeConventionalMemory(&hidden->dma_seginfo);
        }

        // Free ring buffer resources.
        if (hidden->ring_buffer) {
            SDL_free(hidden->ring_buffer);
        }
        if (hidden->staging_buffer) {
            SDL_free(hidden->staging_buffer);
        }

        // Clear ISR-visible statics.
        isr_ring_buffer = NULL;
        isr_ring_read = 0;
        isr_ring_write = 0;
        isr_ring_size = 0;
        isr_ring_mask = 0;
        isr_chunk_size = 0;
        isr_dma_buffer = NULL;
        isr_dma_halfdma = 0;
        isr_irq_ack_port = 0;

        SDL_free(hidden);
    }
}

static bool CheckForSoundBlaster(void)
{
    ResetSoundBlasterDSP();

    // wait for the DSP to say it's ready.
    bool ready = false;
    for (int i = 0; i < 300; i++) { // may take up to 100msecs to initialize. We'll give it 300.
        SDL_DelayPrecise(1000);
        if (ReadSoundBlasterReady()) {
            ready = true;
            break;
        }
    }

    if (!ready) {
        return SDL_SetError("No SoundBlaster detected on port 0x%X", soundblaster_base_port); // either no SoundBlaster or it's on a different base port.
    } else if (ReadSoundBlasterDSP() != 0xAA) {
        return SDL_SetError("Not a SoundBlaster at port 0x%X", soundblaster_base_port); // either it's not a SoundBlaster or there's a problem.
    }
    return true;
}

static bool IsSoundBlasterPresent(void)
{
    const char *env = SDL_getenv("BLASTER");
    if (!env) {
        return SDL_SetError("No BLASTER environment variable to find Sound Blaster"); // definitely doesn't have a Sound Blaster (or they screwed up).
    }

    char *copy = SDL_strdup(env);
    if (!copy) {
        return false; // oh well.
    }

    char *str = copy;
    char *saveptr = NULL;

    char *token;
    while ((token = SDL_strtok_r(str, " ", &saveptr)) != NULL) {
        str = NULL; // must be NULL for future calls to tokenize the same string.
        char *endp = NULL;
        const int base = (SDL_toupper(*token) == 'A') ? 16 : 10;
        const int num = (int)SDL_strtol(token + 1, &endp, base);
        if ((token[1] == 0) || (*endp != 0)) { // bogus num
            continue;
        } else if (num < 0) {
            continue;
        }

        switch (SDL_toupper(*token)) {
        case 'A': // Base i/o port (in hex)
            soundblaster_base_port = num;
            break;

        case 'I': // IRQ
            soundblaster_irq = num;
            break;

        case 'D': // DMA channel
            soundblaster_dma_channel = num;
            break;

        case 'H': // High DMA channel
            soundblaster_highdma_channel = num;
            break;

        // don't care about these.
        // case 'M':  // mixer chip base port
        // case 'P':  // MPU-401 base port
        // case 'T':  // type of device
        // case 'E':  // EMU8000 base port: an AWE32 thing
        default:
            break;
        }
    }
    SDL_free(copy);

    if (soundblaster_base_port < 0 || soundblaster_irq < 0 || (soundblaster_dma_channel < 0 && soundblaster_highdma_channel < 0)) {
        return SDL_SetError("BLASTER environment variable is incomplete or incorrect");
    } else if (!CheckForSoundBlaster()) {
        return false;
    }

    WriteSoundBlasterDSP(0xE1); // query DSP version
    soundblaster_version = (int)ReadSoundBlasterDSP();
    soundblaster_version_minor = (int)ReadSoundBlasterDSP();

    SDL_Log("SB: BLASTER env='%s'", env);
    SDL_Log("SB: port=0x%X", soundblaster_base_port);
    SDL_Log("SB: irq=%d", soundblaster_irq);
    SDL_Log("SB: dma8=%d", soundblaster_dma_channel);
    SDL_Log("SB: dma16=%d", soundblaster_highdma_channel);
    SDL_Log("SB: version=%d.%d", soundblaster_version, soundblaster_version_minor);

    soundblaster_is_sb16 = !FORCE_SB_8BIT && (soundblaster_version >= 4);
    soundblaster_silence_value = soundblaster_is_sb16 ? 0x00 : 0x80; // S16LE silence is 0x00, U8 silence is 0x80

    return true;
}

static bool DOSSOUNDBLASTER_Init(SDL_AudioDriverImpl *impl)
{
    if (!IsSoundBlasterPresent()) {
        return false;
    }

    impl->OpenDevice = DOSSOUNDBLASTER_OpenDevice;
    impl->WaitDevice = DOSSOUNDBLASTER_WaitDevice;
    impl->GetDeviceBuf = DOSSOUNDBLASTER_GetDeviceBuf;
    impl->PlayDevice = DOSSOUNDBLASTER_PlayDevice;
    impl->CloseDevice = DOSSOUNDBLASTER_CloseDevice;

    impl->OnlyHasDefaultPlaybackDevice = true;

    return true;
}

AudioBootStrap DOSSOUNDBLASTER_bootstrap = {
    "soundblaster", "Sound Blaster", DOSSOUNDBLASTER_Init, false, false
};

#endif // SDL_AUDIO_DRIVER_DOS_SOUNDBLASTER
