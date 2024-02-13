# Migrating to SDL 3.0

This guide provides useful information for migrating applications from SDL 2.0 to SDL 3.0.

Details on API changes are organized by SDL 2.0 header below.

The file with your main() function should include <SDL3/SDL_main.h>, as that is no longer included in SDL.h.

Many functions and symbols have been renamed. We have provided a handy Python script [rename_symbols.py](https://github.com/libsdl-org/SDL/blob/main/build-scripts/rename_symbols.py) to rename SDL2 functions to their SDL3 counterparts:
```sh
rename_symbols.py --all-symbols source_code_path
```

It's also possible to apply a semantic patch to migrate more easily to SDL3: [SDL_migration.cocci](https://github.com/libsdl-org/SDL/blob/main/build-scripts/SDL_migration.cocci)

SDL headers should now be included as `#include <SDL3/SDL.h>`. Typically that's the only SDL header you'll need in your application unless you are using OpenGL or Vulkan functionality. SDL_image, SDL_mixer, SDL_net, SDL_ttf and SDL_rtf have also their preferred include path changed: for SDL_image, it becomes `#include <SDL3_image/SDL_image.h>`. We have provided a handy Python script [rename_headers.py](https://github.com/libsdl-org/SDL/blob/main/build-scripts/rename_headers.py) to rename SDL2 headers to their SDL3 counterparts:
```sh
rename_headers.py source_code_path
```

Some macros are renamed and/or removed in SDL3. We have provided a handy Python script [rename_macros.py](https://github.com/libsdl-org/SDL/blob/main/build-scripts/rename_macros.py) to replace these, and also add fixme comments on how to further improve the code:
```sh
rename_macros.py source_code_path
```


CMake users should use this snippet to include SDL support in their project:
```
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)
target_link_libraries(mygame PRIVATE SDL3::SDL3)
```

Autotools users should use this snippet to include SDL support in their project:
```
PKG_CHECK_MODULES([SDL3], [sdl3])
```
and then add $SDL3_CFLAGS to their project CFLAGS and $SDL3_LIBS to their project LDFLAGS

Makefile users can use this snippet to include SDL support in their project:
```
CFLAGS += $(shell pkg-config sdl3 --cflags)
LDFLAGS += $(shell pkg-config sdl3 --libs)
```

The SDL3test library has been renamed SDL3_test.

The SDLmain library has been removed, it's been entirely replaced by SDL_main.h.

The vi format comments have been removed from source code. Vim users can use the [editorconfig plugin](https://github.com/editorconfig/editorconfig-vim) to automatically set tab spacing for the SDL coding style.

## SDL_atomic.h

The following structures have been renamed:
- SDL_atomic_t => SDL_AtomicInt

The following functions have been renamed:
* SDL_AtomicCAS() => SDL_AtomicCompareAndSwap()
* SDL_AtomicCASPtr() => SDL_AtomicCompareAndSwapPointer()
* SDL_AtomicLock() => SDL_LockSpinlock()
* SDL_AtomicTryLock() => SDL_TryLockSpinlock()
* SDL_AtomicUnlock() => SDL_UnlockSpinlock()

## SDL_audio.h

The audio subsystem in SDL3 is dramatically different than SDL2. The primary way to play audio is no longer an audio callback; instead you bind SDL_AudioStreams to devices; however, there is still a callback method available if needed.

The SDL 1.2 audio compatibility API has also been removed, as it was a simplified version of the audio callback interface.

SDL3 will not implicitly initialize the audio subsystem on your behalf if you open a device without doing so. Please explicitly call SDL_Init(SDL_INIT_AUDIO) at some point.

SDL3's audio subsystem offers an enormous amount of power over SDL2, but if you just want a simple migration of your existing code, you can ignore most of it. The simplest migration path from SDL2 looks something like this:

In SDL2, you might have done something like this to play audio...

```c
    void SDLCALL MyAudioCallback(void *userdata, Uint8 * stream, int len)
    {
        /* Calculate a little more audio here, maybe using `userdata`, write it to `stream` */
    }

    /* ...somewhere near startup... */
    SDL_AudioSpec my_desired_audio_format;
    SDL_zero(my_desired_audio_format);
    my_desired_audio_format.format = AUDIO_S16;
    my_desired_audio_format.channels = 2;
    my_desired_audio_format.freq = 44100;
    my_desired_audio_format.samples = 1024;
    my_desired_audio_format.callback = MyAudioCallback;
    my_desired_audio_format.userdata = &my_audio_callback_user_data;
    SDL_AudioDeviceID my_audio_device = SDL_OpenAudioDevice(NULL, 0, &my_desired_audio_format, NULL, 0);
    SDL_PauseAudioDevice(my_audio_device, 0);
```

...in SDL3, you can do this...

```c
    void SDLCALL MyNewAudioCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
    {
        /* Calculate a little more audio here, maybe using `userdata`, write it to `stream`
         *
         * If you want to use the original callback, you could do something like this:
         */
        if (additional_amount > 0) {
            Uint8 *data = SDL_stack_alloc(Uint8, additional_amount);
            if (data) {
                MyAudioCallback(userdata, data, additional_amount);
                SDL_PutAudioStreamData(stream, data, additional_amount);
                SDL_stack_free(data);
            }
        }
    }

    /* ...somewhere near startup... */
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &spec, MyNewAudioCallback, &my_audio_callback_user_data);
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
```

If you used SDL_QueueAudio instead of a callback in SDL2, this is also straightforward.

```c
    /* ...somewhere near startup... */
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &spec, NULL, NULL);
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));

    /* ...in your main loop... */
    /* calculate a little more audio into `buf`, add it to `stream` */
    SDL_PutAudioStreamData(stream, buf, buflen);

```

...these same migration examples apply to audio capture, just using SDL_GetAudioStreamData instead of SDL_PutAudioStreamData.

SDL_AudioInit() and SDL_AudioQuit() have been removed. Instead you can call SDL_InitSubSystem() and SDL_QuitSubSystem() with SDL_INIT_AUDIO, which will properly refcount the subsystems. You can choose a specific audio driver using SDL_AUDIO_DRIVER hint.

The `SDL_AUDIO_ALLOW_*` symbols have been removed; now one may request the format they desire from the audio device, but ultimately SDL_AudioStream will manage the difference. One can use SDL_GetAudioDeviceFormat() to see what the final format is, if any "allowed" changes should be accomodated by the app.

SDL_AudioDeviceID now represents both an open audio device's handle (a "logical" device) and the instance ID that the hardware owns as long as it exists on the system (a "physical" device). The separation between device instances and device indexes is gone, and logical and physical devices are almost entirely interchangeable at the API level.

Devices are opened by physical device instance ID, and a new logical instance ID is generated by the open operation; This allows any device to be opened multiple times, possibly by unrelated pieces of code. SDL will manage the logical devices to provide a single stream of audio to the physical device behind the scenes.

Devices are not opened by an arbitrary string name anymore, but by device instance ID (or magic numbers to request a reasonable default, like a NULL string in SDL2). In SDL2, the string was used to open both a standard list of system devices, but also allowed for arbitrary devices, such as hostnames of network sound servers. In SDL3, many of the backends that supported arbitrary device names are obsolete and have been removed; of those that remain, arbitrary devices will be opened with a default device ID and an SDL_hint, so specific end-users can set an environment variable to fit their needs and apps don't have to concern themselves with it.

Many functions that would accept a device index and an `iscapture` parameter now just take an SDL_AudioDeviceID, as they are unique across all devices, instead of separate indices into output and capture device lists.

Rather than iterating over audio devices using a device index, there are new functions, SDL_GetAudioOutputDevices() and SDL_GetAudioCaptureDevices(), to get the current list of devices, and new functions to get information about devices from their instance ID:

```c
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        int i, num_devices;
        SDL_AudioDeviceID *devices = SDL_GetAudioOutputDevices(&num_devices);
        if (devices) {
            for (i = 0; i < num_devices; ++i) {
                SDL_AudioDeviceID instance_id = devices[i];
                char *name = SDL_GetAudioDeviceName(instance_id);
                SDL_Log("AudioDevice %" SDL_PRIu32 ": %s\n", instance_id, name);
                SDL_free(name);
            }
            SDL_free(devices);
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}
```

SDL_LockAudioDevice() and SDL_UnlockAudioDevice() have been removed, since there is no callback in another thread to protect. SDL's audio subsystem and SDL_AudioStream maintain their own locks internally, so audio streams are safe to use from any thread. If the app assigns a callback to a specific stream, it can use the stream's lock through SDL_LockAudioStream() if necessary.

SDL_PauseAudioDevice() no longer takes a second argument; it always pauses the device. To unpause, use SDL_ResumeAudioDevice().

Audio devices, opened by SDL_OpenAudioDevice(), no longer start in a paused state, as they don't begin processing audio until a stream is bound.

SDL_GetAudioDeviceStatus() has been removed; there is now SDL_AudioDevicePaused().

SDL_QueueAudio(), SDL_DequeueAudio, and SDL_ClearQueuedAudio and SDL_GetQueuedAudioSize() have been removed; an SDL_AudioStream bound to a device provides the exact same functionality.

APIs that use channel counts used to use a Uint8 for the channel; now they use int.

SDL_AudioSpec has been reduced; now it only holds format, channel, and sample rate. SDL_GetSilenceValueForFormat() can provide the information from the SDL_AudioSpec's `silence` field. The other SDL2 SDL_AudioSpec fields aren't relevant anymore.

SDL_GetAudioDeviceSpec() is removed; use SDL_GetAudioDeviceFormat() instead.

SDL_GetDefaultAudioInfo() is removed; SDL_GetAudioDeviceFormat() with SDL_AUDIO_DEVICE_DEFAULT_OUTPUT or SDL_AUDIO_DEVICE_DEFAULT_CAPTURE. There is no replacement for querying the default device name; the string is no longer used to open devices, and SDL3 will migrate between physical devices on the fly if the system default changes, so if you must show this to the user, a generic name like "System default" is recommended.

SDL_MixAudio() has been removed, as it relied on legacy SDL 1.2 quirks; SDL_MixAudioFormat() remains and offers the same functionality.

SDL_AudioInit() and SDL_AudioQuit() have been removed. Instead you can call SDL_InitSubSystem() and SDL_QuitSubSystem() with SDL_INIT_AUDIO, which will properly refcount the subsystems. You can choose a specific audio driver using SDL_AUDIO_DRIVER hint.

SDL_FreeWAV has been removed and calls can be replaced with SDL_free.

SDL_LoadWAV() is a proper function now and no longer a macro (but offers the same functionality otherwise).

SDL_LoadWAV_RW() and SDL_LoadWAV() return an int now: zero on success, -1 on error, like most of SDL. They no longer return a pointer to an SDL_AudioSpec.

SDL_AudioCVT interface has been removed, the SDL_AudioStream interface (for audio supplied in pieces) or the new SDL_ConvertAudioSamples() function (for converting a complete audio buffer in one call) can be used instead.

Code that used to look like this:
```c
    SDL_AudioCVT cvt;
    SDL_BuildAudioCVT(&cvt, src_format, src_channels, src_rate, dst_format, dst_channels, dst_rate);
    cvt.len = src_len;
    cvt.buf = (Uint8 *) SDL_malloc(src_len * cvt.len_mult);
    SDL_memcpy(cvt.buf, src_data, src_len);
    SDL_ConvertAudio(&cvt);
    do_something(cvt.buf, cvt.len_cvt);
```
should be changed to:
```c
    Uint8 *dst_data = NULL;
    int dst_len = 0;
    const SDL_AudioSpec src_spec = { src_format, src_channels, src_rate };
    const SDL_AudioSpec dst_spec = { dst_format, dst_channels, dst_rate };
    if (SDL_ConvertAudioSamples(&src_spec, src_data, src_len, &dst_spec, &dst_data, &dst_len) < 0) {
        /* error */
    }
    do_something(dst_data, dst_len);
    SDL_free(dst_data);
```

AUDIO_U16, AUDIO_U16LSB, AUDIO_U16MSB, and AUDIO_U16SYS have been removed. They were not heavily used, and one could not memset a buffer in this format to silence with a single byte value. Use a different audio format.

If you need to convert U16 audio data to a still-supported format at runtime, the fastest, lossless conversion is to SDL_AUDIO_S16:

```c
    /* this converts the buffer in-place. The buffer size does not change. */
    Sint16 *audio_ui16_to_si16(Uint16 *buffer, const size_t num_samples)
    {
        size_t i;
        const Uint16 *src = buffer;
        Sint16 *dst = (Sint16 *) buffer;

        for (i = 0; i < num_samples; i++) {
            dst[i] = (Sint16) (src[i] ^ 0x8000);
        }

        return dst;
    }
```

All remaining `AUDIO_*` symbols have been renamed to `SDL_AUDIO_*` for API consistency, but othewise are identical in value and usage.

In SDL2, SDL_AudioStream would convert/resample audio data during input (via SDL_AudioStreamPut). In SDL3, it does this work when requesting audio (via SDL_GetAudioStreamData, which would have been SDL_AudioStreamGet in SDL2). The way you use an AudioStream is roughly the same, just be aware that the workload moved to a different phase.

In SDL2, SDL_AudioStreamAvailable() returns 0 if passed a NULL stream. In SDL3, the equivalent SDL_GetAudioStreamAvailable() call returns -1 and sets an error string, which matches other audiostream APIs' behavior.

In SDL2, SDL_AUDIODEVICEREMOVED events would fire for open devices with the `which` field set to the SDL_AudioDeviceID of the lost device, and in later SDL2 releases, would also fire this event with a `which` field of zero for unopened devices, to signify that the app might want to refresh the available device list. In SDL3, this event works the same, except it won't ever fire with a zero; in this case it'll return the physical device's SDL_AudioDeviceID. Any still-open SDL_AudioDeviceIDs generated from this device with SDL_OpenAudioDevice() will also fire a separate event.

The following functions have been renamed:
* SDL_AudioStreamAvailable() => SDL_GetAudioStreamAvailable()
* SDL_AudioStreamClear() => SDL_ClearAudioStream()
* SDL_AudioStreamFlush() => SDL_FlushAudioStream()
* SDL_AudioStreamGet() => SDL_GetAudioStreamData()
* SDL_AudioStreamPut() => SDL_PutAudioStreamData()
* SDL_FreeAudioStream() => SDL_DestroyAudioStream()
* SDL_NewAudioStream() => SDL_CreateAudioStream()


The following functions have been removed:
* SDL_GetNumAudioDevices()
* SDL_GetAudioDeviceSpec()
* SDL_ConvertAudio()
* SDL_BuildAudioCVT()
* SDL_OpenAudio()
* SDL_CloseAudio()
* SDL_PauseAudio()
* SDL_GetAudioStatus()
* SDL_GetAudioDeviceStatus()
* SDL_GetDefaultAudioInfo()
* SDL_LockAudio()
* SDL_LockAudioDevice()
* SDL_UnlockAudio()
* SDL_UnlockAudioDevice()
* SDL_MixAudio()
* SDL_QueueAudio()
* SDL_DequeueAudio()
* SDL_ClearAudioQueue()
* SDL_GetQueuedAudioSize()

The following symbols have been renamed:
* AUDIO_F32 => SDL_AUDIO_F32LE
* AUDIO_F32LSB => SDL_AUDIO_F32LE
* AUDIO_F32MSB => SDL_AUDIO_F32BE
* AUDIO_F32SYS => SDL_AUDIO_F32
* AUDIO_S16 => SDL_AUDIO_S16LE
* AUDIO_S16LSB => SDL_AUDIO_S16LE
* AUDIO_S16MSB => SDL_AUDIO_S16BE
* AUDIO_S16SYS => SDL_AUDIO_S16
* AUDIO_S32 => SDL_AUDIO_S32LE
* AUDIO_S32LSB => SDL_AUDIO_S32LE
* AUDIO_S32MSB => SDL_AUDIO_S32BE
* AUDIO_S32SYS => SDL_AUDIO_S32
* AUDIO_S8 => SDL_AUDIO_S8
* AUDIO_U8 => SDL_AUDIO_U8

## SDL_cpuinfo.h

The intrinsics headers (mmintrin.h, etc.) have been moved to `<SDL3/SDL_intrin.h>` and are no longer automatically included in SDL.h.

SDL_Has3DNow() has been removed; there is no replacement.

SDL_HasRDTSC() has been removed; there is no replacement. Don't use the RDTSC opcode in modern times, use SDL_GetPerformanceCounter and SDL_GetPerformanceFrequency instead.

SDL_SIMDAlloc(), SDL_SIMDRealloc(), and SDL_SIMDFree() have been removed. You can use SDL_aligned_alloc() and SDL_aligned_free() with SDL_SIMDGetAlignment() to get the same functionality.

## SDL_error.h

The following functions have been removed:
* SDL_GetErrorMsg() - Can be implemented as `SDL_strlcpy(errstr, SDL_GetError(), maxlen);`

## SDL_events.h

The timestamp member of the SDL_Event structure now represents nanoseconds, and is populated with SDL_GetTicksNS()

The timestamp_us member of the sensor events has been renamed sensor_timestamp and now represents nanoseconds. This value is filled in from the hardware, if available, and may not be synchronized with values returned from SDL_GetTicksNS().

You should set the event.common.timestamp field before passing an event to SDL_PushEvent(). If the timestamp is 0 it will be filled in with SDL_GetTicksNS().

Event memory is now managed by SDL, so you should not free the data in SDL_EVENT_DROP_FILE, and if you want to hold onto the text in SDL_EVENT_TEXT_EDITING and SDL_EVENT_TEXT_INPUT events, you should make a copy of it.

Mouse events use floating point values for mouse coordinates and relative motion values. You can get sub-pixel motion depending on the platform and display scaling.

The SDL_DISPLAYEVENT_* events have been moved to top level events, and SDL_DISPLAYEVENT has been removed. In general, handling this change just means checking for the individual events instead of first checking for SDL_DISPLAYEVENT and then checking for display events. You can compare the event >= SDL_EVENT_DISPLAY_FIRST and <= SDL_EVENT_DISPLAY_LAST if you need to see whether it's a display event.

The SDL_WINDOWEVENT_* events have been moved to top level events, and SDL_WINDOWEVENT has been removed. In general, handling this change just means checking for the individual events instead of first checking for SDL_WINDOWEVENT and then checking for window events. You can compare the event >= SDL_EVENT_WINDOW_FIRST and <= SDL_EVENT_WINDOW_LAST if you need to see whether it's a window event.

The SDL_EVENT_WINDOW_RESIZED event is always sent, even in response to SDL_SetWindowSize().

The SDL_EVENT_WINDOW_SIZE_CHANGED event has been removed, and you can use SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED to detect window backbuffer size changes.

The gamepad event structures caxis, cbutton, cdevice, ctouchpad, and csensor have been renamed gaxis, gbutton, gdevice, gtouchpad, and gsensor.

The mouseX and mouseY fields of SDL_MouseWheelEvent have been renamed mouse_x and mouse_y.

The touchId and fingerId fields of SDL_TouchFingerEvent have been renamed touchID and fingerID.

SDL_QUERY, SDL_IGNORE, SDL_ENABLE, and SDL_DISABLE have been removed. You can use the functions SDL_SetEventEnabled() and SDL_EventEnabled() to set and query event processing state.

SDL_AddEventWatch() now returns -1 if it fails because it ran out of memory and couldn't add the event watch callback.

The following symbols have been renamed:
* SDL_APP_DIDENTERBACKGROUND => SDL_EVENT_DID_ENTER_BACKGROUND
* SDL_APP_DIDENTERFOREGROUND => SDL_EVENT_DID_ENTER_FOREGROUND
* SDL_APP_LOWMEMORY => SDL_EVENT_LOW_MEMORY
* SDL_APP_TERMINATING => SDL_EVENT_TERMINATING
* SDL_APP_WILLENTERBACKGROUND => SDL_EVENT_WILL_ENTER_BACKGROUND
* SDL_APP_WILLENTERFOREGROUND => SDL_EVENT_WILL_ENTER_FOREGROUND
* SDL_AUDIODEVICEADDED => SDL_EVENT_AUDIO_DEVICE_ADDED
* SDL_AUDIODEVICEREMOVED => SDL_EVENT_AUDIO_DEVICE_REMOVED
* SDL_CLIPBOARDUPDATE => SDL_EVENT_CLIPBOARD_UPDATE
* SDL_CONTROLLERAXISMOTION => SDL_EVENT_GAMEPAD_AXIS_MOTION
* SDL_CONTROLLERBUTTONDOWN => SDL_EVENT_GAMEPAD_BUTTON_DOWN
* SDL_CONTROLLERBUTTONUP => SDL_EVENT_GAMEPAD_BUTTON_UP
* SDL_CONTROLLERDEVICEADDED => SDL_EVENT_GAMEPAD_ADDED
* SDL_CONTROLLERDEVICEREMAPPED => SDL_EVENT_GAMEPAD_REMAPPED
* SDL_CONTROLLERDEVICEREMOVED => SDL_EVENT_GAMEPAD_REMOVED
* SDL_CONTROLLERSENSORUPDATE => SDL_EVENT_GAMEPAD_SENSOR_UPDATE
* SDL_CONTROLLERSTEAMHANDLEUPDATED => SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED
* SDL_CONTROLLERTOUCHPADDOWN => SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN
* SDL_CONTROLLERTOUCHPADMOTION => SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION
* SDL_CONTROLLERTOUCHPADUP => SDL_EVENT_GAMEPAD_TOUCHPAD_UP
* SDL_DROPBEGIN => SDL_EVENT_DROP_BEGIN
* SDL_DROPCOMPLETE => SDL_EVENT_DROP_COMPLETE
* SDL_DROPFILE => SDL_EVENT_DROP_FILE
* SDL_DROPTEXT => SDL_EVENT_DROP_TEXT
* SDL_FINGERDOWN => SDL_EVENT_FINGER_DOWN
* SDL_FINGERMOTION => SDL_EVENT_FINGER_MOTION
* SDL_FINGERUP => SDL_EVENT_FINGER_UP
* SDL_FIRSTEVENT => SDL_EVENT_FIRST
* SDL_JOYAXISMOTION => SDL_EVENT_JOYSTICK_AXIS_MOTION
* SDL_JOYBATTERYUPDATED => SDL_EVENT_JOYSTICK_BATTERY_UPDATED
* SDL_JOYBUTTONDOWN => SDL_EVENT_JOYSTICK_BUTTON_DOWN
* SDL_JOYBUTTONUP => SDL_EVENT_JOYSTICK_BUTTON_UP
* SDL_JOYDEVICEADDED => SDL_EVENT_JOYSTICK_ADDED
* SDL_JOYDEVICEREMOVED => SDL_EVENT_JOYSTICK_REMOVED
* SDL_JOYHATMOTION => SDL_EVENT_JOYSTICK_HAT_MOTION
* SDL_KEYDOWN => SDL_EVENT_KEY_DOWN
* SDL_KEYMAPCHANGED => SDL_EVENT_KEYMAP_CHANGED
* SDL_KEYUP => SDL_EVENT_KEY_UP
* SDL_LASTEVENT => SDL_EVENT_LAST
* SDL_LOCALECHANGED => SDL_EVENT_LOCALE_CHANGED
* SDL_MOUSEBUTTONDOWN => SDL_EVENT_MOUSE_BUTTON_DOWN
* SDL_MOUSEBUTTONUP => SDL_EVENT_MOUSE_BUTTON_UP
* SDL_MOUSEMOTION => SDL_EVENT_MOUSE_MOTION
* SDL_MOUSEWHEEL => SDL_EVENT_MOUSE_WHEEL
* SDL_POLLSENTINEL => SDL_EVENT_POLL_SENTINEL
* SDL_QUIT => SDL_EVENT_QUIT
* SDL_RENDER_DEVICE_RESET => SDL_EVENT_RENDER_DEVICE_RESET
* SDL_RENDER_TARGETS_RESET => SDL_EVENT_RENDER_TARGETS_RESET
* SDL_SENSORUPDATE => SDL_EVENT_SENSOR_UPDATE
* SDL_SYSWMEVENT => SDL_EVENT_SYSWM
* SDL_TEXTEDITING => SDL_EVENT_TEXT_EDITING
* SDL_TEXTEDITING_EXT => SDL_EVENT_TEXT_EDITING_EXT
* SDL_TEXTINPUT => SDL_EVENT_TEXT_INPUT
* SDL_USEREVENT => SDL_EVENT_USER

The following structures have been renamed:
* SDL_ControllerAxisEvent => SDL_GamepadAxisEvent
* SDL_ControllerButtonEvent => SDL_GamepadButtonEvent
* SDL_ControllerDeviceEvent => SDL_GamepadDeviceEvent
* SDL_ControllerSensorEvent => SDL_GamepadSensorEvent
* SDL_ControllerTouchpadEvent => SDL_GamepadTouchpadEvent

The following functions have been removed:
* SDL_EventState() - replaced with SDL_SetEventEnabled()
* SDL_GetEventState() - replaced with SDL_EventEnabled()

## SDL_gamecontroller.h

SDL_gamecontroller.h has been renamed SDL_gamepad.h, and all APIs have been renamed to match.

The SDL_EVENT_GAMEPAD_ADDED event now provides the joystick instance ID in the which member of the cdevice event structure.

The functions SDL_GetGamepads(), SDL_GetGamepadInstanceName(), SDL_GetGamepadInstancePath(), SDL_GetGamepadInstancePlayerIndex(), SDL_GetGamepadInstanceGUID(), SDL_GetGamepadInstanceVendor(), SDL_GetGamepadInstanceProduct(), SDL_GetGamepadInstanceProductVersion(), and SDL_GetGamepadInstanceType() have been added to directly query the list of available gamepads.

The gamepad face buttons have been renamed from A/B/X/Y to North/South/East/West to indicate that they are positional rather than hardware-specific. You can use SDL_GetGamepadButtonLabel() to get the labels for the face buttons, e.g. A/B/X/Y or Cross/Circle/Square/Triangle. The hint SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS is ignored, and mappings that use this hint are translated correctly into positional buttons. Applications should provide a way for users to swap between South/East as their accept/cancel buttons, as this varies based on region and muscle memory. You can use an approach similar to the following to handle this:

```c
#define CONFIRM_BUTTON SDL_GAMEPAD_BUTTON_SOUTH
#define CANCEL_BUTTON SDL_GAMEPAD_BUTTON_EAST

SDL_bool flipped_buttons;

void InitMappedButtons(SDL_Gamepad *gamepad)
{
    if (!GetFlippedButtonSetting(&flipped_buttons)) {
        if (SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B) {
            flipped_buttons = SDL_TRUE;
        } else {
            flipped_buttons = SDL_FALSE;
        }
    }
}

SDL_GamepadButton GetMappedButton(SDL_GamepadButton button)
{
    if (flipped_buttons) {
        switch (button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return SDL_GAMEPAD_BUTTON_EAST;
        case SDL_GAMEPAD_BUTTON_EAST:
            return SDL_GAMEPAD_BUTTON_SOUTH;
        case SDL_GAMEPAD_BUTTON_WEST:
            return SDL_GAMEPAD_BUTTON_NORTH;
        case SDL_GAMEPAD_BUTTON_NORTH:
            return SDL_GAMEPAD_BUTTON_WEST;
        default:
            break;
        }
    }
    return button;
}

SDL_GamepadButtonLabel GetConfirmActionLabel(SDL_Gamepad *gamepad)
{
    return SDL_GetGamepadButtonLabel(gamepad, GetMappedButton(CONFIRM_BUTTON));
}

SDL_GamepadButtonLabel GetCancelActionLabel(SDL_Gamepad *gamepad)
{
    return SDL_GetGamepadButtonLabel(gamepad, GetMappedButton(CANCEL_BUTTON));
}

void HandleGamepadEvent(SDL_Event *event)
{
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        switch (GetMappedButton(event->gbutton.button)) {
        case CONFIRM_BUTTON:
            /* Handle confirm action */
            break;
        case CANCEL_BUTTON:
            /* Handle cancel action */
            break;
        default:
            /* ... */
            break;
        }
    }
}
```

SDL_GameControllerGetSensorDataWithTimestamp() has been removed. If you want timestamps for the sensor data, you should use the sensor_timestamp member of SDL_EVENT_GAMEPAD_SENSOR_UPDATE events.

SDL_CONTROLLER_TYPE_VIRTUAL has been removed, so virtual controllers can emulate other gamepad types. If you need to know whether a controller is virtual, you can use SDL_IsJoystickVirtual().

SDL_CONTROLLER_TYPE_AMAZON_LUNA has been removed, and can be replaced with this code:
```c
SDL_bool SDL_IsJoystickAmazonLunaController(Uint16 vendor_id, Uint16 product_id)
{
    return ((vendor_id == 0x1949 && product_id == 0x0419) ||
            (vendor_id == 0x0171 && product_id == 0x0419));
}
```

SDL_CONTROLLER_TYPE_GOOGLE_STADIA has been removed, and can be replaced with this code:
```c
SDL_bool SDL_IsJoystickGoogleStadiaController(Uint16 vendor_id, Uint16 product_id)
{
    return (vendor_id == 0x18d1 && product_id == 0x9400);
}
```

SDL_CONTROLLER_TYPE_NVIDIA_SHIELD has been removed, and can be replaced with this code:
```c
SDL_bool SDL_IsJoystickNVIDIASHIELDController(Uint16 vendor_id, Uint16 product_id)
{
    return (vendor_id == 0x0955 && (product_id == 0x7210 || product_id == 0x7214));
}
```

The inputType and outputType fields of SDL_GamepadBinding have been renamed input_type and output_type.

The following enums have been renamed:
* SDL_GameControllerAxis => SDL_GamepadAxis
* SDL_GameControllerBindType => SDL_GamepadBindingType
* SDL_GameControllerButton => SDL_GamepadButton
* SDL_GameControllerType => SDL_GamepadType

The following structures have been renamed:
* SDL_GameController => SDL_Gamepad

The following functions have been renamed:
* SDL_GameControllerAddMapping() => SDL_AddGamepadMapping()
* SDL_GameControllerAddMappingsFromFile() => SDL_AddGamepadMappingsFromFile()
* SDL_GameControllerAddMappingsFromRW() => SDL_AddGamepadMappingsFromRW()
* SDL_GameControllerClose() => SDL_CloseGamepad()
* SDL_GameControllerFromInstanceID() => SDL_GetGamepadFromInstanceID()
* SDL_GameControllerFromPlayerIndex() => SDL_GetGamepadFromPlayerIndex()
* SDL_GameControllerGetAppleSFSymbolsNameForAxis() => SDL_GetGamepadAppleSFSymbolsNameForAxis()
* SDL_GameControllerGetAppleSFSymbolsNameForButton() => SDL_GetGamepadAppleSFSymbolsNameForButton()
* SDL_GameControllerGetAttached() => SDL_GamepadConnected()
* SDL_GameControllerGetAxis() => SDL_GetGamepadAxis()
* SDL_GameControllerGetAxisFromString() => SDL_GetGamepadAxisFromString()
* SDL_GameControllerGetButton() => SDL_GetGamepadButton()
* SDL_GameControllerGetButtonFromString() => SDL_GetGamepadButtonFromString()
* SDL_GameControllerGetFirmwareVersion() => SDL_GetGamepadFirmwareVersion()
* SDL_GameControllerGetJoystick() => SDL_GetGamepadJoystick()
* SDL_GameControllerGetNumTouchpadFingers() => SDL_GetNumGamepadTouchpadFingers()
* SDL_GameControllerGetNumTouchpads() => SDL_GetNumGamepadTouchpads()
* SDL_GameControllerGetPlayerIndex() => SDL_GetGamepadPlayerIndex()
* SDL_GameControllerGetProduct() => SDL_GetGamepadProduct()
* SDL_GameControllerGetProductVersion() => SDL_GetGamepadProductVersion()
* SDL_GameControllerGetSensorData() => SDL_GetGamepadSensorData()
* SDL_GameControllerGetSensorDataRate() => SDL_GetGamepadSensorDataRate()
* SDL_GameControllerGetSerial() => SDL_GetGamepadSerial()
* SDL_GameControllerGetSteamHandle() => SDL_GetGamepadSteamHandle()
* SDL_GameControllerGetStringForAxis() => SDL_GetGamepadStringForAxis()
* SDL_GameControllerGetStringForButton() => SDL_GetGamepadStringForButton()
* SDL_GameControllerGetTouchpadFinger() => SDL_GetGamepadTouchpadFinger()
* SDL_GameControllerGetType() => SDL_GetGamepadType()
* SDL_GameControllerGetVendor() => SDL_GetGamepadVendor()
* SDL_GameControllerHasAxis() => SDL_GamepadHasAxis()
* SDL_GameControllerHasButton() => SDL_GamepadHasButton()
* SDL_GameControllerHasSensor() => SDL_GamepadHasSensor()
* SDL_GameControllerIsSensorEnabled() => SDL_GamepadSensorEnabled()
* SDL_GameControllerMapping() => SDL_GetGamepadMapping()
* SDL_GameControllerMappingForGUID() => SDL_GetGamepadMappingForGUID()
* SDL_GameControllerName() => SDL_GetGamepadName()
* SDL_GameControllerOpen() => SDL_OpenGamepad()
* SDL_GameControllerPath() => SDL_GetGamepadPath()
* SDL_GameControllerRumble() => SDL_RumbleGamepad()
* SDL_GameControllerRumbleTriggers() => SDL_RumbleGamepadTriggers()
* SDL_GameControllerSendEffect() => SDL_SendGamepadEffect()
* SDL_GameControllerSetLED() => SDL_SetGamepadLED()
* SDL_GameControllerSetPlayerIndex() => SDL_SetGamepadPlayerIndex()
* SDL_GameControllerSetSensorEnabled() => SDL_SetGamepadSensorEnabled()
* SDL_GameControllerUpdate() => SDL_UpdateGamepads()
* SDL_IsGameController() => SDL_IsGamepad()

The following functions have been removed:
* SDL_GameControllerEventState() - replaced with SDL_SetGamepadEventsEnabled() and SDL_GamepadEventsEnabled()
* SDL_GameControllerGetBindForAxis() - replaced with SDL_GetGamepadBindings()
* SDL_GameControllerGetBindForButton() - replaced with SDL_GetGamepadBindings()
* SDL_GameControllerHasLED() - replaced with SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN
* SDL_GameControllerHasRumble() - replaced with SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN
* SDL_GameControllerHasRumbleTriggers() - replaced with SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN
* SDL_GameControllerMappingForDeviceIndex() - replaced with SDL_GetGamepadInstanceMapping()
* SDL_GameControllerMappingForIndex() - replaced with SDL_GetGamepadMappings()
* SDL_GameControllerNameForIndex() - replaced with SDL_GetGamepadInstanceName()
* SDL_GameControllerNumMappings() - replaced with SDL_GetGamepadMappings()
* SDL_GameControllerPathForIndex() - replaced with SDL_GetGamepadInstancePath()
* SDL_GameControllerTypeForIndex() - replaced with SDL_GetGamepadInstanceType()

The following symbols have been renamed:
* SDL_CONTROLLER_AXIS_INVALID => SDL_GAMEPAD_AXIS_INVALID
* SDL_CONTROLLER_AXIS_LEFTX => SDL_GAMEPAD_AXIS_LEFTX
* SDL_CONTROLLER_AXIS_LEFTY => SDL_GAMEPAD_AXIS_LEFTY
* SDL_CONTROLLER_AXIS_MAX => SDL_GAMEPAD_AXIS_MAX
* SDL_CONTROLLER_AXIS_RIGHTX => SDL_GAMEPAD_AXIS_RIGHTX
* SDL_CONTROLLER_AXIS_RIGHTY => SDL_GAMEPAD_AXIS_RIGHTY
* SDL_CONTROLLER_AXIS_TRIGGERLEFT => SDL_GAMEPAD_AXIS_LEFT_TRIGGER
* SDL_CONTROLLER_AXIS_TRIGGERRIGHT => SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
* SDL_CONTROLLER_BINDTYPE_AXIS => SDL_GAMEPAD_BINDTYPE_AXIS
* SDL_CONTROLLER_BINDTYPE_BUTTON => SDL_GAMEPAD_BINDTYPE_BUTTON
* SDL_CONTROLLER_BINDTYPE_HAT => SDL_GAMEPAD_BINDTYPE_HAT
* SDL_CONTROLLER_BINDTYPE_NONE => SDL_GAMEPAD_BINDTYPE_NONE
* SDL_CONTROLLER_BUTTON_A => SDL_GAMEPAD_BUTTON_SOUTH
* SDL_CONTROLLER_BUTTON_B => SDL_GAMEPAD_BUTTON_EAST
* SDL_CONTROLLER_BUTTON_BACK => SDL_GAMEPAD_BUTTON_BACK
* SDL_CONTROLLER_BUTTON_DPAD_DOWN => SDL_GAMEPAD_BUTTON_DPAD_DOWN
* SDL_CONTROLLER_BUTTON_DPAD_LEFT => SDL_GAMEPAD_BUTTON_DPAD_LEFT
* SDL_CONTROLLER_BUTTON_DPAD_RIGHT => SDL_GAMEPAD_BUTTON_DPAD_RIGHT
* SDL_CONTROLLER_BUTTON_DPAD_UP => SDL_GAMEPAD_BUTTON_DPAD_UP
* SDL_CONTROLLER_BUTTON_GUIDE => SDL_GAMEPAD_BUTTON_GUIDE
* SDL_CONTROLLER_BUTTON_INVALID => SDL_GAMEPAD_BUTTON_INVALID
* SDL_CONTROLLER_BUTTON_LEFTSHOULDER => SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
* SDL_CONTROLLER_BUTTON_LEFTSTICK => SDL_GAMEPAD_BUTTON_LEFT_STICK
* SDL_CONTROLLER_BUTTON_MAX => SDL_GAMEPAD_BUTTON_MAX
* SDL_CONTROLLER_BUTTON_MISC1 => SDL_GAMEPAD_BUTTON_MISC1
* SDL_CONTROLLER_BUTTON_PADDLE1 => SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1
* SDL_CONTROLLER_BUTTON_PADDLE2 => SDL_GAMEPAD_BUTTON_LEFT_PADDLE1
* SDL_CONTROLLER_BUTTON_PADDLE3 => SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2
* SDL_CONTROLLER_BUTTON_PADDLE4 => SDL_GAMEPAD_BUTTON_LEFT_PADDLE2
* SDL_CONTROLLER_BUTTON_RIGHTSHOULDER => SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
* SDL_CONTROLLER_BUTTON_RIGHTSTICK => SDL_GAMEPAD_BUTTON_RIGHT_STICK
* SDL_CONTROLLER_BUTTON_START => SDL_GAMEPAD_BUTTON_START
* SDL_CONTROLLER_BUTTON_TOUCHPAD => SDL_GAMEPAD_BUTTON_TOUCHPAD
* SDL_CONTROLLER_BUTTON_X => SDL_GAMEPAD_BUTTON_WEST
* SDL_CONTROLLER_BUTTON_Y => SDL_GAMEPAD_BUTTON_NORTH
* SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT => SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT
* SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR => SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR
* SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT => SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT
* SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO => SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO
* SDL_CONTROLLER_TYPE_PS3 => SDL_GAMEPAD_TYPE_PS3
* SDL_CONTROLLER_TYPE_PS4 => SDL_GAMEPAD_TYPE_PS4
* SDL_CONTROLLER_TYPE_PS5 => SDL_GAMEPAD_TYPE_PS5
* SDL_CONTROLLER_TYPE_UNKNOWN => SDL_GAMEPAD_TYPE_STANDARD
* SDL_CONTROLLER_TYPE_VIRTUAL => SDL_GAMEPAD_TYPE_VIRTUAL
* SDL_CONTROLLER_TYPE_XBOX360 => SDL_GAMEPAD_TYPE_XBOX360
* SDL_CONTROLLER_TYPE_XBOXONE => SDL_GAMEPAD_TYPE_XBOXONE

## SDL_gesture.h

The gesture API has been removed. There is no replacement planned in SDL3.
However, the SDL2 code has been moved to a single-header library that can
be dropped into an SDL3 or SDL2 program, to continue to provide this
functionality to your app and aid migration. That is located in the
[SDL_gesture GitHub repository](https://github.com/libsdl-org/SDL_gesture).

## SDL_haptic.h

Gamepads with simple rumble capability no longer show up in the SDL haptics interface, instead you should use SDL_RumbleGamepad().

Rather than iterating over haptic devices using device index, there is a new function SDL_GetHaptics() to get the current list of haptic devices, and new functions to get information about haptic devices from their instance ID:
```c
{
    if (SDL_InitSubSystem(SDL_INIT_HAPTIC) == 0) {
        int i, num_haptics;
        SDL_HapticID *haptics = SDL_GetHaptics(&num_haptics);
        if (haptics) {
            for (i = 0; i < num_haptics; ++i) {
                SDL_HapticID instance_id = haptics[i];
                const char *name = SDL_GetHapticInstanceName(instance_id);

                SDL_Log("Haptic %" SDL_PRIu32 ": %s\n",
                        instance_id, name ? name : "Unknown");
            }
            SDL_free(haptics);
        }
        SDL_QuitSubSystem(SDL_INIT_HAPTIC);
    }
}
```

SDL_HapticEffectSupported(), SDL_HapticRumbleSupported(), and SDL_IsJoystickHaptic() now return SDL_bool instead of an optional negative error code.

The following functions have been renamed:
* SDL_HapticClose() => SDL_CloseHaptic()
* SDL_HapticDestroyEffect() => SDL_DestroyHapticEffect()
* SDL_HapticGetEffectStatus() => SDL_GetHapticEffectStatus()
* SDL_HapticNewEffect() => SDL_CreateHapticEffect()
* SDL_HapticNumAxes() => SDL_GetNumHapticAxes()
* SDL_HapticNumEffects() => SDL_GetMaxHapticEffects()
* SDL_HapticNumEffectsPlaying() => SDL_GetMaxHapticEffectsPlaying()
* SDL_HapticOpen() => SDL_OpenHaptic()
* SDL_HapticOpenFromJoystick() => SDL_OpenHapticFromJoystick()
* SDL_HapticOpenFromMouse() => SDL_OpenHapticFromMouse()
* SDL_HapticPause() => SDL_PauseHaptic()
* SDL_HapticQuery() => SDL_GetHapticFeatures()
* SDL_HapticRumbleInit() => SDL_InitHapticRumble()
* SDL_HapticRumblePlay() => SDL_PlayHapticRumble()
* SDL_HapticRumbleStop() => SDL_StopHapticRumble()
* SDL_HapticRunEffect() => SDL_RunHapticEffect()
* SDL_HapticSetAutocenter() => SDL_SetHapticAutocenter()
* SDL_HapticSetGain() => SDL_SetHapticGain()
* SDL_HapticStopAll() => SDL_StopHapticEffects()
* SDL_HapticStopEffect() => SDL_StopHapticEffect()
* SDL_HapticUnpause() => SDL_ResumeHaptic()
* SDL_HapticUpdateEffect() => SDL_UpdateHapticEffect()
* SDL_JoystickIsHaptic() => SDL_IsJoystickHaptic()
* SDL_MouseIsHaptic() => SDL_IsMouseHaptic()

The following functions have been removed:
* SDL_HapticIndex() - replaced with SDL_GetHapticInstanceID()
* SDL_HapticName() - replaced with SDL_GetHapticInstanceName()
* SDL_HapticOpened() - replaced with SDL_GetHapticFromInstanceID()
* SDL_NumHaptics() - replaced with SDL_GetHaptics()

## SDL_hints.h

SDL_AddHintCallback() now returns a standard int result instead of void, returning 0 if the function succeeds or a negative error code if there was an error.

Calling SDL_GetHint() with the name of the hint being changed from within a hint callback will now return the new value rather than the old value. The old value is still passed as a parameter to the hint callback.

The following hints have been removed:
* SDL_HINT_ACCELEROMETER_AS_JOYSTICK
* SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS - gamepad buttons are always positional
* SDL_HINT_IDLE_TIMER_DISABLED - use SDL_DisableScreenSaver() instead
* SDL_HINT_IME_SUPPORT_EXTENDED_TEXT - the normal text editing event has extended text
* SDL_HINT_MOUSE_RELATIVE_SCALING - mouse coordinates are no longer automatically scaled by the SDL renderer
* SDL_HINT_RENDER_BATCHING - Render batching is always enabled, apps should call SDL_FlushRenderer() before calling into a lower-level graphics API.
* SDL_HINT_RENDER_LOGICAL_SIZE_MODE - the logical size mode is explicitly set with SDL_SetRenderLogicalPresentation()
* SDL_HINT_RENDER_OPENGL_SHADERS - shaders are always used if they are available
* SDL_HINT_RENDER_SCALE_QUALITY - textures now default to linear filtering, use SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST) if you want nearest pixel mode instead
* SDL_HINT_VIDEO_EXTERNAL_CONTEXT - replaced with SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN in SDL_CreateWindowWithProperties()
* SDL_HINT_THREAD_STACK_SIZE - the stack size can be specified using SDL_CreateThreadWithStackSize()
* SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL - replaced with SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN in SDL_CreateWindowWithProperties()
* SDL_HINT_VIDEO_FOREIGN_WINDOW_VULKAN - replaced with SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN in SDL_CreateWindowWithProperties()
* SDL_HINT_VIDEO_HIGHDPI_DISABLED - high DPI support is always enabled
* SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT - replaced with SDL_PROP_WINDOW_CREATE_WIN32_PIXEL_FORMAT_HWND_POINTER in SDL_CreateWindowWithProperties()
* SDL_HINT_VIDEO_X11_FORCE_EGL - use SDL_HINT_VIDEO_FORCE_EGL instead
* SDL_HINT_VIDEO_X11_XINERAMA - Xinerama no longer supported by the X11 backend
* SDL_HINT_VIDEO_X11_XVIDMODE - Xvidmode no longer supported by the X11 backend
* SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING - SDL now properly handles the 0x406D1388 Exception if no debugger intercepts it, preventing its propagation.
* SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4 - replaced with SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, defaulting to SDL_TRUE
* SDL_HINT_XINPUT_USE_OLD_JOYSTICK_MAPPING

* Renamed hints SDL_HINT_VIDEODRIVER and SDL_HINT_AUDIODRIVER to SDL_HINT_VIDEO_DRIVER and SDL_HINT_AUDIO_DRIVER
* Renamed environment variables SDL_VIDEODRIVER and SDL_AUDIODRIVER to SDL_VIDEO_DRIVER and SDL_AUDIO_DRIVER
* The environment variables SDL_VIDEO_X11_WMCLASS and SDL_VIDEO_WAYLAND_WMCLASS have been removed and replaced with the unified hint SDL_HINT_APP_ID

The following hints have been renamed:
* SDL_HINT_ALLOW_TOPMOST => SDL_HINT_WINDOW_ALLOW_TOPMOST
* SDL_HINT_DIRECTINPUT_ENABLED => SDL_HINT_JOYSTICK_DIRECTINPUT
* SDL_HINT_GDK_TEXTINPUT_DEFAULT => SDL_HINT_GDK_TEXTINPUT_DEFAULT_TEXT
* SDL_HINT_JOYSTICK_GAMECUBE_RUMBLE_BRAKE => SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE_RUMBLE_BRAKE
* SDL_HINT_LINUX_DIGITAL_HATS => SDL_HINT_JOYSTICK_LINUX_DIGITAL_HATS
* SDL_HINT_LINUX_HAT_DEADZONES => SDL_HINT_JOYSTICK_LINUX_HAT_DEADZONES
* SDL_HINT_LINUX_JOYSTICK_CLASSIC => SDL_HINT_JOYSTICK_LINUX_CLASSIC
* SDL_HINT_LINUX_JOYSTICK_DEADZONES => SDL_HINT_JOYSTICK_LINUX_DEADZONES
* SDL_HINT_PS2_DYNAMIC_VSYNC => SDL_HINT_RENDER_PS2_DYNAMIC_VSYNC

## SDL_init.h

The following symbols have been renamed:
* SDL_INIT_GAMECONTROLLER => SDL_INIT_GAMEPAD

The following symbols have been removed:
* SDL_INIT_NOPARACHUTE
* SDL_INIT_EVERYTHING - you should only initialize the subsystems you are using

## SDL_joystick.h

SDL_JoystickID has changed from Sint32 to Uint32, with an invalid ID being 0.

Rather than iterating over joysticks using device index, there is a new function SDL_GetJoysticks() to get the current list of joysticks, and new functions to get information about joysticks from their instance ID:
```c
{
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0) {
        int i, num_joysticks;
        SDL_JoystickID *joysticks = SDL_GetJoysticks(&num_joysticks);
        if (joysticks) {
            for (i = 0; i < num_joysticks; ++i) {
                SDL_JoystickID instance_id = joysticks[i];
                const char *name = SDL_GetJoystickInstanceName(instance_id);
                const char *path = SDL_GetJoystickInstancePath(instance_id);

                SDL_Log("Joystick %" SDL_PRIu32 ": %s%s%s VID 0x%.4x, PID 0x%.4x\n",
                        instance_id, name ? name : "Unknown", path ? ", " : "", path ? path : "", SDL_GetJoystickInstanceVendor(instance_id), SDL_GetJoystickInstanceProduct(instance_id));
            }
            SDL_free(joysticks);
        }
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }
}
```

The SDL_EVENT_JOYSTICK_ADDED event now provides the joystick instance ID in the `which` member of the jdevice event structure.

The functions SDL_GetJoysticks(), SDL_GetJoystickInstanceName(), SDL_GetJoystickInstancePath(), SDL_GetJoystickInstancePlayerIndex(), SDL_GetJoystickInstanceGUID(), SDL_GetJoystickInstanceVendor(), SDL_GetJoystickInstanceProduct(), SDL_GetJoystickInstanceProductVersion(), and SDL_GetJoystickInstanceType() have been added to directly query the list of available joysticks.

SDL_AttachVirtualJoystick() and SDL_AttachVirtualJoystickEx() now return the joystick instance ID instead of a device index, and return 0 if there was an error.

The following functions have been renamed:
* SDL_JoystickAttachVirtual() => SDL_AttachVirtualJoystick()
* SDL_JoystickAttachVirtualEx() => SDL_AttachVirtualJoystickEx()
* SDL_JoystickClose() => SDL_CloseJoystick()
* SDL_JoystickCurrentPowerLevel() => SDL_GetJoystickPowerLevel()
* SDL_JoystickDetachVirtual() => SDL_DetachVirtualJoystick()
* SDL_JoystickFromInstanceID() => SDL_GetJoystickFromInstanceID()
* SDL_JoystickFromPlayerIndex() => SDL_GetJoystickFromPlayerIndex()
* SDL_JoystickGetAttached() => SDL_JoystickConnected()
* SDL_JoystickGetAxis() => SDL_GetJoystickAxis()
* SDL_JoystickGetAxisInitialState() => SDL_GetJoystickAxisInitialState()
* SDL_JoystickGetButton() => SDL_GetJoystickButton()
* SDL_JoystickGetFirmwareVersion() => SDL_GetJoystickFirmwareVersion()
* SDL_JoystickGetGUID() => SDL_GetJoystickGUID()
* SDL_JoystickGetGUIDFromString() => SDL_GetJoystickGUIDFromString()
* SDL_JoystickGetGUIDString() => SDL_GetJoystickGUIDString()
* SDL_JoystickGetHat() => SDL_GetJoystickHat()
* SDL_JoystickGetPlayerIndex() => SDL_GetJoystickPlayerIndex()
* SDL_JoystickGetProduct() => SDL_GetJoystickProduct()
* SDL_JoystickGetProductVersion() => SDL_GetJoystickProductVersion()
* SDL_JoystickGetSerial() => SDL_GetJoystickSerial()
* SDL_JoystickGetType() => SDL_GetJoystickType()
* SDL_JoystickGetVendor() => SDL_GetJoystickVendor()
* SDL_JoystickInstanceID() => SDL_GetJoystickInstanceID()
* SDL_JoystickIsVirtual() => SDL_IsJoystickVirtual()
* SDL_JoystickName() => SDL_GetJoystickName()
* SDL_JoystickNumAxes() => SDL_GetNumJoystickAxes()
* SDL_JoystickNumButtons() => SDL_GetNumJoystickButtons()
* SDL_JoystickNumHats() => SDL_GetNumJoystickHats()
* SDL_JoystickOpen() => SDL_OpenJoystick()
* SDL_JoystickPath() => SDL_GetJoystickPath()
* SDL_JoystickRumble() => SDL_RumbleJoystick()
* SDL_JoystickRumbleTriggers() => SDL_RumbleJoystickTriggers()
* SDL_JoystickSendEffect() => SDL_SendJoystickEffect()
* SDL_JoystickSetLED() => SDL_SetJoystickLED()
* SDL_JoystickSetPlayerIndex() => SDL_SetJoystickPlayerIndex()
* SDL_JoystickSetVirtualAxis() => SDL_SetJoystickVirtualAxis()
* SDL_JoystickSetVirtualButton() => SDL_SetJoystickVirtualButton()
* SDL_JoystickSetVirtualHat() => SDL_SetJoystickVirtualHat()
* SDL_JoystickUpdate() => SDL_UpdateJoysticks()

The following symbols have been renamed:
* SDL_JOYSTICK_TYPE_GAMECONTROLLER => SDL_JOYSTICK_TYPE_GAMEPAD

The following functions have been removed:
* SDL_JoystickEventState() - replaced with SDL_SetJoystickEventsEnabled() and SDL_JoystickEventsEnabled()
* SDL_JoystickGetDeviceGUID() - replaced with SDL_GetJoystickInstanceGUID()
* SDL_JoystickGetDeviceInstanceID()
* SDL_JoystickGetDevicePlayerIndex() - replaced with SDL_GetJoystickInstancePlayerIndex()
* SDL_JoystickGetDeviceProduct() - replaced with SDL_GetJoystickInstanceProduct()
* SDL_JoystickGetDeviceProductVersion() - replaced with SDL_GetJoystickInstanceProductVersion()
* SDL_JoystickGetDeviceType() - replaced with SDL_GetJoystickInstanceType()
* SDL_JoystickGetDeviceVendor() - replaced with SDL_GetJoystickInstanceVendor()
* SDL_JoystickHasLED() - replaced with SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN
* SDL_JoystickHasRumble() - replaced with SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN
* SDL_JoystickHasRumbleTriggers() - replaced with SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN
* SDL_JoystickNameForIndex() - replaced with SDL_GetJoystickInstanceName()
* SDL_JoystickNumBalls() - API has been removed, see https://github.com/libsdl-org/SDL/issues/6766
* SDL_JoystickPathForIndex() - replaced with SDL_GetJoystickInstancePath()
* SDL_NumJoysticks() - replaced with SDL_GetJoysticks()

The following symbols have been removed:
* SDL_JOYBALLMOTION

## SDL_keyboard.h

The following functions have been renamed:
* SDL_IsScreenKeyboardShown() => SDL_ScreenKeyboardShown()
* SDL_IsTextInputActive() => SDL_TextInputActive()
* SDL_IsTextInputShown() => SDL_TextInputShown()

## SDL_keycode.h

The following symbols have been renamed:
* KMOD_ALT => SDL_KMOD_ALT
* KMOD_CAPS => SDL_KMOD_CAPS
* KMOD_CTRL => SDL_KMOD_CTRL
* KMOD_GUI => SDL_KMOD_GUI
* KMOD_LALT => SDL_KMOD_LALT
* KMOD_LCTRL => SDL_KMOD_LCTRL
* KMOD_LGUI => SDL_KMOD_LGUI
* KMOD_LSHIFT => SDL_KMOD_LSHIFT
* KMOD_MODE => SDL_KMOD_MODE
* KMOD_NONE => SDL_KMOD_NONE
* KMOD_NUM => SDL_KMOD_NUM
* KMOD_RALT => SDL_KMOD_RALT
* KMOD_RCTRL => SDL_KMOD_RCTRL
* KMOD_RESERVED => SDL_KMOD_RESERVED
* KMOD_RGUI => SDL_KMOD_RGUI
* KMOD_RSHIFT => SDL_KMOD_RSHIFT
* KMOD_SCROLL => SDL_KMOD_SCROLL
* KMOD_SHIFT => SDL_KMOD_SHIFT

## SDL_loadso.h

SDL_LoadFunction() now returns `SDL_FunctionPointer` instead of `void *`, and should be cast to the appropriate function type. You can define SDL_FUNCTION_POINTER_IS_VOID_POINTER in your project to restore the previous behavior.

## SDL_main.h

SDL3 doesn't have a static libSDLmain to link against anymore.
Instead SDL_main.h is now a header-only library **and not included by SDL.h anymore**.

Using it is really simple: Just `#include <SDL3/SDL_main.h>` in the source file with your standard
`int main(int argc, char* argv[])` function. See docs/README-main-functions.md for details.

Several platform-specific entry point functions have been removed as unnecessary. If for some reason you explicitly need them, here are easy replacements:

```c
#define SDL_WinRTRunApp(MAIN_FUNC, RESERVED)  SDL_RunApp(0, NULL, MAIN_FUNC, RESERVED)
#define SDL_UIKitRunApp(ARGC, ARGV, MAIN_FUNC)  SDL_RunApp(ARGC, ARGV, MAIN_FUNC, NULL)
#define SDL_GDKRunApp(MAIN_FUNC, RESERVED)  SDL_RunApp(0, NULL, MAIN_FUNC, RESERVED)
```

## SDL_messagebox.h

The buttonid field of SDL_MessageBoxButtonData has been renamed buttonID.

## SDL_metal.h

SDL_Metal_GetDrawableSize() has been removed. SDL_GetWindowSizeInPixels() can be used in its place.

## SDL_mouse.h

SDL_ShowCursor() has been split into three functions: SDL_ShowCursor(), SDL_HideCursor(), and SDL_CursorVisible()

SDL_GetMouseState(), SDL_GetGlobalMouseState(), SDL_GetRelativeMouseState(), SDL_WarpMouseInWindow(), and SDL_WarpMouseGlobal() all use floating point mouse positions, to provide sub-pixel precision on platforms that support it.

The following functions have been renamed:
* SDL_FreeCursor() => SDL_DestroyCursor()

## SDL_mutex.h

SDL_MUTEX_MAXWAIT has been removed; it suggested there was a maximum timeout one could outlive, instead of an infinite wait. Instead, pass a -1 to functions that accepted this symbol.

SDL_LockMutex and SDL_UnlockMutex now return void; if the mutex is valid (including being a NULL pointer, which returns immediately), these functions never fail. If the mutex is invalid or the caller does something illegal, like unlock another thread's mutex, this is considered undefined behavior.

The following functions have been renamed:
* SDL_CondBroadcast() => SDL_BroadcastCondition()
* SDL_CondSignal() => SDL_SignalCondition()
* SDL_CondWait() => SDL_WaitCondition()
* SDL_CondWaitTimeout() => SDL_WaitConditionTimeout()
* SDL_CreateCond() => SDL_CreateCondition()
* SDL_DestroyCond() => SDL_DestroyCondition()
* SDL_SemPost() => SDL_PostSemaphore()
* SDL_SemTryWait() => SDL_TryWaitSemaphore()
* SDL_SemValue() => SDL_GetSemaphoreValue()
* SDL_SemWait() => SDL_WaitSemaphore()
* SDL_SemWaitTimeout() => SDL_WaitSemaphoreTimeout()

The following symbols have been renamed:
* SDL_cond => SDL_Condition
* SDL_mutex => SDL_Mutex
* SDL_sem => SDL_Semaphore

## SDL_pixels.h

SDL_CalculateGammaRamp has been removed, because SDL_SetWindowGammaRamp has been removed as well due to poor support in modern operating systems (see [SDL_video.h](#sdl_videoh)).

The BitsPerPixel and BytesPerPixel fields of SDL_PixelFormat have been renamed bits_per_pixel and bytes_per_pixel.

The following functions have been renamed:
* SDL_AllocFormat() => SDL_CreatePixelFormat()
* SDL_AllocPalette() => SDL_CreatePalette()
* SDL_FreeFormat() => SDL_DestroyPixelFormat()
* SDL_FreePalette() => SDL_DestroyPalette()
* SDL_MasksToPixelFormatEnum() => SDL_GetPixelFormatEnumForMasks()
* SDL_PixelFormatEnumToMasks() => SDL_GetMasksForPixelFormatEnum()

The following symbols have been renamed:
* SDL_DISPLAYEVENT_DISCONNECTED => SDL_EVENT_DISPLAY_REMOVED
* SDL_DISPLAYEVENT_MOVED => SDL_EVENT_DISPLAY_MOVED
* SDL_DISPLAYEVENT_ORIENTATION => SDL_EVENT_DISPLAY_ORIENTATION
* SDL_WINDOWEVENT_CLOSE => SDL_EVENT_WINDOW_CLOSE_REQUESTED
* SDL_WINDOWEVENT_DISPLAY_CHANGED => SDL_EVENT_WINDOW_DISPLAY_CHANGED
* SDL_WINDOWEVENT_ENTER => SDL_EVENT_WINDOW_ENTER
* SDL_WINDOWEVENT_EXPOSED => SDL_EVENT_WINDOW_EXPOSED
* SDL_WINDOWEVENT_FOCUS_GAINED => SDL_EVENT_WINDOW_FOCUS_GAINED
* SDL_WINDOWEVENT_FOCUS_LOST => SDL_EVENT_WINDOW_FOCUS_LOST
* SDL_WINDOWEVENT_HIDDEN => SDL_EVENT_WINDOW_HIDDEN
* SDL_WINDOWEVENT_HIT_TEST => SDL_EVENT_WINDOW_HIT_TEST
* SDL_WINDOWEVENT_ICCPROF_CHANGED => SDL_EVENT_WINDOW_ICCPROF_CHANGED
* SDL_WINDOWEVENT_LEAVE => SDL_EVENT_WINDOW_LEAVE
* SDL_WINDOWEVENT_MAXIMIZED => SDL_EVENT_WINDOW_MAXIMIZED
* SDL_WINDOWEVENT_MINIMIZED => SDL_EVENT_WINDOW_MINIMIZED
* SDL_WINDOWEVENT_MOVED => SDL_EVENT_WINDOW_MOVED
* SDL_WINDOWEVENT_RESIZED => SDL_EVENT_WINDOW_RESIZED
* SDL_WINDOWEVENT_RESTORED => SDL_EVENT_WINDOW_RESTORED
* SDL_WINDOWEVENT_SHOWN => SDL_EVENT_WINDOW_SHOWN
* SDL_WINDOWEVENT_SIZE_CHANGED => SDL_EVENT_WINDOW_SIZE_CHANGED
* SDL_WINDOWEVENT_TAKE_FOCUS => SDL_EVENT_WINDOW_TAKE_FOCUS


## SDL_platform.h

The following platform preprocessor macros have been renamed:

| SDL2              | SDL3                      |
|-------------------|---------------------------|
| `__3DS__`         | `SDL_PLATFORM_3DS`        |
| `__AIX__`         | `SDL_PLATFORM_AIX`        |
| `__ANDROID__`     | `SDL_PLATFORM_ANDROID`    |
| `__APPLE__`       | `SDL_PLATFORM_APPLE`      |
| `__BSDI__`        | `SDL_PLATFORM_BSDI`       |
| `__CYGWIN_`       | `SDL_PLATFORM_CYGWIN`     |
| `__EMSCRIPTEN__`  | `SDL_PLATFORM_EMSCRIPTEN` |
| `__FREEBSD__`     | `SDL_PLATFORM_FREEBSD`    |
| `__GDK__`         | `SDL_PLATFORM_GDK`        |
| `__HAIKU__`       | `SDL_PLATFORM_HAIKU`      |
| `__HPUX__`        | `SDL_PLATFORM_HPUX`       |
| `__IPHONEOS__`    | `SDL_PLATFORM_IOS`        |
| `__IRIX__`        | `SDL_PLATFORM_IRIX`       |
| `__LINUX__`       | `SDL_PLATFORM_LINUX`      |
| `__MACOSX__`      | `SDL_PLATFORM_MACOS`      |
| `__NETBSD__`      | `SDL_PLATFORM_NETBSD`     |
| `__NGAGE__`       | `SDL_PLATFORM_NGAGE`      |
| `__OPENBSD__`     | `SDL_PLATFORM_OPENBSD`    |
| `__OS2__`         | `SDL_PLATFORM_OS2`        |
| `__OSF__`         | `SDL_PLATFORM_OSF`        |
| `__PS2__`         | `SDL_PLATFORM_PS2`        |
| `__PSP__`         | `SDL_PLATFORM_PSP`        |
| `__QNXNTO__`      | `SDL_PLATFORM_QNXNTO`     |
| `__RISCOS__`      | `SDL_PLATFORM_RISCOS`     |
| `__SOLARIS__`     | `SDL_PLATFORM_SOLARIS`    |
| `__TVOS__`        | `SDL_PLATFORM_TVOS`       |
| `__unix__`        | `SDL_PLATFORM_UNI`        |
| `__VITA__`        | `SDL_PLATFORM_VITA`       |
| `__WIN32__`       | `SDL_PLATFORM_WIN32`      |
| `__WINGDK__`      | `SDL_PLATFORM_WINGDK`     |
| `__WINRT__`       | `SDL_PLATFORM_WINRT`      |
| `__XBOXONE__`     | `SDL_PLATFORM_XBOXONE`    |
| `__XBOXSERIES__`  | `SDL_PLATFORM_XBOXSERIES` |

You can use the Python script [rename_macros.py](https://github.com/libsdl-org/SDL/blob/main/build-scripts/rename_macros.py) to automatically rename these in your source code.

A new macro `SDL_PLATFORM_WINDOWS` has been added that is true for all Windows platforms, including Xbox, GDK, etc.

The following platform preprocessor macros have been removed:
* `__DREAMCAST__`
* `__NACL__`
* `__PNACL__`
* `__WINDOWS__`

## SDL_rect.h

The following functions have been renamed:
* SDL_EncloseFPoints() => SDL_GetRectEnclosingPointsFloat()
* SDL_EnclosePoints() => SDL_GetRectEnclosingPoints()
* SDL_FRectEmpty() => SDL_RectEmptyFloat()
* SDL_FRectEquals() => SDL_RectsEqualFloat()
* SDL_FRectEqualsEpsilon() => SDL_RectsEqualEpsilon()
* SDL_HasIntersection() => SDL_HasRectIntersection()
* SDL_HasIntersectionF() => SDL_HasRectIntersectionFloat()
* SDL_IntersectFRect() => SDL_GetRectIntersectionFloat()
* SDL_IntersectFRectAndLine() => SDL_GetRectAndLineIntersectionFloat()
* SDL_IntersectRect() => SDL_GetRectIntersection()
* SDL_IntersectRectAndLine() => SDL_GetRectAndLineIntersection()
* SDL_PointInFRect() => SDL_PointInRectFloat()
* SDL_RectEquals() => SDL_RectsEqual()
* SDL_UnionFRect() => SDL_GetRectUnionFloat()
* SDL_UnionRect() => SDL_GetRectUnion()

## SDL_render.h

The 2D renderer API always uses batching in SDL3. There is no magic to turn
it on and off; it doesn't matter if you select a specific renderer or try to
use any hint. This means that all apps that use SDL3's 2D renderer and also
want to call directly into the platform's lower-layer graphics API _must_ call
SDL_FlushRenderer() before doing so. This will make sure any pending rendering
work from SDL is done before the app starts directly drawing.

SDL_GetRenderDriverInfo() has been removed, since most of the information it reported were
estimates and could not be accurate before creating a renderer. Often times this function
was used to figure out the index of a driver, so one would call it in a for-loop, looking
for the driver named "opengl" or whatnot. SDL_GetRenderDriver() has been added for this
functionality, which returns only the name of the driver.

Additionally, SDL_CreateRenderer()'s second argument is no longer an integer index, but a
`const char *` representing a renderer's name; if you were just using a for-loop to find
which index is the "opengl" or whatnot driver, you can just pass that string directly
here, now. Passing NULL is the same as passing -1 here in SDL2, to signify you want SDL
to decide for you.

The SDL_RENDERER_TARGETTEXTURE flag has been removed, all current renderers support target texture functionality.

When a renderer is created, it will automatically set the logical size to the size of
the window in points. For high DPI displays, this will set up scaling from points to
pixels. You can disable this scaling with:
```c
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED, SDL_SCALEMODE_NEAREST);
```

Mouse and touch events are no longer filtered to change their coordinates, instead you
can call SDL_ConvertEventToRenderCoordinates() to explicitly map event coordinates into
the rendering viewport.

SDL_RenderWindowToLogical() and SDL_RenderLogicalToWindow() have been renamed SDL_RenderCoordinatesFromWindow() and SDL_RenderCoordinatesToWindow() and take floating point coordinates in both directions.

The viewport, clipping state, and scale for render targets are now persistent and will remain set whenever they are active.

SDL_Vertex has been changed to use floating point colors, in the range of [0..1] for SDR content.

SDL_RenderReadPixels() returns a surface instead of filling in preallocated memory.

The following functions have been renamed:
* SDL_GetRendererOutputSize() => SDL_GetCurrentRenderOutputSize()
* SDL_RenderCopy() => SDL_RenderTexture()
* SDL_RenderCopyEx() => SDL_RenderTextureRotated()
* SDL_RenderCopyExF() => SDL_RenderTextureRotated()
* SDL_RenderCopyF() => SDL_RenderTexture()
* SDL_RenderDrawLine() => SDL_RenderLine()
* SDL_RenderDrawLineF() => SDL_RenderLine()
* SDL_RenderDrawLines() => SDL_RenderLines()
* SDL_RenderDrawLinesF() => SDL_RenderLines()
* SDL_RenderDrawPoint() => SDL_RenderPoint()
* SDL_RenderDrawPointF() => SDL_RenderPoint()
* SDL_RenderDrawPoints() => SDL_RenderPoints()
* SDL_RenderDrawPointsF() => SDL_RenderPoints()
* SDL_RenderDrawRect() => SDL_RenderRect()
* SDL_RenderDrawRectF() => SDL_RenderRect()
* SDL_RenderDrawRects() => SDL_RenderRects()
* SDL_RenderDrawRectsF() => SDL_RenderRects()
* SDL_RenderFillRectF() => SDL_RenderFillRect()
* SDL_RenderFillRectsF() => SDL_RenderFillRects()
* SDL_RenderFlush() => SDL_FlushRenderer()
* SDL_RenderGetClipRect() => SDL_GetRenderClipRect()
* SDL_RenderGetIntegerScale() => SDL_GetRenderIntegerScale()
* SDL_RenderGetLogicalSize() => SDL_GetRenderLogicalPresentation()
* SDL_RenderGetMetalCommandEncoder() => SDL_GetRenderMetalCommandEncoder()
* SDL_RenderGetMetalLayer() => SDL_GetRenderMetalLayer()
* SDL_RenderGetScale() => SDL_GetRenderScale()
* SDL_RenderGetViewport() => SDL_GetRenderViewport()
* SDL_RenderGetWindow() => SDL_GetRenderWindow()
* SDL_RenderIsClipEnabled() => SDL_RenderClipEnabled()
* SDL_RenderLogicalToWindow() => SDL_RenderCoordinatesToWindow()
* SDL_RenderSetClipRect() => SDL_SetRenderClipRect()
* SDL_RenderSetIntegerScale() => SDL_SetRenderIntegerScale()
* SDL_RenderSetLogicalSize() => SDL_SetRenderLogicalPresentation()
* SDL_RenderSetScale() => SDL_SetRenderScale()
* SDL_RenderSetVSync() => SDL_SetRenderVSync()
* SDL_RenderSetViewport() => SDL_SetRenderViewport()
* SDL_RenderWindowToLogical() => SDL_RenderCoordinatesFromWindow()

The following functions have been removed:
* SDL_GL_BindTexture() - use SDL_GetTextureProperties() to get the OpenGL texture ID and bind the texture directly
* SDL_GL_UnbindTexture() - use SDL_GetTextureProperties() to get the OpenGL texture ID and unbind the texture directly
* SDL_GetTextureUserData() - use SDL_GetTextureProperties() instead
* SDL_RenderGetIntegerScale()
* SDL_RenderSetIntegerScale() - this is now explicit with SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
* SDL_RenderTargetSupported() - render targets are always supported
* SDL_SetTextureUserData() - use SDL_GetTextureProperties() instead

The following symbols have been renamed:
* SDL_RendererFlip => SDL_FlipMode
* SDL_ScaleModeBest => SDL_SCALEMODE_BEST
* SDL_ScaleModeLinear => SDL_SCALEMODE_LINEAR
* SDL_ScaleModeNearest => SDL_SCALEMODE_NEAREST

## SDL_rwops.h

The following symbols have been renamed:
* RW_SEEK_CUR => SDL_RW_SEEK_CUR
* RW_SEEK_END => SDL_RW_SEEK_END
* RW_SEEK_SET => SDL_RW_SEEK_SET

SDL_RWread and SDL_RWwrite (and SDL_RWops::read, SDL_RWops::write) have a different function signature in SDL3.

Previously they looked more like stdio:

```c
size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size, size_t maxnum);
size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size, size_t maxnum);
```

But now they look more like POSIX:

```c
size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size);
size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size);
```

Code that used to look like this:
```
size_t custom_read(void *ptr, size_t size, size_t nitems, SDL_RWops *stream)
{
    return SDL_RWread(stream, ptr, size, nitems);
}
```
should be changed to:
```
size_t custom_read(void *ptr, size_t size, size_t nitems, SDL_RWops *stream)
{
    if (size > 0 && nitems > 0) {
        return SDL_RWread(stream, ptr, size * nitems) / size;
    }
    return 0;
}
```

SDL_RWFromFP has been removed from the API, due to issues when the SDL library uses a different C runtime from the application.

You can implement this in your own code easily:
```c
#include <stdio.h>


static Sint64 SDLCALL stdio_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    int stdiowhence;

    switch (whence) {
    case SDL_RW_SEEK_SET:
        stdiowhence = SEEK_SET;
        break;
    case SDL_RW_SEEK_CUR:
        stdiowhence = SEEK_CUR;
        break;
    case SDL_RW_SEEK_END:
        stdiowhence = SEEK_END;
        break;
    default:
        return SDL_SetError("Unknown value for 'whence'");
    }

    if (fseek((FILE *)context->hidden.stdio.fp, (fseek_off_t)offset, stdiowhence) == 0) {
        Sint64 pos = ftell((FILE *)context->hidden.stdio.fp);
        if (pos < 0) {
            return SDL_SetError("Couldn't get stream offset");
        }
        return pos;
    }
    return SDL_Error(SDL_EFSEEK);
}

static size_t SDLCALL stdio_read(SDL_RWops *context, void *ptr, size_t size)
{
    size_t bytes;

    bytes = fread(ptr, 1, size, (FILE *)context->hidden.stdio.fp);
    if (bytes == 0 && ferror((FILE *)context->hidden.stdio.fp)) {
        SDL_Error(SDL_EFREAD);
    }
    return bytes;
}

static size_t SDLCALL stdio_write(SDL_RWops *context, const void *ptr, size_t size)
{
    size_t bytes;

    bytes = fwrite(ptr, 1, size, (FILE *)context->hidden.stdio.fp);
    if (bytes == 0 && ferror((FILE *)context->hidden.stdio.fp)) {
        SDL_Error(SDL_EFWRITE);
    }
    return bytes;
}

static int SDLCALL stdio_close(SDL_RWops *context)
{
    int status = 0;
    if (context->hidden.stdio.autoclose) {
        if (fclose((FILE *)context->hidden.stdio.fp) != 0) {
            status = SDL_Error(SDL_EFWRITE);
        }
    }
    SDL_DestroyRW(context);
    return status;
}

SDL_RWops *SDL_RWFromFP(void *fp, SDL_bool autoclose)
{
    SDL_RWops *rwops = NULL;

    rwops = SDL_CreateRW();
    if (rwops != NULL) {
        rwops->seek = stdio_seek;
        rwops->read = stdio_read;
        rwops->write = stdio_write;
        rwops->close = stdio_close;
        rwops->hidden.stdio.fp = fp;
        rwops->hidden.stdio.autoclose = autoclose;
        rwops->type = SDL_RWOPS_STDFILE;
    }
    return rwops;
}
```

The functions SDL_ReadU8(), SDL_ReadU16LE(), SDL_ReadU16BE(), SDL_ReadU32LE(), SDL_ReadU32BE(), SDL_ReadU64LE(), and SDL_ReadU64BE() now return SDL_TRUE if the read succeeded and SDL_FALSE if it didn't, and store the data in a pointer passed in as a parameter.

The following functions have been renamed:
* SDL_AllocRW() => SDL_CreateRW()
* SDL_FreeRW() => SDL_DestroyRW()
* SDL_ReadBE16() => SDL_ReadU16BE()
* SDL_ReadBE32() => SDL_ReadU32BE()
* SDL_ReadBE64() => SDL_ReadU64BE()
* SDL_ReadLE16() => SDL_ReadU16LE()
* SDL_ReadLE32() => SDL_ReadU32LE()
* SDL_ReadLE64() => SDL_ReadU64LE()
* SDL_WriteBE16() => SDL_WriteU16BE()
* SDL_WriteBE32() => SDL_WriteU32BE()
* SDL_WriteBE64() => SDL_WriteU64BE()
* SDL_WriteLE16() => SDL_WriteU16LE()
* SDL_WriteLE32() => SDL_WriteU32LE()
* SDL_WriteLE64() => SDL_WriteU64LE()

## SDL_sensor.h

SDL_SensorID has changed from Sint32 to Uint32, with an invalid ID being 0.

Rather than iterating over sensors using device index, there is a new function SDL_GetSensors() to get the current list of sensors, and new functions to get information about sensors from their instance ID:
```c
{
    if (SDL_InitSubSystem(SDL_INIT_SENSOR) == 0) {
        int i, num_sensors;
        SDL_SensorID *sensors = SDL_GetSensors(&num_sensors);
        if (sensors) {
            for (i = 0; i < num_sensors; ++i) {
                SDL_Log("Sensor %" SDL_PRIu32 ": %s, type %d, platform type %d\n",
                        sensors[i],
                        SDL_GetSensorInstanceName(sensors[i]),
                        SDL_GetSensorInstanceType(sensors[i]),
                        SDL_GetSensorInstanceNonPortableType(sensors[i]));
            }
            SDL_free(sensors);
        }
        SDL_QuitSubSystem(SDL_INIT_SENSOR);
    }
}
```

Removed SDL_SensorGetDataWithTimestamp(), if you want timestamps for the sensor data, you should use the sensor_timestamp member of SDL_EVENT_SENSOR_UPDATE events.


The following functions have been renamed:
* SDL_SensorClose() => SDL_CloseSensor()
* SDL_SensorFromInstanceID() => SDL_GetSensorFromInstanceID()
* SDL_SensorGetData() => SDL_GetSensorData()
* SDL_SensorGetInstanceID() => SDL_GetSensorInstanceID()
* SDL_SensorGetName() => SDL_GetSensorName()
* SDL_SensorGetNonPortableType() => SDL_GetSensorNonPortableType()
* SDL_SensorGetType() => SDL_GetSensorType()
* SDL_SensorOpen() => SDL_OpenSensor()
* SDL_SensorUpdate() => SDL_UpdateSensors()

The following functions have been removed:
* SDL_LockSensors()
* SDL_NumSensors() - replaced with SDL_GetSensors()
* SDL_SensorGetDeviceInstanceID()
* SDL_SensorGetDeviceName() - replaced with SDL_GetSensorInstanceName()
* SDL_SensorGetDeviceNonPortableType() - replaced with SDL_GetSensorInstanceNonPortableType()
* SDL_SensorGetDeviceType() - replaced with SDL_GetSensorInstanceType()
* SDL_UnlockSensors()

## SDL_shape.h

This header has been removed and a simplified version of this API has been added as SDL_SetWindowShape() in SDL_video.h. See test/testshape.c for an example.

## SDL_stdinc.h

The standard C headers like stdio.h and stdlib.h are no longer included, you should include them directly in your project if you use non-SDL C runtime functions.
M_PI is no longer defined in SDL_stdinc.h, you can use the new symbols SDL_PI_D (double) and SDL_PI_F (float) instead.


The following functions have been renamed:
* SDL_strtokr() => SDL_strtok_r()

The following functions have been removed:
* SDL_memcpy4()

## SDL_surface.h

The userdata member of SDL_Surface has been replaced with a more general properties interface, which can be queried with SDL_GetSurfaceProperties()

Removed unused 'flags' parameter from SDL_ConvertSurface and SDL_ConvertSurfaceFormat.

SDL_CreateRGBSurface() and SDL_CreateRGBSurfaceWithFormat() have been combined into a new function SDL_CreateSurface().
SDL_CreateRGBSurfaceFrom() and SDL_CreateRGBSurfaceWithFormatFrom() have been combined into a new function SDL_CreateSurfaceFrom().

You can implement the old functions in your own code easily:
```c
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    return SDL_CreateSurface(width, height,
            SDL_GetPixelFormatEnumForMasks(depth, Rmask, Gmask, Bmask, Amask));
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, Uint32 format)
{
    return SDL_CreateSurface(width, height, format);
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    return SDL_CreateSurfaceFrom(pixels, width, height, pitch,
            SDL_GetPixelFormatEnumForMasks(depth, Rmask, Gmask, Bmask, Amask));
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int width, int height, int depth, int pitch, Uint32 format)
{
    return SDL_CreateSurfaceFrom(pixels, width, height, pitch, format);
}

```

But if you're migrating your code which uses masks, you probably have a format in mind, possibly one of these:
```c
// Various mask (R, G, B, A) and their corresponding format:
0xFF000000 0x00FF0000 0x0000FF00 0x000000FF => SDL_PIXELFORMAT_RGBA8888
0x00FF0000 0x0000FF00 0x000000FF 0xFF000000 => SDL_PIXELFORMAT_ARGB8888
0x0000FF00 0x00FF0000 0xFF000000 0x000000FF => SDL_PIXELFORMAT_BGRA8888
0x000000FF 0x0000FF00 0x00FF0000 0xFF000000 => SDL_PIXELFORMAT_ABGR8888
0x0000F800 0x000007E0 0x0000001F 0x00000000 => SDL_PIXELFORMAT_RGB565
```

SDL_BlitSurfaceScaled() and SDL_BlitSurfaceUncheckedScaled() now take a scale paramater.

SDL_SoftStretch() now takes a scale paramater.

The following functions have been renamed:
* SDL_FillRect() => SDL_FillSurfaceRect()
* SDL_FillRects() => SDL_FillSurfaceRects()
* SDL_FreeSurface() => SDL_DestroySurface()
* SDL_GetClipRect() => SDL_GetSurfaceClipRect()
* SDL_GetColorKey() => SDL_GetSurfaceColorKey()
* SDL_HasColorKey() => SDL_SurfaceHasColorKey()
* SDL_HasSurfaceRLE() => SDL_SurfaceHasRLE()
* SDL_LowerBlit() => SDL_BlitSurfaceUnchecked()
* SDL_LowerBlitScaled() => SDL_BlitSurfaceUncheckedScaled()
* SDL_SetClipRect() => SDL_SetSurfaceClipRect()
* SDL_SetColorKey() => SDL_SetSurfaceColorKey()
* SDL_UpperBlit() => SDL_BlitSurface()
* SDL_UpperBlitScaled() => SDL_BlitSurfaceScaled()

The following functions have been removed:
* SDL_GetYUVConversionMode()
* SDL_GetYUVConversionModeForResolution()
* SDL_SetYUVConversionMode() - use SDL_SetSurfaceColorspace() to set the surface colorspace and SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER with SDL_CreateTextureWithProperties() to set the texture colorspace. The default colorspace for YUV pixel formats is SDL_COLORSPACE_JPEG.
* SDL_SoftStretchLinear() - use SDL_SoftStretch() with SDL_SCALEMODE_LINEAR

## SDL_system.h

SDL_WindowsMessageHook has changed signatures so the message may be modified and it can block further message processing.

SDL_AndroidGetExternalStorageState() takes the state as an output parameter and returns 0 if the function succeeds or a negative error code if there was an error.

SDL_AndroidRequestPermission is no longer a blocking call; the caller now provides a callback function that fires when a response is available.

The following functions have been removed:
* SDL_RenderGetD3D11Device() - replaced with the "SDL.renderer.d3d11.device" property
* SDL_RenderGetD3D12Device() - replaced with the "SDL.renderer.d3d12.device" property
* SDL_RenderGetD3D9Device() - replaced with the "SDL.renderer.d3d9.device" property

## SDL_syswm.h

This header has been removed.

The Windows and X11 events are now available via callbacks which you can set with SDL_SetWindowsMessageHook() and SDL_SetX11EventHook().

The information previously available in SDL_GetWindowWMInfo() is now available as window properties, e.g.
```c
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);

#if defined(__WIN32__)
    HWND hwnd = NULL;
    if (SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_WINDOWS) {
        hwnd = info.info.win.window;
    }
    if (hwnd) {
        ...
    }
#elif defined(__MACOSX__)
    NSWindow *nswindow = NULL;
    if (SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_COCOA) {
        nswindow = (__bridge NSWindow *)info.info.cocoa.window;
    }
    if (nswindow) {
        ...
    }
#elif defined(SDL_PLATFORM_LINUX)
    if (SDL_GetWindowWMInfo(window, &info)) {
        if (info.subsystem == SDL_SYSWM_X11) {
            Display *xdisplay = info.info.x11.display;
            Window xwindow = info.info.x11.window;
            if (xdisplay && xwindow) {
                ...
            }
        } else if (info.subsystem == SDL_SYSWM_WAYLAND) {
            struct wl_display *display = info.info.wl.display;
            struct wl_surface *surface = info.info.wl.surface;
            if (display && surface) {
                ...
            }
        }
    }
#endif
```
becomes:
```c
#if defined(SDL_PLATFORM_WIN32)
    HWND hwnd = (HWND)SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (hwnd) {
        ...
    }
#elif defined(SDL_PLATFORM_MACOS)
    NSWindow *nswindow = (__bridge NSWindow *)SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    if (nswindow) {
        ...
    }
#elif defined(SDL_PLATFORM_LINUX)
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        Display *xdisplay = (Display *)SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        Window xwindow = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (xdisplay && xwindow) {
            ...
        }
    } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        struct wl_display *display = (struct wl_display *)SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
        struct wl_surface *surface = (struct wl_surface *)SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
        if (display && surface) {
            ...
        }
    }
#endif
```

## SDL_thread.h

The following functions have been renamed:
* SDL_TLSCleanup() => SDL_CleanupTLS()
* SDL_TLSCreate() => SDL_CreateTLS()
* SDL_TLSGet() => SDL_GetTLS()
* SDL_TLSSet() => SDL_SetTLS()
* SDL_ThreadID() => SDL_GetCurrentThreadID()

The following symbols have been renamed:
* SDL_threadID => SDL_ThreadID

## SDL_timer.h

SDL_GetTicks() now returns a 64-bit value. Instead of using the SDL_TICKS_PASSED macro, you can directly compare tick values, e.g.
```c
Uint32 deadline = SDL_GetTicks() + 1000;
...
if (SDL_TICKS_PASSED(SDL_GetTicks(), deadline)) {
    ...
}
```
becomes:
```c
Uint64 deadline = SDL_GetTicks() + 1000
...
if (SDL_GetTicks() >= deadline) {
    ...
}
```

If you were using this macro for other things besides SDL ticks values, you can define it in your own code as:
```c
#define SDL_TICKS_PASSED(A, B)  ((Sint32)((B) - (A)) <= 0)
```

## SDL_touch.h

SDL_GetNumTouchFingers() returns a negative error code if there was an error.

SDL_GetTouchName is replaced with SDL_GetTouchDeviceName(), which takes an SDL_TouchID instead of an index.

SDL_TouchID and SDL_FingerID are now Uint64 with 0 being an invalid value.

The following functions have been removed:
* SDL_GetNumTouchDevices() - replaced with SDL_GetTouchDevices()
* SDL_GetTouchDevice() - replaced with SDL_GetTouchDevices()


## SDL_version.h

SDL_GetRevisionNumber() has been removed from the API, it always returned 0 in SDL 2.0.


The following structures have been renamed:
* SDL_version => SDL_Version

## SDL_video.h

Several video backends have had their names lower-cased ("kmsdrm", "rpi", "android", "psp", "ps2", "vita"). SDL already does a case-insensitive compare for SDL_HINT_VIDEO_DRIVER tests, but if your app is calling SDL_GetVideoDriver() or SDL_GetCurrentVideoDriver() and doing case-sensitive compares on those strings, please update your code.

SDL_VideoInit() and SDL_VideoQuit() have been removed. Instead you can call SDL_InitSubSystem() and SDL_QuitSubSystem() with SDL_INIT_VIDEO, which will properly refcount the subsystems. You can choose a specific video driver using SDL_VIDEO_DRIVER hint.

Rather than iterating over displays using display index, there is a new function SDL_GetDisplays() to get the current list of displays, and functions which used to take a display index now take SDL_DisplayID, with an invalid ID being 0.
```c
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        int i, num_displays = 0;
        SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
        if (displays) {
            for (i = 0; i < num_displays; ++i) {
                SDL_DisplayID instance_id = displays[i];
                const char *name = SDL_GetDisplayName(instance_id);

                SDL_Log("Display %" SDL_PRIu32 ": %s\n", instance_id, name ? name : "Unknown");
            }
            SDL_free(displays);
        }
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}
```

SDL_CreateWindow() has been simplified and no longer takes a window position. You can use SDL_CreateWindowWithProperties() if you need to set the window position when creating it, e.g.
```c
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
    SDL_SetNumberProperty(props, "flags", flags);
    pWindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (window) {
        ...
    }
```

The SDL_WINDOWPOS_UNDEFINED_DISPLAY() and SDL_WINDOWPOS_CENTERED_DISPLAY() macros take a display ID instead of display index. The display ID 0 has a special meaning in this case, and is used to indicate the primary display.

The SDL_WINDOW_SHOWN flag has been removed. Windows are shown by default and can be created hidden by using the SDL_WINDOW_HIDDEN flag.

The SDL_WINDOW_SKIP_TASKBAR flag has been replaced by the SDL_WINDOW_UTILITY flag, which has the same functionality.

SDL_DisplayMode now includes the pixel density which can be greater than 1.0 for display modes that have a higher pixel size than the mode size. You should use SDL_GetWindowSizeInPixels() to get the actual pixel size of the window back buffer.

The refresh rate in SDL_DisplayMode is now a float.

Rather than iterating over display modes using an index, there is a new function SDL_GetFullscreenDisplayModes() to get the list of available fullscreen modes on a display.
```c
{
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    int num_modes = 0;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display, &num_modes);
    if (modes) {
        for (i = 0; i < num_modes; ++i) {
            SDL_DisplayMode *mode = modes[i];
            SDL_Log("Display %" SDL_PRIu32 " mode %d: %dx%d@%gx %gHz\n",
                    display, i, mode->w, mode->h, mode->pixel_density, mode->refresh_rate);
        }
        SDL_free(modes);
    }
}
```

SDL_GetDesktopDisplayMode() and SDL_GetCurrentDisplayMode() return pointers to display modes rather than filling in application memory.

Windows now have an explicit fullscreen mode that is set, using SDL_SetWindowFullscreenMode(). The fullscreen mode for a window can be queried with SDL_GetWindowFullscreenMode(), which returns a pointer to the mode, or NULL if the window will be fullscreen desktop. SDL_SetWindowFullscreen() just takes a boolean value, setting the correct fullscreen state based on the selected mode.

SDL_WINDOW_FULLSCREEN_DESKTOP has been removed, and you can call SDL_GetWindowFullscreenMode() to see whether an exclusive fullscreen mode will be used or the borderless fullscreen desktop mode will be used when the window is fullscreen.

SDL_SetWindowBrightness and SDL_SetWindowGammaRamp have been removed from the API, because they interact poorly with modern operating systems and aren't able to limit their effects to the SDL window.

Programs which have access to shaders can implement more robust versions of those functions using custom shader code rendered as a post-process effect.

Removed SDL_GL_CONTEXT_EGL from OpenGL configuration attributes. You can instead use `SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);`

SDL_GL_GetProcAddress() and SDL_EGL_GetProcAddress() now return `SDL_FunctionPointer` instead of `void *`, and should be cast to the appropriate function type. You can define SDL_FUNCTION_POINTER_IS_VOID_POINTER in your project to restore the previous behavior.

SDL_GL_SwapWindow() returns 0 if the function succeeds or a negative error code if there was an error.

SDL_GL_GetSwapInterval() takes the interval as an output parameter and returns 0 if the function succeeds or a negative error code if there was an error.

SDL_GL_GetDrawableSize() has been removed. SDL_GetWindowSizeInPixels() can be used in its place.

The SDL_WINDOW_TOOLTIP and SDL_WINDOW_POPUP_MENU window flags are now supported on Windows, Mac (Cocoa), X11, and Wayland. Creating windows with these flags must happen via the `SDL_CreatePopupWindow()` function. This function requires passing in the handle to a valid parent window for the popup, and the popup window is positioned relative to the parent.

The following functions have been renamed:
* SDL_GetClosestDisplayMode() => SDL_GetClosestFullscreenDisplayMode()
* SDL_GetDisplayOrientation() => SDL_GetCurrentDisplayOrientation()
* SDL_GetPointDisplayIndex() => SDL_GetDisplayForPoint()
* SDL_GetRectDisplayIndex() => SDL_GetDisplayForRect()
* SDL_GetWindowDisplayIndex() => SDL_GetDisplayForWindow()
* SDL_GetWindowDisplayMode() => SDL_GetWindowFullscreenMode()
* SDL_HasWindowSurface() => SDL_WindowHasSurface()
* SDL_IsScreenSaverEnabled() => SDL_ScreenSaverEnabled()
* SDL_SetWindowDisplayMode() => SDL_SetWindowFullscreenMode()

The following functions have been removed:
* SDL_GetClosestFullscreenDisplayMode()
* SDL_GetDisplayDPI() - not reliable across platforms, approximately replaced by multiplying `display_scale` in the structure returned by SDL_GetDesktopDisplayMode() times 160 on iPhone and Android, and 96 on other platforms.
* SDL_GetDisplayMode()
* SDL_GetNumDisplayModes() - replaced with SDL_GetFullscreenDisplayModes()
* SDL_GetNumVideoDisplays() - replaced with SDL_GetDisplays()
* SDL_GetWindowData() - use SDL_GetWindowProperties() instead
* SDL_SetWindowData() - use SDL_GetWindowProperties() instead
* SDL_CreateWindowFrom() - use SDL_CreateWindowWithProperties() with the properties that allow you to wrap an existing window

The SDL_Window id type is named SDL_WindowID
The SDL_WindowFlags enum should be replaced with Uint32

The following symbols have been renamed:
* SDL_WINDOW_ALLOW_HIGHDPI => SDL_WINDOW_HIGH_PIXEL_DENSITY
* SDL_WINDOW_INPUT_GRABBED => SDL_WINDOW_MOUSE_GRABBED

The following window operations are now considered to be asynchronous requests and should not be assumed to succeed unless
a corresponding event has been received:
* SDL_SetWindowSize() (SDL_EVENT_WINDOW_RESIZED)
* SDL_SetWindowPosition() (SDL_EVENT_WINDOW_MOVED)
* SDL_MinimizeWindow() (SDL_EVENT_WINDOW_MINIMIZED)
* SDL_MaximizeWindow() (SDL_EVENT_WINDOW_MAXIMIZED)
* SDL_RestoreWindow() (SDL_EVENT_WINDOW_RESTORED)
* SDL_SetWindowFullscreen() (SDL_EVENT_WINDOW_ENTER_FULLSCREEN / SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)

If it is required that operations be applied immediately after one of the preceeding calls, the `SDL_SyncWindow()` function
will attempt to wait until all pending window operations have completed. Be aware that this function can potentially block for
long periods of time, as it may have to wait for window animations to complete. Also note that windowing systems can deny or
not precisely obey these requests (e.g. windows may not be allowed to be larger than the usable desktop space or placed
offscreen), so a corresponding event may never arrive or not contain the expected values.

## SDL_vulkan.h

SDL_Vulkan_GetInstanceExtensions() no longer takes a window parameter, and no longer makes the app allocate query/allocate space for the result, instead returning a static const internal string.

SDL_Vulkan_GetVkGetInstanceProcAddr() now returns `SDL_FunctionPointer` instead of `void *`, and should be cast to PFN_vkGetInstanceProcAddr.

SDL_Vulkan_CreateSurface() now takes a VkAllocationCallbacks pointer as its third parameter. If you don't have an allocator to supply, pass a NULL here to use the system default allocator (SDL2 always used the system default allocator here).

SDL_Vulkan_GetDrawableSize() has been removed. SDL_GetWindowSizeInPixels() can be used in its place.
