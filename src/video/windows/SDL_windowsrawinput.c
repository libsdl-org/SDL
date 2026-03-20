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

#if defined(SDL_VIDEO_DRIVER_WINDOWS)

#include "SDL_windowsvideo.h"

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)

#include "SDL_windowsevents.h"

#include "../../joystick/usb_ids.h"
#include "../../events/SDL_events_c.h"
#include "../../thread/SDL_systhread.h"

#define ENABLE_RAW_MOUSE_INPUT      0x01
#define ENABLE_RAW_KEYBOARD_INPUT   0x02
#define RAW_KEYBOARD_FLAG_NOHOTKEYS 0x04
#define RAW_KEYBOARD_FLAG_INPUTSINK 0x08

typedef struct
{
    bool done;
    SDL_AtomicU32 flags; // Thread sets this to the actually-set flags if updating state failed
    HANDLE ready_event;
    HANDLE signal_event;
    HANDLE thread;
} RawInputThreadData;

static RawInputThreadData thread_data = { 0 };

typedef enum
{
    RINP_QUIT,
    RINP_UPDATE,
    RINP_CONTINUE
} RawInputIterateResult;

static RawInputIterateResult IterateRawInputThread(void)
{
    // The high-order word of GetQueueStatus() will let us know if there's immediate raw input to be processed.
    // A (necessary!) side effect is that it also marks the message queue bits as stale,
    // so MsgWaitForMultipleObjects will block.

    // Any pending flag update won't be processed until the queue drains, but this is
    // at most one poll cycle since GetQueueStatus clears the wake bits.
    if (HIWORD(GetQueueStatus(QS_RAWINPUT)) & QS_RAWINPUT) {
        return thread_data.done ? RINP_QUIT : RINP_CONTINUE;
    }

    static const DWORD WAIT_SIGNAL = WAIT_OBJECT_0;
    static const DWORD WAIT_INPUT = WAIT_OBJECT_0 + 1;

    // There wasn't anything, so we'll wait until new data (or signal_event) wakes us up.
    DWORD wait_status = MsgWaitForMultipleObjects(1, &thread_data.signal_event, FALSE, INFINITE, QS_RAWINPUT);
    if (wait_status == WAIT_SIGNAL) {
        // signal_event can only be set if we need to update or quit.
        return thread_data.done ? RINP_QUIT : RINP_UPDATE;
    } else if (wait_status == WAIT_INPUT) {
        return RINP_CONTINUE;
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Raw input thread exiting, unexpected wait result: %lu", wait_status);
        return RINP_QUIT;
    }
}

static bool UpdateRawInputDeviceFlags(HWND window, Uint32 last_flags, Uint32 new_flags)
{
    // We had nothing enabled, and we're trying to stop everything. Nothing to do.
    if (last_flags == new_flags) {
        return true;
    }

    RAWINPUTDEVICE devices[2] = { 0 };
    UINT count = 0;

    if ((new_flags & ENABLE_RAW_MOUSE_INPUT) != (last_flags & ENABLE_RAW_MOUSE_INPUT)) {
        devices[count].usUsagePage = USB_USAGEPAGE_GENERIC_DESKTOP;
        devices[count].usUsage = USB_USAGE_GENERIC_MOUSE;

        if (new_flags & ENABLE_RAW_MOUSE_INPUT) {
            devices[count].dwFlags = 0;
            devices[count].hwndTarget = window;
        } else {
            devices[count].dwFlags = RIDEV_REMOVE;
            devices[count].hwndTarget = NULL;
        }

        ++count;
    }

    const Uint32 old_kb_flags = last_flags & (ENABLE_RAW_KEYBOARD_INPUT | RAW_KEYBOARD_FLAG_NOHOTKEYS | RAW_KEYBOARD_FLAG_INPUTSINK);
    const Uint32 new_kb_flags = new_flags & (ENABLE_RAW_KEYBOARD_INPUT | RAW_KEYBOARD_FLAG_NOHOTKEYS | RAW_KEYBOARD_FLAG_INPUTSINK);

    if (old_kb_flags != new_kb_flags) {
        devices[count].usUsagePage = USB_USAGEPAGE_GENERIC_DESKTOP;
        devices[count].usUsage = USB_USAGE_GENERIC_KEYBOARD;

        if (new_kb_flags & ENABLE_RAW_KEYBOARD_INPUT) {
            devices[count].dwFlags = 0;
            devices[count].hwndTarget = window;
            if (new_kb_flags & RAW_KEYBOARD_FLAG_NOHOTKEYS) {
                devices[count].dwFlags |= RIDEV_NOHOTKEYS;
            }

            if (new_kb_flags & RAW_KEYBOARD_FLAG_INPUTSINK) {
                devices[count].dwFlags |= RIDEV_INPUTSINK;
            }
        } else {
            devices[count].dwFlags = RIDEV_REMOVE;
            devices[count].hwndTarget = NULL;
        }

        ++count;
    }

    if (RegisterRawInputDevices(devices, count, sizeof(devices[0]))) {
        return true;
    }
    return false;
}

static DWORD WINAPI WIN_RawInputThread(LPVOID param)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    Uint32 last_flags = 0;
    HWND window;

    SDL_SYS_SetupThread("SDLRawInput");

    window = CreateWindowEx(0, TEXT("Message"), NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (!window) {
        return 0;
    }

    // This doesn't *really* need to be atomic, because the parent is waiting for us
    last_flags = SDL_GetAtomicU32(&thread_data.flags);
    if (!UpdateRawInputDeviceFlags(window, 0, last_flags)) {
        DestroyWindow(window);
        return 0;
    }

    // Make sure we get events as soon as possible
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // Tell the parent we're ready to go!
    SetEvent(thread_data.ready_event);

    RawInputIterateResult iter_result;

    Uint64 idle_begin = SDL_GetTicksNS();
    while ((iter_result = IterateRawInputThread()) != RINP_QUIT) {
        switch (iter_result) {
        case RINP_QUIT: // Unreachable
            break;
        case RINP_UPDATE:
        {
            const Uint32 new_flags = SDL_GetAtomicU32(&thread_data.flags);
            if (!UpdateRawInputDeviceFlags(window, last_flags, new_flags)) {
                // Revert the shared flags so the main thread can detect the failure
                SDL_SetAtomicU32(&thread_data.flags, last_flags);
            } else {
                last_flags = new_flags;
            }
        } break;
        case RINP_CONTINUE:
        {
            Uint64 idle_end = SDL_GetTicksNS();
            Uint64 idle_time = idle_end - idle_begin;
            Uint64 usb_8khz_interval = SDL_US_TO_NS(125);
            Uint64 poll_start = idle_time < usb_8khz_interval ? _this->internal->last_rawinput_poll : idle_end;

            WIN_PollRawInput(_this, poll_start);

            // Reset idle_begin for the next go-around
            idle_begin = SDL_GetTicksNS();
        } break;
        }
    }

    if (_this->internal->raw_input_fake_pen_id) {
        SDL_RemovePenDevice(0, SDL_GetKeyboardFocus(), _this->internal->raw_input_fake_pen_id);
        _this->internal->raw_input_fake_pen_id = 0;
    }

    // Reset this here, since if we're exiting due to failure, WIN_UpdateRawInputEnabled would see a stale value.
    SDL_SetAtomicU32(&thread_data.flags, 0);

    UpdateRawInputDeviceFlags(NULL, last_flags, 0);

    DestroyWindow(window);

    return 0;
}

static void CleanupRawInputThreadData(void)
{
    if (thread_data.thread) {
        thread_data.done = true;
        SetEvent(thread_data.signal_event);
        WaitForSingleObject(thread_data.thread, 3000);
        CloseHandle(thread_data.thread);
    }

    if (thread_data.ready_event) {
        CloseHandle(thread_data.ready_event);
    }

    if (thread_data.signal_event) {
        CloseHandle(thread_data.signal_event);
    }

    thread_data = (RawInputThreadData){ 0 };
}

// Computes the desired raw input flags from SDL_VideoData and ensures the
// raw input thread's device registrations match.
// Creates the thread on first use, only WIN_QuitRawInput actually shuts it down.
static bool WIN_UpdateRawInputEnabled(SDL_VideoDevice *_this)
{
    bool result = false;
    SDL_VideoData *data = _this->internal;
    Uint32 desired_flags = 0;

    if (data->raw_mouse_enabled) {
        desired_flags |= ENABLE_RAW_MOUSE_INPUT;
    }
    if (data->raw_keyboard_enabled) {
        desired_flags |= ENABLE_RAW_KEYBOARD_INPUT;
    }
    if (data->raw_keyboard_flag_nohotkeys) {
        desired_flags |= RAW_KEYBOARD_FLAG_NOHOTKEYS;
    }
    if (data->raw_keyboard_flag_inputsink) {
        desired_flags |= RAW_KEYBOARD_FLAG_INPUTSINK;
    }

    if (desired_flags == SDL_GetAtomicU32(&thread_data.flags)) {
        result = true;
        goto done;
    }

    // If the thread exited unexpectedly (e.g. MsgWaitForMultipleObjects failed),
    // the handle is stale. Clean it up so the creation path below can recover.
    if (thread_data.thread && WaitForSingleObject(thread_data.thread, 0) != WAIT_TIMEOUT) {
        CleanupRawInputThreadData();
    }

    // The thread will read from this to update its flags
    SDL_SetAtomicU32(&thread_data.flags, desired_flags);

    if (thread_data.thread) {
        // Thread is already running. Fire (the event) and forget, it'll read the atomic flags on wakeup.

        // If RegisterRawInputDevices fails, the thread reverts the atomic and the next call
        // to this function will see the mismatch and retry.
        SDL_assert(thread_data.signal_event);
        SetEvent(thread_data.signal_event);
        result = true;
    } else if (desired_flags) {
        HANDLE wait_handles[2];

        // Thread isn't running, spin it up
        thread_data.ready_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!thread_data.ready_event) {
            WIN_SetError("CreateEvent");
            goto done;
        }

        thread_data.done = false;
        thread_data.signal_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!thread_data.signal_event) {
            WIN_SetError("CreateEvent");
            goto done;
        }

        thread_data.thread = CreateThread(NULL, 0, WIN_RawInputThread, NULL, 0, NULL);
        if (!thread_data.thread) {
            WIN_SetError("CreateThread");
            goto done;
        }

        // Wait for the thread to complete initial setup or exit
        wait_handles[0] = thread_data.ready_event;
        wait_handles[1] = thread_data.thread;
        if (WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE) != WAIT_OBJECT_0) {
            SDL_SetError("Couldn't set up raw input handling");
            goto done;
        }
        result = true;
    } else {
        // Thread isn't running and we tried to disable raw input, nothing to do
        result = true;
    }
done:
    if (!result) {
        CleanupRawInputThreadData();
    }
    return result;
}

bool WIN_SetRawMouseEnabled(SDL_VideoDevice *_this, bool enabled)
{
    SDL_VideoData *data = _this->internal;
    data->raw_mouse_enabled = enabled;
    if (data->gameinput_context) {
        if (!WIN_UpdateGameInputEnabled(_this)) {
            data->raw_mouse_enabled = !enabled;
            return false;
        }
    } else {
        if (!WIN_UpdateRawInputEnabled(_this)) {
            data->raw_mouse_enabled = !enabled;
            return false;
        }
    }
    return true;
}

bool WIN_SetRawKeyboardEnabled(SDL_VideoDevice *_this, bool enabled)
{
    SDL_VideoData *data = _this->internal;
    data->raw_keyboard_enabled = enabled;
    if (data->gameinput_context) {
        if (!WIN_UpdateGameInputEnabled(_this)) {
            data->raw_keyboard_enabled = !enabled;
            return false;
        }
    } else {
        if (!WIN_UpdateRawInputEnabled(_this)) {
            data->raw_keyboard_enabled = !enabled;
            return false;
        }
    }
    return true;
}

typedef enum WIN_RawKeyboardFlag {
    NOHOTKEYS,
    INPUTSINK,
} WIN_RawKeyboardFlag;

static bool WIN_SetRawKeyboardFlag(SDL_VideoDevice *_this, WIN_RawKeyboardFlag flag, bool enabled)
{
    SDL_VideoData *data = _this->internal;

    switch(flag) {
        case NOHOTKEYS:
            data->raw_keyboard_flag_nohotkeys = enabled;
            break;
        case INPUTSINK:
            data->raw_keyboard_flag_inputsink = enabled;
            break;
        default:
            return false;
    }

    if (data->gameinput_context) {
        return true;
    }

    return WIN_UpdateRawInputEnabled(_this);
}

bool WIN_SetRawKeyboardFlag_NoHotkeys(SDL_VideoDevice *_this, bool enabled)
{
    return WIN_SetRawKeyboardFlag(_this, NOHOTKEYS, enabled);
}

bool WIN_SetRawKeyboardFlag_Inputsink(SDL_VideoDevice *_this, bool enabled)
{
    return WIN_SetRawKeyboardFlag(_this, INPUTSINK, enabled);
}

void WIN_QuitRawInput(SDL_VideoDevice *_this)
{
    CleanupRawInputThreadData();
}

#else

bool WIN_SetRawMouseEnabled(SDL_VideoDevice *_this, bool enabled)
{
    return SDL_Unsupported();
}

bool WIN_SetRawKeyboardEnabled(SDL_VideoDevice *_this, bool enabled)
{
    return SDL_Unsupported();
}

bool WIN_SetRawKeyboardFlag_NoHotkeys(SDL_VideoDevice *_this, bool enabled)
{
    return SDL_Unsupported();
}

bool WIN_SetRawKeyboardFlag_Inputsink(SDL_VideoDevice *_this, bool enabled)
{
    return SDL_Unsupported();
}

void WIN_QuitRawInput(SDL_VideoDevice *_this)
{
}

#endif // !SDL_PLATFORM_XBOXONE && !SDL_PLATFORM_XBOXSERIES

#endif // SDL_VIDEO_DRIVER_WINDOWS
