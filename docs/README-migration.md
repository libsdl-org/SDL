# Migrating to SDL 3.0

This guide provides useful information for migrating applications from SDL 2.0 to SDL 3.0.

Details on API changes are organized by SDL 2.0 header below.

The file with your main() function should include <SDL3/SDL_main.h>, as that is no longer included in SDL.h.

Functions that previously returned a negative error code now return bool.

Code that used to look like this:
```c
    if (SDL_Function() < 0 || SDL_Function() == -1) {
        /* Failure... */
    }
```
or
```c
    if (SDL_Function() == 0) {
        /* Success... */
    }
```
or
```c
    if (!SDL_Function()) {
        /* Success... */
    }
```
should be changed to:
```c
    if (SDL_Function()) {
        /* Success... */
    } else {
        /* Failure... */
    }
```
This only applies to camel case functions, e.g. `SDL_[A-Z]*`. Lower case functions like SDL_strcmp() and SDL_memcmp() are unchanged, matching their C runtime counterpart.

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
```cmake
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)
target_link_libraries(mygame PRIVATE SDL3::SDL3)
```

Autotools users should use this snippet to include SDL support in their project:
```m4
PKG_CHECK_MODULES([SDL3], [sdl3])
```
and then add `$SDL3_CFLAGS` to their project `CFLAGS` and `$SDL3_LIBS` to their project `LDFLAGS`.

Makefile users can use this snippet to include SDL support in their project:
```make
CFLAGS += $(shell pkg-config sdl3 --cflags)
LDFLAGS += $(shell pkg-config sdl3 --libs)
```

The SDL3test library has been renamed SDL3_test.

The SDLmain library has been removed, it's been entirely replaced by SDL_main.h.

The vi format comments have been removed from source code. Vim users can use the [editorconfig plugin](https://github.com/editorconfig/editorconfig-vim) to automatically set tab spacing for the SDL coding style.

Installed SDL CMake configuration files no longer define `SDL3_PREFIX`, `SDL3_EXEC_PREFIX`, `SDL3_INCLUDE_DIR`, `SDL3_INCLUDE_DIRS`, `SDL3_BINDIR` or `SDL3_LIBDIR`. Users are expected to use [CMake generator expressions](https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html#target-dependent-expressions) with `SDL3::SDL3`, `SDL3::SDL3-shared`, `SDL3::SDL3-static` or `SDL3::Headers`.  By no longer defining these CMake variables, using a system SDL3 or using a vendoring SDL3 behave in the same way.

## SDL_atomic.h

The following structures have been renamed:
- SDL_atomic_t => SDL_AtomicInt

The following functions have been renamed:
* SDL_AtomicAdd() => SDL_AddAtomicInt()
* SDL_AtomicCAS() => SDL_CompareAndSwapAtomicInt()
* SDL_AtomicCASPtr() => SDL_CompareAndSwapAtomicPointer()
* SDL_AtomicGet() => SDL_GetAtomicInt()
* SDL_AtomicGetPtr() => SDL_GetAtomicPointer()
* SDL_AtomicLock() => SDL_LockSpinlock()
* SDL_AtomicSet() => SDL_SetAtomicInt()
* SDL_AtomicSetPtr() => SDL_SetAtomicPointer()
* SDL_AtomicTryLock() => SDL_TryLockSpinlock()
* SDL_AtomicUnlock() => SDL_UnlockSpinlock()

## SDL_audio.h

The audio subsystem in SDL3 is dramatically different than SDL2. The primary way to play audio is no longer an audio callback; instead you bind SDL_AudioStreams to devices; however, there is still a callback method available if needed.

The SDL 1.2 audio compatibility API has also been removed, as it was a simplified version of the audio callback interface.

SDL3 will not implicitly initialize the audio subsystem on your behalf if you open a device without doing so. Please explicitly call SDL_Init(SDL_INIT_AUDIO) at some point.

SDL2 referred to audio devices that record sound as "capture" devices, and ones that play sound to speakers as "output" devices. In SDL3, we've changed this terminology to be "recording" devices and "playback" devices, which we hope is more clear.

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
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, MyNewAudioCallback, &my_audio_callback_user_data);
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
```

If you used SDL_QueueAudio instead of a callback in SDL2, this is also straightforward.

```c
    /* ...somewhere near startup... */
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 44100 };
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));

    /* ...in your main loop... */
    /* calculate a little more audio into `buf`, add it to `stream` */
    SDL_PutAudioStreamData(stream, buf, buflen);

```

...these same migration examples apply to audio recording, just using SDL_GetAudioStreamData instead of SDL_PutAudioStreamData.

SDL_AudioInit() and SDL_AudioQuit() have been removed. Instead you can call SDL_InitSubSystem() and SDL_QuitSubSystem() with SDL_INIT_AUDIO, which will properly refcount the subsystems. You can choose a specific audio driver using SDL_AUDIO_DRIVER hint.

The `SDL_AUDIO_ALLOW_*` symbols have been removed; now one may request the format they desire from the audio device, but ultimately SDL_AudioStream will manage the difference. One can use SDL_GetAudioDeviceFormat() to see what the final format is, if any "allowed" changes should be accomodated by the app.

SDL_AudioDeviceID now represents both an open audio device's handle (a "logical" device) and the instance ID that the hardware owns as long as it exists on the system (a "physical" device). The separation between device instances and device indexes is gone, and logical and physical devices are almost entirely interchangeable at the API level.

Devices are opened by physical device instance ID, and a new logical instance ID is generated by the open operation; This allows any device to be opened multiple times, possibly by unrelated pieces of code. SDL will manage the logical devices to provide a single stream of audio to the physical device behind the scenes.

Devices are not opened by an arbitrary string name anymore, but by device instance ID (or magic numbers to request a reasonable default, like a NULL string in SDL2). In SDL2, the string was used to open both a standard list of system devices, but also allowed for arbitrary devices, such as hostnames of network sound servers. In SDL3, many of the backends that supported arbitrary device names are obsolete and have been removed; of those that remain, arbitrary devices will be opened with a default device ID and an SDL_hint, so specific end-users can set an environment variable to fit their needs and apps don't have to concern themselves with it.

Many functions that would accept a device index and an `iscapture` parameter now just take an SDL_AudioDeviceID, as they are unique across all devices, instead of separate indices into playback and recording device lists.

Rather than iterating over audio devices using a device index, there are new functions, SDL_GetAudioPlaybackDevices() and SDL_GetAudioRecordingDevices(), to get the current list of devices, and new functions to get information about devices from their instance ID:

```c
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        int i, num_devices;
        SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices(&num_devices);
        if (devices) {
            for (i = 0; i < num_devices; ++i) {
                SDL_AudioDeviceID instance_id = devices[i];
                SDL_Log("AudioDevice %" SDL_PRIu32 ": %s\n", instance_id, SDL_GetAudioDeviceName(instance_id));
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

SDL_GetDefaultAudioInfo() is removed; SDL_GetAudioDeviceFormat() with SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK or SDL_AUDIO_DEVICE_DEFAULT_RECORDING. There is no replacement for querying the default device name; the string is no longer used to open devices, and SDL3 will migrate between physical devices on the fly if the system default changes, so if you must show this to the user, a generic name like "System default" is recommended.

SDL_MixAudioFormat() and SDL_MIX_MAXVOLUME have been removed in favour of SDL_MixAudio(), which now takes the audio format, and a float volume between 0.0 and 1.0.

SDL_FreeWAV has been removed and calls can be replaced with SDL_free.

SDL_LoadWAV() is a proper function now and no longer a macro (but offers the same functionality otherwise).

SDL_LoadWAV_IO() and SDL_LoadWAV() return an bool now, like most of SDL. They no longer return a pointer to an SDL_AudioSpec.

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
    if (!SDL_ConvertAudioSamples(&src_spec, src_data, src_len, &dst_spec, &dst_data, &dst_len)) {
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
* SDL_AudioStreamClear() => SDL_ClearAudioStream(), returns bool
* SDL_AudioStreamFlush() => SDL_FlushAudioStream(), returns bool
* SDL_AudioStreamGet() => SDL_GetAudioStreamData()
* SDL_AudioStreamPut() => SDL_PutAudioStreamData(), returns bool
* SDL_FreeAudioStream() => SDL_DestroyAudioStream()
* SDL_LoadWAV_RW() => SDL_LoadWAV_IO(), returns bool
* SDL_MixAudioFormat() => SDL_MixAudio(), returns bool
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

The following symbols have been removed:
* SDL_MIX_MAXVOLUME - mixer volume is now a float between 0.0 and 1.0

## SDL_cpuinfo.h

The intrinsics headers (mmintrin.h, etc.) have been moved to `<SDL3/SDL_intrin.h>` and are no longer automatically included in SDL.h.

SDL_Has3DNow() has been removed; there is no replacement.

SDL_HasRDTSC() has been removed; there is no replacement. Don't use the RDTSC opcode in modern times, use SDL_GetPerformanceCounter and SDL_GetPerformanceFrequency instead.

SDL_SIMDAlloc(), SDL_SIMDRealloc(), and SDL_SIMDFree() have been removed. You can use SDL_aligned_alloc() and SDL_aligned_free() with SDL_GetSIMDAlignment() to get the same functionality.

The following functions have been renamed:
* SDL_GetCPUCount() => SDL_GetNumLogicalCPUCores()
* SDL_SIMDGetAlignment() => SDL_GetSIMDAlignment()

## SDL_endian.h

The following functions have been renamed:
* SDL_SwapBE16() => SDL_Swap16BE()
* SDL_SwapBE32() => SDL_Swap32BE()
* SDL_SwapBE64() => SDL_Swap64BE()
* SDL_SwapLE16() => SDL_Swap16LE()
* SDL_SwapLE32() => SDL_Swap32LE()
* SDL_SwapLE64() => SDL_Swap64LE()

## SDL_error.h

The following functions have been removed:
* SDL_GetErrorMsg() - Can be implemented as `SDL_strlcpy(errstr, SDL_GetError(), maxlen);`

## SDL_events.h

SDL_PRESSED and SDL_RELEASED have been removed. For the most part you can replace uses of these with true and false respectively. Events which had a field `state` to represent these values have had those fields changed to bool `down`, e.g. `event.key.state` is now `event.key.down`.

The timestamp member of the SDL_Event structure now represents nanoseconds, and is populated with SDL_GetTicksNS()

The timestamp_us member of the sensor events has been renamed sensor_timestamp and now represents nanoseconds. This value is filled in from the hardware, if available, and may not be synchronized with values returned from SDL_GetTicksNS().

You should set the event.common.timestamp field before passing an event to SDL_PushEvent(). If the timestamp is 0 it will be filled in with SDL_GetTicksNS().

Event memory is now managed by SDL, so you should not free the data in SDL_EVENT_DROP_FILE, and if you want to hold onto the text in SDL_EVENT_TEXT_EDITING and SDL_EVENT_TEXT_INPUT events, you should make a copy of it. SDL_TEXTINPUTEVENT_TEXT_SIZE is no longer necessary and has been removed.

Mouse events use floating point values for mouse coordinates and relative motion values. You can get sub-pixel motion depending on the platform and display scaling.

The SDL_DISPLAYEVENT_* events have been moved to top level events, and SDL_DISPLAYEVENT has been removed. In general, handling this change just means checking for the individual events instead of first checking for SDL_DISPLAYEVENT and then checking for display events. You can compare the event >= SDL_EVENT_DISPLAY_FIRST and <= SDL_EVENT_DISPLAY_LAST if you need to see whether it's a display event.

The SDL_WINDOWEVENT_* events have been moved to top level events, and SDL_WINDOWEVENT has been removed. In general, handling this change just means checking for the individual events instead of first checking for SDL_WINDOWEVENT and then checking for window events. You can compare the event >= SDL_EVENT_WINDOW_FIRST and <= SDL_EVENT_WINDOW_LAST if you need to see whether it's a window event.

The SDL_EVENT_WINDOW_RESIZED event is always sent, even in response to SDL_SetWindowSize().

The SDL_EVENT_WINDOW_SIZE_CHANGED event has been removed, and you can use SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED to detect window backbuffer size changes.

The keysym field of key events has been removed to remove one level of indirection, and `sym` has been renamed `key`.

Code that used to look like this:
```c
    SDL_Event event;
    SDL_Keycode key = event.key.keysym.sym;
    SDL_Keymod mod = event.key.keysym.mod;
```
should be changed to:
```c
    SDL_Event event;
    SDL_Keycode key = event.key.key;
    SDL_Keymod mod = event.key.mod;
```

The keycode in key events is affected by modifiers by default. e.g. pressing the A key would generate the keycode `SDLK_A`, or 'a', and pressing it while holding the shift key would generate the keycode `SDLK_A`, or 'A'. This behavior can be customized with `SDL_HINT_KEYCODE_OPTIONS`.

The gamepad event structures caxis, cbutton, cdevice, ctouchpad, and csensor have been renamed gaxis, gbutton, gdevice, gtouchpad, and gsensor.

The mouseX and mouseY fields of SDL_MouseWheelEvent have been renamed mouse_x and mouse_y.

The touchId and fingerId fields of SDL_TouchFingerEvent have been renamed touchID and fingerID.

The level field of SDL_JoyBatteryEvent has been split into state and percent.

The iscapture field of SDL_AudioDeviceEvent has been renamed recording.

SDL_QUERY, SDL_IGNORE, SDL_ENABLE, and SDL_DISABLE have been removed. You can use the functions SDL_SetEventEnabled() and SDL_EventEnabled() to set and query event processing state.

SDL_AddEventWatch() now returns SDL_FALSE_ if it fails because it ran out of memory and couldn't add the event watch callback.

SDL_RegisterEvents() now returns 0 if it couldn't allocate any user events.

SDL_EventFilter functions now return bool.

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
* SDL_JOYBALLMOTION => SDL_EVENT_JOYSTICK_BALL_MOTION
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
* SDL_TEXTEDITING => SDL_EVENT_TEXT_EDITING
* SDL_TEXTEDITING_EXT => SDL_EVENT_TEXT_EDITING_EXT
* SDL_TEXTINPUT => SDL_EVENT_TEXT_INPUT
* SDL_USEREVENT => SDL_EVENT_USER

The following symbols have been removed:
* SDL_DROPEVENT_DATA_SIZE - drop event data is dynamically allocated
* SDL_SYSWMEVENT - you can use SDL_SetWindowsMessageHook() and SDL_SetX11EventHook() to watch and modify system events before SDL sees them.
* SDL_TEXTEDITINGEVENT_TEXT_SIZE - text editing event data is dynamically allocated

The following structures have been renamed:
* SDL_ControllerAxisEvent => SDL_GamepadAxisEvent
* SDL_ControllerButtonEvent => SDL_GamepadButtonEvent
* SDL_ControllerDeviceEvent => SDL_GamepadDeviceEvent
* SDL_ControllerSensorEvent => SDL_GamepadSensorEvent
* SDL_ControllerTouchpadEvent => SDL_GamepadTouchpadEvent

The following functions have been removed:
* SDL_EventState() - replaced with SDL_SetEventEnabled()
* SDL_GetEventState() - replaced with SDL_EventEnabled()

The following enums have been renamed:
* SDL_eventaction => SDL_EventAction

The following functions have been renamed:
* SDL_DelEventWatch() => SDL_RemoveEventWatch()

## SDL_gamecontroller.h

SDL_gamecontroller.h has been renamed SDL_gamepad.h, and all APIs have been renamed to match.

The SDL_EVENT_GAMEPAD_ADDED event now provides the joystick instance ID in the which member of the cdevice event structure.

The functions SDL_GetGamepads(), SDL_GetGamepadNameForID(), SDL_GetGamepadPathForID(), SDL_GetGamepadPlayerIndexForID(), SDL_GetGamepadGUIDForID(), SDL_GetGamepadVendorForID(), SDL_GetGamepadProductForID(), SDL_GetGamepadProductVersionForID(), and SDL_GetGamepadTypeForID() have been added to directly query the list of available gamepads.

The gamepad face buttons have been renamed from A/B/X/Y to North/South/East/West to indicate that they are positional rather than hardware-specific. You can use SDL_GetGamepadButtonLabel() to get the labels for the face buttons, e.g. A/B/X/Y or Cross/Circle/Square/Triangle. The hint SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS is ignored, and mappings that use this hint are translated correctly into positional buttons. Applications should provide a way for users to swap between South/East as their accept/cancel buttons, as this varies based on region and muscle memory. You can use an approach similar to the following to handle this:

```c
#define CONFIRM_BUTTON SDL_GAMEPAD_BUTTON_SOUTH
#define CANCEL_BUTTON SDL_GAMEPAD_BUTTON_EAST

bool flipped_buttons;

void InitMappedButtons(SDL_Gamepad *gamepad)
{
    if (!GetFlippedButtonSetting(&flipped_buttons)) {
        if (SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B) {
            flipped_buttons = true;
        } else {
            flipped_buttons = false;
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
bool SDL_IsJoystickAmazonLunaController(Uint16 vendor_id, Uint16 product_id)
{
    return ((vendor_id == 0x1949 && product_id == 0x0419) ||
            (vendor_id == 0x0171 && product_id == 0x0419));
}
```

SDL_CONTROLLER_TYPE_GOOGLE_STADIA has been removed, and can be replaced with this code:
```c
bool SDL_IsJoystickGoogleStadiaController(Uint16 vendor_id, Uint16 product_id)
{
    return (vendor_id == 0x18d1 && product_id == 0x9400);
}
```

SDL_CONTROLLER_TYPE_NVIDIA_SHIELD has been removed, and can be replaced with this code:
```c
bool SDL_IsJoystickNVIDIASHIELDController(Uint16 vendor_id, Uint16 product_id)
{
    return (vendor_id == 0x0955 && (product_id == 0x7210 || product_id == 0x7214));
}
```

The inputType and outputType fields of SDL_GamepadBinding have been renamed input_type and output_type.

SDL_GetGamepadTouchpadFinger() takes a pointer to bool for the finger state instead of a pointer to Uint8.

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
* SDL_GameControllerAddMappingsFromRW() => SDL_AddGamepadMappingsFromIO()
* SDL_GameControllerClose() => SDL_CloseGamepad()
* SDL_GameControllerFromInstanceID() => SDL_GetGamepadFromID()
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
* SDL_GameControllerGetSensorData() => SDL_GetGamepadSensorData(), returns bool
* SDL_GameControllerGetSensorDataRate() => SDL_GetGamepadSensorDataRate()
* SDL_GameControllerGetSerial() => SDL_GetGamepadSerial()
* SDL_GameControllerGetSteamHandle() => SDL_GetGamepadSteamHandle()
* SDL_GameControllerGetStringForAxis() => SDL_GetGamepadStringForAxis()
* SDL_GameControllerGetStringForButton() => SDL_GetGamepadStringForButton()
* SDL_GameControllerGetTouchpadFinger() => SDL_GetGamepadTouchpadFinger(), returns bool
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
* SDL_GameControllerRumble() => SDL_RumbleGamepad(), returns bool
* SDL_GameControllerRumbleTriggers() => SDL_RumbleGamepadTriggers(), returns bool
* SDL_GameControllerSendEffect() => SDL_SendGamepadEffect(), returns bool
* SDL_GameControllerSetLED() => SDL_SetGamepadLED(), returns bool
* SDL_GameControllerSetPlayerIndex() => SDL_SetGamepadPlayerIndex(), returns bool
* SDL_GameControllerSetSensorEnabled() => SDL_SetGamepadSensorEnabled(), returns bool
* SDL_GameControllerUpdate() => SDL_UpdateGamepads()
* SDL_IsGameController() => SDL_IsGamepad()

The following functions have been removed:
* SDL_GameControllerEventState() - replaced with SDL_SetGamepadEventsEnabled() and SDL_GamepadEventsEnabled()
* SDL_GameControllerGetBindForAxis() - replaced with SDL_GetGamepadBindings()
* SDL_GameControllerGetBindForButton() - replaced with SDL_GetGamepadBindings()
* SDL_GameControllerHasLED() - replaced with SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN
* SDL_GameControllerHasRumble() - replaced with SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN
* SDL_GameControllerHasRumbleTriggers() - replaced with SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN
* SDL_GameControllerMappingForDeviceIndex() - replaced with SDL_GetGamepadMappingForID()
* SDL_GameControllerMappingForIndex() - replaced with SDL_GetGamepadMappings()
* SDL_GameControllerNameForIndex() - replaced with SDL_GetGamepadNameForID()
* SDL_GameControllerNumMappings() - replaced with SDL_GetGamepadMappings()
* SDL_GameControllerPathForIndex() - replaced with SDL_GetGamepadPathForID()
* SDL_GameControllerTypeForIndex() - replaced with SDL_GetGamepadTypeForID()

The following symbols have been renamed:
* SDL_CONTROLLER_AXIS_INVALID => SDL_GAMEPAD_AXIS_INVALID
* SDL_CONTROLLER_AXIS_LEFTX => SDL_GAMEPAD_AXIS_LEFTX
* SDL_CONTROLLER_AXIS_LEFTY => SDL_GAMEPAD_AXIS_LEFTY
* SDL_CONTROLLER_AXIS_MAX => SDL_GAMEPAD_AXIS_COUNT
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
* SDL_CONTROLLER_BUTTON_MAX => SDL_GAMEPAD_BUTTON_COUNT
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

## SDL_guid.h

SDL_GUIDToString() returns a const pointer to the string representation of a GUID.

The following functions have been renamed:
* SDL_GUIDFromString() => SDL_StringToGUID()

## SDL_haptic.h

Gamepads with simple rumble capability no longer show up in the SDL haptics interface, instead you should use SDL_RumbleGamepad().

Rather than iterating over haptic devices using device index, there is a new function SDL_GetHaptics() to get the current list of haptic devices, and new functions to get information about haptic devices from their instance ID:
```c
{
    if (SDL_InitSubSystem(SDL_INIT_HAPTIC)) {
        int i, num_haptics;
        SDL_HapticID *haptics = SDL_GetHaptics(&num_haptics);
        if (haptics) {
            for (i = 0; i < num_haptics; ++i) {
                SDL_HapticID instance_id = haptics[i];
                SDL_Log("Haptic %" SDL_PRIu32 ": %s\n", instance_id, SDL_GetHapticNameForID(instance_id));
            }
            SDL_free(haptics);
        }
        SDL_QuitSubSystem(SDL_INIT_HAPTIC);
    }
}
```

SDL_GetHapticEffectStatus() now returns bool instead of an int result. You should call SDL_GetHapticFeatures() to make sure effect status is supported before calling this function.

The following functions have been renamed:
* SDL_HapticClose() => SDL_CloseHaptic()
* SDL_HapticDestroyEffect() => SDL_DestroyHapticEffect()
* SDL_HapticGetEffectStatus() => SDL_GetHapticEffectStatus(), returns bool
* SDL_HapticNewEffect() => SDL_CreateHapticEffect()
* SDL_HapticNumAxes() => SDL_GetNumHapticAxes()
* SDL_HapticNumEffects() => SDL_GetMaxHapticEffects()
* SDL_HapticNumEffectsPlaying() => SDL_GetMaxHapticEffectsPlaying()
* SDL_HapticOpen() => SDL_OpenHaptic()
* SDL_HapticOpenFromJoystick() => SDL_OpenHapticFromJoystick()
* SDL_HapticOpenFromMouse() => SDL_OpenHapticFromMouse()
* SDL_HapticPause() => SDL_PauseHaptic(), returns bool
* SDL_HapticQuery() => SDL_GetHapticFeatures()
* SDL_HapticRumbleInit() => SDL_InitHapticRumble(), returns bool
* SDL_HapticRumblePlay() => SDL_PlayHapticRumble(), returns bool
* SDL_HapticRumbleStop() => SDL_StopHapticRumble(), returns bool
* SDL_HapticRunEffect() => SDL_RunHapticEffect(), returns bool
* SDL_HapticSetAutocenter() => SDL_SetHapticAutocenter(), returns bool
* SDL_HapticSetGain() => SDL_SetHapticGain(), returns bool
* SDL_HapticStopAll() => SDL_StopHapticEffects(), returns bool
* SDL_HapticStopEffect() => SDL_StopHapticEffect(), returns bool
* SDL_HapticUnpause() => SDL_ResumeHaptic(), returns bool
* SDL_HapticUpdateEffect() => SDL_UpdateHapticEffect(), returns bool
* SDL_JoystickIsHaptic() => SDL_IsJoystickHaptic()
* SDL_MouseIsHaptic() => SDL_IsMouseHaptic()

The following functions have been removed:
* SDL_HapticIndex() - replaced with SDL_GetHapticID()
* SDL_HapticName() - replaced with SDL_GetHapticNameForID()
* SDL_HapticOpened() - replaced with SDL_GetHapticFromID()
* SDL_NumHaptics() - replaced with SDL_GetHaptics()

## SDL_hints.h

Calling SDL_GetHint() with the name of the hint being changed from within a hint callback will now return the new value rather than the old value. The old value is still passed as a parameter to the hint callback.

The environment variables SDL_VIDEODRIVER and SDL_AUDIODRIVER have been renamed to SDL_VIDEO_DRIVER and SDL_AUDIO_DRIVER.

The environment variables SDL_VIDEO_X11_WMCLASS and SDL_VIDEO_WAYLAND_WMCLASS have been removed and replaced by either using the appindentifier param to SDL_SetAppMetadata() or setting SDL_PROP_APP_METADATA_IDENTIFIER_STRING with SDL_SetAppMetadataProperty()

The environment variable AUDIODEV is used exclusively to specify the audio device for the OSS and NetBSD audio drivers. Its use in the ALSA driver has been replaced with the hint SDL_HINT_AUDIO_ALSA_DEFAULT_DEVICE and in the sndio driver with the environment variable AUDIODEVICE.

The following hints have been renamed:
* SDL_HINT_VIDEODRIVER => SDL_HINT_VIDEO_DRIVER
* SDL_HINT_AUDIODRIVER => SDL_HINT_AUDIO_DRIVER
* SDL_HINT_ALLOW_TOPMOST => SDL_HINT_WINDOW_ALLOW_TOPMOST
* SDL_HINT_DIRECTINPUT_ENABLED => SDL_HINT_JOYSTICK_DIRECTINPUT
* SDL_HINT_GDK_TEXTINPUT_DEFAULT => SDL_HINT_GDK_TEXTINPUT_DEFAULT_TEXT
* SDL_HINT_JOYSTICK_GAMECUBE_RUMBLE_BRAKE => SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE_RUMBLE_BRAKE
* SDL_HINT_LINUX_DIGITAL_HATS => SDL_HINT_JOYSTICK_LINUX_DIGITAL_HATS
* SDL_HINT_LINUX_HAT_DEADZONES => SDL_HINT_JOYSTICK_LINUX_HAT_DEADZONES
* SDL_HINT_LINUX_JOYSTICK_CLASSIC => SDL_HINT_JOYSTICK_LINUX_CLASSIC
* SDL_HINT_LINUX_JOYSTICK_DEADZONES => SDL_HINT_JOYSTICK_LINUX_DEADZONES
* SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP => SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE

The following hints have been removed:
* SDL_HINT_ACCELEROMETER_AS_JOYSTICK
* SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO - the audio will be paused when the application is paused, and SDL_HINT_ANDROID_BLOCK_ON_PAUSE can be used to control that
* SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS - gamepad buttons are always positional
* SDL_HINT_GRAB_KEYBOARD - use SDL_SetWindowKeyboardGrab() instead
* SDL_HINT_IDLE_TIMER_DISABLED - use SDL_DisableScreenSaver() instead
* SDL_HINT_IME_INTERNAL_EDITING - replaced with SDL_HINT_IME_IMPLEMENTED_UI
* SDL_HINT_IME_SHOW_UI - replaced with SDL_HINT_IME_IMPLEMENTED_UI
* SDL_HINT_IME_SUPPORT_EXTENDED_TEXT - the normal text editing event has extended text
* SDL_HINT_MOUSE_RELATIVE_SCALING - mouse coordinates are no longer automatically scaled by the SDL renderer
* SDL_HINT_PS2_DYNAMIC_VSYNC - use SDL_SetRenderVSync(renderer, -1) instead
* SDL_HINT_RENDER_BATCHING - Render batching is always enabled, apps should call SDL_FlushRenderer() before calling into a lower-level graphics API.
* SDL_HINT_RENDER_LOGICAL_SIZE_MODE - the logical size mode is explicitly set with SDL_SetRenderLogicalPresentation()
* SDL_HINT_RENDER_OPENGL_SHADERS - shaders are always used if they are available
* SDL_HINT_RENDER_SCALE_QUALITY - textures now default to linear filtering, use SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST) if you want nearest pixel mode instead
* SDL_HINT_THREAD_STACK_SIZE - the stack size can be specified using SDL_CreateThreadWithProperties()
* SDL_HINT_VIDEO_EXTERNAL_CONTEXT - replaced with SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN in SDL_CreateWindowWithProperties()
* SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL - replaced with SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN in SDL_CreateWindowWithProperties()
* SDL_HINT_VIDEO_FOREIGN_WINDOW_VULKAN - replaced with SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN in SDL_CreateWindowWithProperties()
* SDL_HINT_VIDEO_HIGHDPI_DISABLED - high DPI support is always enabled
* SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT - replaced with SDL_PROP_WINDOW_CREATE_WIN32_PIXEL_FORMAT_HWND_POINTER in SDL_CreateWindowWithProperties()
* SDL_HINT_VIDEO_X11_FORCE_EGL - use SDL_HINT_VIDEO_FORCE_EGL instead
* SDL_HINT_VIDEO_X11_XINERAMA - Xinerama no longer supported by the X11 backend
* SDL_HINT_VIDEO_X11_XVIDMODE - Xvidmode no longer supported by the X11 backend
* SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING - SDL now properly handles the 0x406D1388 Exception if no debugger intercepts it, preventing its propagation.
* SDL_HINT_WINDOWS_FORCE_MUTEX_CRITICAL_SECTIONS - Slim Reader/Writer Locks are always used if available
* SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4 - replaced with SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, defaulting to true
* SDL_HINT_WINRT_HANDLE_BACK_BUTTON - WinRT support was removed in SDL3.
* SDL_HINT_WINRT_PRIVACY_POLICY_LABEL - WinRT support was removed in SDL3.
* SDL_HINT_WINRT_PRIVACY_POLICY_URL - WinRT support was removed in SDL3.
* SDL_HINT_XINPUT_USE_OLD_JOYSTICK_MAPPING
* SDL_HINT_AUDIO_DEVICE_APP_NAME - replaced by either using the appname param to SDL_SetAppMetadata() or setting SDL_PROP_APP_METADATA_NAME_STRING with SDL_SetAppMetadataProperty()

The following environment variables have been renamed:
* SDL_AUDIODRIVER => SDL_AUDIO_DRIVER
* SDL_PATH_DSP => AUDIODEV
* SDL_VIDEODRIVER => SDL_VIDEO_DRIVER

The following environment variables have been removed:
* SDL_AUDIO_ALSA_DEBUG - replaced by setting the hint SDL_HINT_LOGGING to "audio=debug"
* SDL_DISKAUDIODELAY - replaced with the hint SDL_HINT_AUDIO_DISK_TIMESCALE which allows scaling the audio time rather than specifying an absolute delay.
* SDL_DISKAUDIOFILE - replaced with the hint SDL_HINT_AUDIO_DISK_OUTPUT_FILE
* SDL_DISKAUDIOFILEIN - replaced with the hint SDL_HINT_AUDIO_DISK_INPUT_FILE
* SDL_DUMMYAUDIODELAY - replaced with the hint SDL_HINT_AUDIO_DUMMY_TIMESCALE which allows scaling the audio time rather than specifying an absolute delay.
* SDL_HAPTIC_GAIN_MAX
* SDL_HIDAPI_DISABLE_LIBUSB - replaced with the hint SDL_HINT_HIDAPI_LIBUSB
* SDL_HIDAPI_JOYSTICK_DISABLE_UDEV - replaced with the hint SDL_HINT_HIDAPI_UDEV
* SDL_INPUT_FREEBSD_KEEP_KBD - replaced with the hint SDL_HINT_MUTE_CONSOLE_KEYBOARD
* SDL_INPUT_LINUX_KEEP_KBD - replaced with the hint SDL_HINT_MUTE_CONSOLE_KEYBOARD
* VITA_DISABLE_TOUCH_BACK - replaced with the hint SDL_HINT_VITA_ENABLE_BACK_TOUCH
* VITA_DISABLE_TOUCH_FRONT - replaced with the hint SDL_HINT_VITA_ENABLE_FRONT_TOUCH
* VITA_MODULE_PATH - replaced with the hint SDL_HINT_VITA_MODULE_PATH
* VITA_PVR_OGL - replaced with the hint SDL_HINT_VITA_PVR_OPENGL
* VITA_PVR_SKIP_INIT - replaced with the hint SDL_HINT_VITA_PVR_INIT
* VITA_RESOLUTION - replaced with the hint SDL_HINT_VITA_RESOLUTION

The following functions have been removed:
* SDL_ClearHints() - replaced with SDL_ResetHints()

The following functions have been renamed:
* SDL_DelHintCallback() => SDL_RemoveHintCallback()

## SDL_init.h

On Haiku OS, SDL no longer sets the current working directory to the executable's path during SDL_Init(). If you need this functionality, the fastest solution is to add this code directly after the call to SDL_Init:

```c
{
    const char *path = SDL_GetBasePath();
    if (path) {
        chdir(path);
    }
}
```

The following symbols have been renamed:
* SDL_INIT_GAMECONTROLLER => SDL_INIT_GAMEPAD

The following symbols have been removed:
* SDL_INIT_NOPARACHUTE
* SDL_INIT_EVERYTHING - you should only initialize the subsystems you are using
* SDL_INIT_TIMER - no longer needed before calling SDL_AddTimer()

## SDL_joystick.h

SDL_JoystickID has changed from Sint32 to Uint32, with an invalid ID being 0.

Rather than iterating over joysticks using device index, there is a new function SDL_GetJoysticks() to get the current list of joysticks, and new functions to get information about joysticks from their instance ID:
```c
{
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
        int i, num_joysticks;
        SDL_JoystickID *joysticks = SDL_GetJoysticks(&num_joysticks);
        if (joysticks) {
            for (i = 0; i < num_joysticks; ++i) {
                SDL_JoystickID instance_id = joysticks[i];
                const char *name = SDL_GetJoystickNameForID(instance_id);
                const char *path = SDL_GetJoystickPathForID(instance_id);

                SDL_Log("Joystick %" SDL_PRIu32 ": %s%s%s VID 0x%.4x, PID 0x%.4x\n",
                        instance_id, name ? name : "Unknown", path ? ", " : "", path ? path : "", SDL_GetJoystickVendorForID(instance_id), SDL_GetJoystickProductForID(instance_id));
            }
            SDL_free(joysticks);
        }
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }
}
```

The SDL_EVENT_JOYSTICK_ADDED event now provides the joystick instance ID in the `which` member of the jdevice event structure.

The functions SDL_GetJoysticks(), SDL_GetJoystickNameForID(), SDL_GetJoystickPathForID(), SDL_GetJoystickPlayerIndexForID(), SDL_GetJoystickGUIDForID(), SDL_GetJoystickVendorForID(), SDL_GetJoystickProductForID(), SDL_GetJoystickProductVersionForID(), and SDL_GetJoystickTypeForID() have been added to directly query the list of available joysticks.

SDL_AttachVirtualJoystick() now returns the joystick instance ID instead of a device index, and returns 0 if there was an error.

SDL_VirtualJoystickDesc version should not be set to SDL_VIRTUAL_JOYSTICK_DESC_VERSION, instead the structure should be initialized using SDL_INIT_INTERFACE().

The following functions have been renamed:
* SDL_JoystickAttachVirtualEx() => SDL_AttachVirtualJoystick()
* SDL_JoystickClose() => SDL_CloseJoystick()
* SDL_JoystickDetachVirtual() => SDL_DetachVirtualJoystick(), returns bool
* SDL_JoystickFromInstanceID() => SDL_GetJoystickFromID()
* SDL_JoystickFromPlayerIndex() => SDL_GetJoystickFromPlayerIndex()
* SDL_JoystickGetAttached() => SDL_JoystickConnected()
* SDL_JoystickGetAxis() => SDL_GetJoystickAxis()
* SDL_JoystickGetAxisInitialState() => SDL_GetJoystickAxisInitialState()
* SDL_JoystickGetBall() => SDL_GetJoystickBall(), returns bool
* SDL_JoystickGetButton() => SDL_GetJoystickButton()
* SDL_JoystickGetFirmwareVersion() => SDL_GetJoystickFirmwareVersion()
* SDL_JoystickGetGUID() => SDL_GetJoystickGUID()
* SDL_JoystickGetGUIDFromString() => SDL_StringToGUID()
* SDL_JoystickGetHat() => SDL_GetJoystickHat()
* SDL_JoystickGetPlayerIndex() => SDL_GetJoystickPlayerIndex()
* SDL_JoystickGetProduct() => SDL_GetJoystickProduct()
* SDL_JoystickGetProductVersion() => SDL_GetJoystickProductVersion()
* SDL_JoystickGetSerial() => SDL_GetJoystickSerial()
* SDL_JoystickGetType() => SDL_GetJoystickType()
* SDL_JoystickGetVendor() => SDL_GetJoystickVendor()
* SDL_JoystickInstanceID() => SDL_GetJoystickID()
* SDL_JoystickIsVirtual() => SDL_IsJoystickVirtual()
* SDL_JoystickName() => SDL_GetJoystickName()
* SDL_JoystickNumAxes() => SDL_GetNumJoystickAxes()
* SDL_JoystickNumBalls() => SDL_GetNumJoystickBalls()
* SDL_JoystickNumButtons() => SDL_GetNumJoystickButtons()
* SDL_JoystickNumHats() => SDL_GetNumJoystickHats()
* SDL_JoystickOpen() => SDL_OpenJoystick()
* SDL_JoystickPath() => SDL_GetJoystickPath()
* SDL_JoystickRumble() => SDL_RumbleJoystick(), returns bool
* SDL_JoystickRumbleTriggers() => SDL_RumbleJoystickTriggers(), returns bool
* SDL_JoystickSendEffect() => SDL_SendJoystickEffect(), returns bool
* SDL_JoystickSetLED() => SDL_SetJoystickLED(), returns bool
* SDL_JoystickSetPlayerIndex() => SDL_SetJoystickPlayerIndex(), returns bool
* SDL_JoystickSetVirtualAxis() => SDL_SetJoystickVirtualAxis(), returns bool
* SDL_JoystickSetVirtualButton() => SDL_SetJoystickVirtualButton(), returns bool
* SDL_JoystickSetVirtualHat() => SDL_SetJoystickVirtualHat(), returns bool
* SDL_JoystickUpdate() => SDL_UpdateJoysticks()

The following symbols have been renamed:
* SDL_JOYSTICK_TYPE_GAMECONTROLLER => SDL_JOYSTICK_TYPE_GAMEPAD

The following functions have been removed:
* SDL_JoystickAttachVirtual() - replaced with SDL_AttachVirtualJoystick()
* SDL_JoystickCurrentPowerLevel() - replaced with SDL_GetJoystickConnectionState() and SDL_GetJoystickPowerInfo()
* SDL_JoystickEventState() - replaced with SDL_SetJoystickEventsEnabled() and SDL_JoystickEventsEnabled()
* SDL_JoystickGetDeviceGUID() - replaced with SDL_GetJoystickGUIDForID()
* SDL_JoystickGetDeviceInstanceID()
* SDL_JoystickGetDevicePlayerIndex() - replaced with SDL_GetJoystickPlayerIndexForID()
* SDL_JoystickGetDeviceProduct() - replaced with SDL_GetJoystickProductForID()
* SDL_JoystickGetDeviceProductVersion() - replaced with SDL_GetJoystickProductVersionForID()
* SDL_JoystickGetDeviceType() - replaced with SDL_GetJoystickTypeForID()
* SDL_JoystickGetDeviceVendor() - replaced with SDL_GetJoystickVendorForID()
* SDL_JoystickGetGUIDString() - replaced with SDL_GUIDToString()
* SDL_JoystickHasLED() - replaced with SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN
* SDL_JoystickHasRumble() - replaced with SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN
* SDL_JoystickHasRumbleTriggers() - replaced with SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN
* SDL_JoystickNameForIndex() - replaced with SDL_GetJoystickNameForID()
* SDL_JoystickPathForIndex() - replaced with SDL_GetJoystickPathForID()
* SDL_NumJoysticks() - replaced with SDL_GetJoysticks()
* SDL_VIRTUAL_JOYSTICK_DESC_VERSION - no longer needed

The following symbols have been removed:
* SDL_JOYBALLMOTION

The following structures have been renamed:
* SDL_JoystickGUID => SDL_GUID

## SDL_keyboard.h

Text input is no longer automatically enabled when initializing video, you should call SDL_StartTextInput() when you want to receive text input and call SDL_StopTextInput() when you are done. Starting text input may shown an input method editor (IME) and cause key up/down events to be skipped, so should only be enabled when the application wants text input.

The text input state hase been changed to be window-specific. SDL_StartTextInput(), SDL_StopTextInput(), SDL_TextInputActive(), and SDL_ClearComposition() all now take a window parameter.

SDL_GetDefaultKeyFromScancode(), SDL_GetKeyFromScancode(), and SDL_GetScancodeFromKey() take an SDL_Keymod parameter and use that to provide the correct result based on keyboard modifier state.

SDL_GetKeyboardState() returns a pointer to bool instead of Uint8.

The following functions have been renamed:
* SDL_IsScreenKeyboardShown() => SDL_ScreenKeyboardShown()
* SDL_IsTextInputActive() => SDL_TextInputActive()

The following functions have been removed:
* SDL_IsTextInputShown()
* SDL_SetTextInputRect() - replaced with SDL_SetTextInputArea()

The following structures have been removed:
* SDL_Keysym

## SDL_keycode.h

SDL_Keycode is now Uint32 and the SDLK_* constants are now defines instead of an enum, to more clearly reflect that they are a subset of the possible values of an SDL_Keycode.

The following symbols have been removed:

* KMOD_RESERVED - No replacement. A bit named "RESERVED" probably shouldn't be used in an app, but if you need it, this was equivalent to KMOD_SCROLL (0x8000) in SDL2.
* SDLK_WWW
* SDLK_MAIL
* SDLK_CALCULATOR
* SDLK_COMPUTER
* SDLK_BRIGHTNESSDOWN
* SDLK_BRIGHTNESSUP
* SDLK_DISPLAYSWITCH
* SDLK_KBDILLUMTOGGLE
* SDLK_KBDILLUMDOWN
* SDLK_KBDILLUMUP
* SDLK_APP1
* SDLK_APP2


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
* KMOD_RGUI => SDL_KMOD_RGUI
* KMOD_RSHIFT => SDL_KMOD_RSHIFT
* KMOD_SCROLL => SDL_KMOD_SCROLL
* KMOD_SHIFT => SDL_KMOD_SHIFT
* SDLK_AUDIOFASTFORWARD => SDLK_MEDIA_FAST_FORWARD
* SDLK_AUDIOMUTE => SDLK_MUTE
* SDLK_AUDIONEXT => SDLK_MEDIA_NEXT_TRACK
* SDLK_AUDIOPLAY => SDLK_MEDIA_PLAY
* SDLK_AUDIOPREV => SDLK_MEDIA_PREVIOUS_TRACK
* SDLK_AUDIOREWIND => SDLK_MEDIA_REWIND
* SDLK_AUDIOSTOP => SDLK_MEDIA_STOP
* SDLK_BACKQUOTE => SDLK_GRAVE
* SDLK_EJECT => SDLK_MEDIA_EJECT
* SDLK_MEDIASELECT => SDLK_MEDIA_SELECT
* SDLK_QUOTE => SDLK_APOSTROPHE
* SDLK_QUOTEDBL => SDLK_DBLAPOSTROPHE
* SDLK_a => SDLK_A
* SDLK_b => SDLK_B
* SDLK_c => SDLK_C
* SDLK_d => SDLK_D
* SDLK_e => SDLK_E
* SDLK_f => SDLK_F
* SDLK_g => SDLK_G
* SDLK_h => SDLK_H
* SDLK_i => SDLK_I
* SDLK_j => SDLK_J
* SDLK_k => SDLK_K
* SDLK_l => SDLK_L
* SDLK_m => SDLK_M
* SDLK_n => SDLK_N
* SDLK_o => SDLK_O
* SDLK_p => SDLK_P
* SDLK_q => SDLK_Q
* SDLK_r => SDLK_R
* SDLK_s => SDLK_S
* SDLK_t => SDLK_T
* SDLK_u => SDLK_U
* SDLK_v => SDLK_V
* SDLK_w => SDLK_W
* SDLK_x => SDLK_X
* SDLK_y => SDLK_Y
* SDLK_z => SDLK_Z

## SDL_loadso.h

SDL_LoadFunction() now returns `SDL_FunctionPointer` instead of `void *`, and should be cast to the appropriate function type. You can define SDL_FUNCTION_POINTER_IS_VOID_POINTER in your project to restore the previous behavior.

## SDL_log.h

SDL_Log() no longer prints a log prefix by default for SDL_LOG_PRIORITY_INFO and below. The log prefixes can be customized with SDL_SetLogPriorityPrefix().

The following macros have been removed:
* SDL_MAX_LOG_MESSAGE - there's no message length limit anymore. If you need an artificial limit, this used to be 4096 in SDL versions before 2.0.24.

The following functions have been renamed:
* SDL_LogGetOutputFunction() => SDL_GetLogOutputFunction()
* SDL_LogGetPriority() => SDL_GetLogPriority()
* SDL_LogResetPriorities() => SDL_ResetLogPriorities()
* SDL_LogSetAllPriority() => SDL_SetLogPriorities()
* SDL_LogSetOutputFunction() => SDL_SetLogOutputFunction()
* SDL_LogSetPriority() => SDL_SetLogPriority()

The following symbols have been renamed:
* SDL_NUM_LOG_PRIORITIES => SDL_LOG_PRIORITY_COUNT

## SDL_main.h

SDL3 doesn't have a static libSDLmain to link against anymore.
Instead SDL_main.h is now a header-only library **and not included by SDL.h anymore**.

Using it is really simple: Just `#include <SDL3/SDL_main.h>` in the source file with your standard
`int main(int argc, char* argv[])` function. See docs/README-main-functions.md for details.

Several platform-specific entry point functions have been removed as unnecessary. If for some reason you explicitly need them, here are easy replacements:

```c
#define SDL_UIKitRunApp(ARGC, ARGV, MAIN_FUNC)  SDL_RunApp(ARGC, ARGV, MAIN_FUNC, NULL)
#define SDL_GDKRunApp(MAIN_FUNC, RESERVED)  SDL_RunApp(0, NULL, MAIN_FUNC, RESERVED)
```

The following functions have been removed:
* SDL_WinRTRunApp() - WinRT support was removed in SDL3.


## SDL_messagebox.h

The buttonid field of SDL_MessageBoxButtonData has been renamed buttonID.

The following symbols have been renamed:
* SDL_MESSAGEBOX_COLOR_MAX => SDL_MESSAGEBOX_COLOR_COUNT

## SDL_metal.h

SDL_Metal_GetDrawableSize() has been removed. SDL_GetWindowSizeInPixels() can be used in its place.

## SDL_mouse.h

SDL_ShowCursor() has been split into three functions: SDL_ShowCursor(), SDL_HideCursor(), and SDL_CursorVisible()

SDL_GetMouseState(), SDL_GetGlobalMouseState(), SDL_GetRelativeMouseState(), SDL_WarpMouseInWindow(), and SDL_WarpMouseGlobal() all use floating point mouse positions, to provide sub-pixel precision on platforms that support it.

SDL_SystemCursor's items from SDL2 have been renamed to match CSS cursor names.

The following functions have been renamed:
* SDL_FreeCursor() => SDL_DestroyCursor()

The following functions have been removed:
* SDL_SetRelativeMouseMode() - replaced with SDL_SetWindowRelativeMouseMode()
* SDL_GetRelativeMouseMode() - replaced with SDL_GetWindowRelativeMouseMode()

The following symbols have been renamed:
* SDL_NUM_SYSTEM_CURSORS => SDL_SYSTEM_CURSOR_COUNT
* SDL_SYSTEM_CURSOR_ARROW => SDL_SYSTEM_CURSOR_DEFAULT
* SDL_SYSTEM_CURSOR_HAND => SDL_SYSTEM_CURSOR_POINTER
* SDL_SYSTEM_CURSOR_IBEAM => SDL_SYSTEM_CURSOR_TEXT
* SDL_SYSTEM_CURSOR_NO => SDL_SYSTEM_CURSOR_NOT_ALLOWED
* SDL_SYSTEM_CURSOR_SIZEALL => SDL_SYSTEM_CURSOR_MOVE
* SDL_SYSTEM_CURSOR_SIZENESW => SDL_SYSTEM_CURSOR_NESW_RESIZE
* SDL_SYSTEM_CURSOR_SIZENS => SDL_SYSTEM_CURSOR_NS_RESIZE
* SDL_SYSTEM_CURSOR_SIZENWSE => SDL_SYSTEM_CURSOR_NWSE_RESIZE
* SDL_SYSTEM_CURSOR_SIZEWE => SDL_SYSTEM_CURSOR_EW_RESIZE
* SDL_SYSTEM_CURSOR_WAITARROW => SDL_SYSTEM_CURSOR_PROGRESS

## SDL_mutex.h

SDL_MUTEX_MAXWAIT has been removed; it suggested there was a maximum timeout one could outlive, instead of an infinite wait. Instead, pass a -1 to functions that accepted this symbol.

SDL_MUTEX_TIMEDOUT has been removed, the wait functions return true if the operation succeeded or false if they timed out.

SDL_LockMutex(), SDL_UnlockMutex(), SDL_WaitSemaphore(), SDL_SignalSemaphore(), SDL_WaitCondition(), SDL_SignalCondition(), and SDL_BroadcastCondition() now return void; if the object is valid (including being a NULL pointer, which returns immediately), these functions never fail. If the object is invalid or the caller does something illegal, like unlock another thread's mutex, this is considered undefined behavior.

SDL_TryWaitSemaphore(), SDL_WaitSemaphoreTimeout(), and SDL_WaitConditionTimeout() now return true if the operation succeeded or false if they timed out.

The following functions have been renamed:
* SDL_CondBroadcast() => SDL_BroadcastCondition()
* SDL_CondSignal() => SDL_SignalCondition()
* SDL_CondWait() => SDL_WaitCondition()
* SDL_CondWaitTimeout() => SDL_WaitConditionTimeout(), returns bool
* SDL_CreateCond() => SDL_CreateCondition()
* SDL_DestroyCond() => SDL_DestroyCondition()
* SDL_SemPost() => SDL_SignalSemaphore()
* SDL_SemTryWait() => SDL_TryWaitSemaphore(), returns bool
* SDL_SemValue() => SDL_GetSemaphoreValue()
* SDL_SemWait() => SDL_WaitSemaphore()
* SDL_SemWaitTimeout() => SDL_WaitSemaphoreTimeout(), returns bool

The following symbols have been renamed:
* SDL_cond => SDL_Condition
* SDL_mutex => SDL_Mutex
* SDL_sem => SDL_Semaphore

## SDL_pixels.h

SDL_PixelFormat has been renamed SDL_PixelFormatDetails and just describes the pixel format, it does not include a palette for indexed pixel types.

SDL_PixelFormatEnum has been renamed SDL_PixelFormat and is used instead of Uint32 for API functions that refer to pixel format by enumerated value.

SDL_MapRGB(), SDL_MapRGBA(), SDL_GetRGB(), and SDL_GetRGBA() take an optional palette parameter for indexed color lookups.

Code that used to look like this:
```c
    SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
```
should be changed to:
```c
    SDL_GetRGBA(pixel, SDL_GetPixelFormatDetails(surface->format), SDL_GetSurfacePalette(surface), &r, &g, &b, &a);
```

Code that used to look like this:
```c
    pixel = SDL_MapRGBA(surface->format, r, g, b, a);
```
should be changed to:
```c
    pixel = SDL_MapSurfaceRGBA(surface, r, g, b, a);
```

SDL_CalculateGammaRamp has been removed, because SDL_SetWindowGammaRamp has been removed as well due to poor support in modern operating systems (see [SDL_video.h](#sdl_videoh)).

The following functions have been renamed:
* SDL_AllocFormat() => SDL_GetPixelFormatDetails()
* SDL_AllocPalette() => SDL_CreatePalette()
* SDL_FreePalette() => SDL_DestroyPalette()
* SDL_MasksToPixelFormatEnum() => SDL_GetPixelFormatForMasks()
* SDL_PixelFormatEnumToMasks() => SDL_GetMasksForPixelFormat(), returns bool

The following symbols have been renamed:
* SDL_PIXELFORMAT_BGR444 => SDL_PIXELFORMAT_XBGR4444
* SDL_PIXELFORMAT_BGR555 => SDL_PIXELFORMAT_XBGR1555
* SDL_PIXELFORMAT_BGR888 => SDL_PIXELFORMAT_XBGR8888
* SDL_PIXELFORMAT_RGB444 => SDL_PIXELFORMAT_XRGB4444
* SDL_PIXELFORMAT_RGB555 => SDL_PIXELFORMAT_XRGB1555
* SDL_PIXELFORMAT_RGB888 => SDL_PIXELFORMAT_XRGB8888

The following functions have been removed:
* SDL_FreeFormat()
* SDL_SetPixelFormatPalette()

The following macros have been removed:
* SDL_Colour - use SDL_Color instead

The following structures have been renamed:
* SDL_PixelFormat => SDL_PixelFormatDetails

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
| `__XBOXONE__`     | `SDL_PLATFORM_XBOXONE`    |
| `__XBOXSERIES__`  | `SDL_PLATFORM_XBOXSERIES` |

You can use the Python script [rename_macros.py](https://github.com/libsdl-org/SDL/blob/main/build-scripts/rename_macros.py) to automatically rename these in your source code.

A new macro `SDL_PLATFORM_WINDOWS` has been added that is true for all Windows platforms, including Xbox, GDK, etc.

The following platform preprocessor macros have been removed:
* `__DREAMCAST__`
* `__NACL__`
* `__PNACL__`
* `__WINDOWS__`
* `__WINRT__`


## SDL_quit.h

SDL_quit.h has been completely removed. It only had one symbol in it--SDL_QuitRequested--and if you want it, you can just add this to your app...

```c
#define SDL_QuitRequested() (SDL_PumpEvents(), (SDL_PeepEvents(NULL,0,SDL_PEEKEVENT,SDL_EVENT_QUIT,SDL_EVENT_QUIT) > 0))
```

...but this macro is sort of messy, calling two functions in sequence in an expression.

The following macros have been removed:
* SDL_QuitRequested - call SDL_PumpEvents() then SDL_PeepEvents() directly, instead.

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
* SDL_UnionFRect() => SDL_GetRectUnionFloat(), returns bool
* SDL_UnionRect() => SDL_GetRectUnion(), returns bool

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

SDL_CreateRenderer()'s second argument is no longer an integer index, but a
`const char *` representing a renderer's name; if you were just using a for-loop to find
which index is the "opengl" or whatnot driver, you can just pass that string directly
here, now. Passing NULL is the same as passing -1 here in SDL2, to signify you want SDL
to decide for you.

SDL_CreateRenderer()'s flags parameter has been removed. See specific flags below for how to achieve the same functionality in SDL 3.0.

SDL_CreateWindowAndRenderer() now takes the window title as the first parameter.

SDL_GetRendererInfo() has been removed. The name of a renderer can be retrieved using SDL_GetRendererName(), and the other information is available as properties on the renderer.

Textures are created with SDL_SCALEMODE_LINEAR by default, and use SDL_BLENDMODE_BLEND by default if they are created with a format that has an alpha channel.

SDL_QueryTexture() has been removed. The properties of the texture can be queried using SDL_PROP_TEXTURE_FORMAT_NUMBER, SDL_PROP_TEXTURE_ACCESS_NUMBER, SDL_PROP_TEXTURE_WIDTH_NUMBER, and SDL_PROP_TEXTURE_HEIGHT_NUMBER. A function SDL_GetTextureSize() has been added to get the size of the texture as floating point values.

Mouse and touch events are no longer filtered to change their coordinates, instead you
can call SDL_ConvertEventToRenderCoordinates() to explicitly map event coordinates into
the rendering viewport.

SDL_RenderWindowToLogical() and SDL_RenderLogicalToWindow() have been renamed SDL_RenderCoordinatesFromWindow() and SDL_RenderCoordinatesToWindow() and take floating point coordinates in both directions.

The viewport, clipping state, and scale for render targets are now persistent and will remain set whenever they are active.

SDL_Vertex has been changed to use floating point colors, in the range of [0..1] for SDR content.

SDL_RenderReadPixels() returns a surface instead of filling in preallocated memory.

The following functions have been renamed:
* SDL_GetRendererOutputSize() => SDL_GetCurrentRenderOutputSize(), returns bool
* SDL_RenderCopy() => SDL_RenderTexture(), returns bool
* SDL_RenderCopyEx() => SDL_RenderTextureRotated(), returns bool
* SDL_RenderCopyExF() => SDL_RenderTextureRotated(), returns bool
* SDL_RenderCopyF() => SDL_RenderTexture(), returns bool
* SDL_RenderDrawLine() => SDL_RenderLine(), returns bool
* SDL_RenderDrawLineF() => SDL_RenderLine(), returns bool
* SDL_RenderDrawLines() => SDL_RenderLines(), returns bool
* SDL_RenderDrawLinesF() => SDL_RenderLines(), returns bool
* SDL_RenderDrawPoint() => SDL_RenderPoint(), returns bool
* SDL_RenderDrawPointF() => SDL_RenderPoint(), returns bool
* SDL_RenderDrawPoints() => SDL_RenderPoints(), returns bool
* SDL_RenderDrawPointsF() => SDL_RenderPoints(), returns bool
* SDL_RenderDrawRect() => SDL_RenderRect(), returns bool
* SDL_RenderDrawRectF() => SDL_RenderRect(), returns bool
* SDL_RenderDrawRects() => SDL_RenderRects(), returns bool
* SDL_RenderDrawRectsF() => SDL_RenderRects(), returns bool
* SDL_RenderFillRectF() => SDL_RenderFillRect(), returns bool
* SDL_RenderFillRectsF() => SDL_RenderFillRects(), returns bool
* SDL_RenderFlush() => SDL_FlushRenderer(), returns bool
* SDL_RenderGetClipRect() => SDL_GetRenderClipRect(), returns bool
* SDL_RenderGetIntegerScale() => SDL_GetRenderIntegerScale()
* SDL_RenderGetLogicalSize() => SDL_GetRenderLogicalPresentation(), returns bool
* SDL_RenderGetMetalCommandEncoder() => SDL_GetRenderMetalCommandEncoder()
* SDL_RenderGetMetalLayer() => SDL_GetRenderMetalLayer()
* SDL_RenderGetScale() => SDL_GetRenderScale(), returns bool
* SDL_RenderGetViewport() => SDL_GetRenderViewport(), returns bool
* SDL_RenderGetWindow() => SDL_GetRenderWindow()
* SDL_RenderIsClipEnabled() => SDL_RenderClipEnabled()
* SDL_RenderLogicalToWindow() => SDL_RenderCoordinatesToWindow(), returns bool
* SDL_RenderSetClipRect() => SDL_SetRenderClipRect(), returns bool
* SDL_RenderSetLogicalSize() => SDL_SetRenderLogicalPresentation(), returns bool
* SDL_RenderSetScale() => SDL_SetRenderScale(), returns bool
* SDL_RenderSetVSync() => SDL_SetRenderVSync(), returns bool
* SDL_RenderSetViewport() => SDL_SetRenderViewport(), returns bool
* SDL_RenderWindowToLogical() => SDL_RenderCoordinatesFromWindow(), returns bool

The following functions have been removed:
* SDL_GL_BindTexture() - use SDL_GetTextureProperties() to get the OpenGL texture ID and bind the texture directly
* SDL_GL_UnbindTexture() - use SDL_GetTextureProperties() to get the OpenGL texture ID and unbind the texture directly
* SDL_GetTextureUserData() - use SDL_GetTextureProperties() instead
* SDL_RenderGetIntegerScale()
* SDL_RenderSetIntegerScale() - this is now explicit with SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
* SDL_RenderTargetSupported() - render targets are always supported
* SDL_SetTextureUserData() - use SDL_GetTextureProperties() instead

The following enums have been renamed:
* SDL_RendererFlip => SDL_FlipMode - moved to SDL_surface.h

The following symbols have been renamed:
* SDL_ScaleModeLinear => SDL_SCALEMODE_LINEAR
* SDL_ScaleModeNearest => SDL_SCALEMODE_NEAREST

The following symbols have been removed:
* SDL_RENDERER_ACCELERATED - all renderers except `SDL_SOFTWARE_RENDERER` are accelerated
* SDL_RENDERER_PRESENTVSYNC - replaced with SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER during renderer creation and SDL_PROP_RENDERER_VSYNC_NUMBER after renderer creation
* SDL_RENDERER_SOFTWARE - you can check whether the name of the renderer is `SDL_SOFTWARE_RENDERER`
* SDL_RENDERER_TARGETTEXTURE - all renderers support target texture functionality
* SDL_ScaleModeBest = use SDL_SCALEMODE_LINEAR instead

## SDL_rwops.h

The following symbols have been renamed:
* RW_SEEK_CUR => SDL_IO_SEEK_CUR
* RW_SEEK_END => SDL_IO_SEEK_END
* RW_SEEK_SET => SDL_IO_SEEK_SET

SDL_rwops.h is now named SDL_iostream.h

SDL_RWops is now an opaque structure, and has been renamed to SDL_IOStream. The SDL3 APIs to create an SDL_IOStream (SDL_IOFromFile, etc) are renamed but otherwise still function as they did in SDL2. However, to make a custom SDL_IOStream with app-provided function pointers, call SDL_OpenIO and provide the function pointers through there. To call into an SDL_IOStream's functionality, use the standard APIs (SDL_ReadIO, etc), as the function pointers are internal.

SDL_IOStream is not to be confused with the unrelated standard C++ iostream class!

The RWops function pointers are now in a separate structure called SDL_IOStreamInterface, which is provided to SDL_OpenIO when creating a custom SDL_IOStream implementation. All the functions now take a `void *` userdata argument for their first parameter instead of an SDL_IOStream, since that's now an opaque structure.

SDL_RWread and SDL_RWwrite (and the read and write function pointers) have a different function signature in SDL3, in addition to being renamed.

Previously they looked more like stdio:

```c
size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size, size_t maxnum);
size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size, size_t maxnum);
```

But now they look more like POSIX:

```c
size_t SDL_ReadIO(SDL_IOStream *context, void *ptr, size_t size);
size_t SDL_WriteIO(SDL_IOStream *context, const void *ptr, size_t size);
```

Code that used to look like this:
```c
size_t custom_read(void *ptr, size_t size, size_t nitems, SDL_RWops *stream)
{
    return SDL_RWread(stream, ptr, size, nitems);
}
```
should be changed to:
```c
size_t custom_read(void *ptr, size_t size, size_t nitems, SDL_IOStream *stream)
{
    if (size > 0 && nitems > 0) {
        return SDL_ReadIO(stream, ptr, size * nitems) / size;
    }
    return 0;
}
```

SDL_RWops::type was removed; it wasn't meaningful for app-provided implementations at all, and wasn't much use for SDL's internal implementations, either. If you _have_ to identify the type, you can examine the SDL_IOStream's properties to detect built-in implementations.

SDL_IOStreamInterface::close implementations should clean up their own userdata, but not call SDL_CloseIO on themselves; now the contract is always that SDL_CloseIO is called, which calls `->close` before freeing the opaque object.

SDL_AllocRW(), SDL_FreeRW(), SDL_RWclose() and direct access to the `->close` function pointer have been removed from the API, so there's only one path to manage RWops lifetimes now: SDL_OpenIO() and SDL_CloseIO().

SDL_RWFromFP has been removed from the API, due to issues when the SDL library uses a different C runtime from the application.

You can implement this in your own code easily:
```c
#include <stdio.h>

typedef struct IOStreamStdioFPData
{
    FILE *fp;
    bool autoclose;
} IOStreamStdioFPData;

static Sint64 SDLCALL stdio_seek(void *userdata, Sint64 offset, int whence)
{
    FILE *fp = ((IOStreamStdioFPData *) userdata)->fp;
    int stdiowhence;

    switch (whence) {
    case SDL_IO_SEEK_SET:
        stdiowhence = SEEK_SET;
        break;
    case SDL_IO_SEEK_CUR:
        stdiowhence = SEEK_CUR;
        break;
    case SDL_IO_SEEK_END:
        stdiowhence = SEEK_END;
        break;
    default:
        SDL_SetError("Unknown value for 'whence'");
        return -1;
    }

    if (fseek(fp, (fseek_off_t)offset, stdiowhence) == 0) {
        const Sint64 pos = ftell(fp);
        if (pos < 0) {
            SDL_SetError("Couldn't get stream offset");
            return -1;
        }
        return pos;
    }
    SDL_SetError("Couldn't seek in stream");
    return -1;
}

static size_t SDLCALL stdio_read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    FILE *fp = ((IOStreamStdioFPData *) userdata)->fp;
    const size_t bytes = fread(ptr, 1, size, fp);
    if (bytes == 0 && ferror(fp)) {
        SDL_SetError("Couldn't read stream");
    }
    return bytes;
}

static size_t SDLCALL stdio_write(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    FILE *fp = ((IOStreamStdioFPData *) userdata)->fp;
    const size_t bytes = fwrite(ptr, 1, size, fp);
    if (bytes == 0 && ferror(fp)) {
        SDL_SetError("Couldn't write stream");
    }
    return bytes;
}

static bool SDLCALL stdio_close(void *userdata)
{
    IOStreamStdioData *rwopsdata = (IOStreamStdioData *) userdata;
    bool status = true;
    if (rwopsdata->autoclose) {
        if (fclose(rwopsdata->fp) != 0) {
            SDL_SetError("Couldn't close stream");
            status = false;
        }
    }
    return status;
}

SDL_IOStream *SDL_RWFromFP(FILE *fp, bool autoclose)
{
    SDL_IOStreamInterface iface;
    IOStreamStdioFPData *rwopsdata;
    SDL_IOStream *rwops;

    rwopsdata = (IOStreamStdioFPData *) SDL_malloc(sizeof (*rwopsdata));
    if (!rwopsdata) {
        return NULL;
    }

    SDL_INIT_INTERFACE(&iface);
    /* There's no stdio_size because SDL_GetIOSize emulates it the same way we'd do it for stdio anyhow. */
    iface.seek = stdio_seek;
    iface.read = stdio_read;
    iface.write = stdio_write;
    iface.close = stdio_close;

    rwopsdata->fp = fp;
    rwopsdata->autoclose = autoclose;

    rwops = SDL_OpenIO(&iface, rwopsdata);
    if (!rwops) {
        iface.close(rwopsdata);
    }
    return rwops;
}
```

The internal `FILE *` is available through a standard SDL_IOStream property, for streams made through SDL_IOFromFile() that use stdio behind the scenes; apps use this pointer at their own risk and should make sure that SDL and the app are using the same C runtime.

On Apple platforms, SDL_RWFromFile (now called SDL_IOFromFile) no longer tries to read from inside the app bundle's resource directory, instead now using the specified path unchanged. One can use SDL_GetBasePath() to find the resource directory on these platforms.


The functions SDL_ReadU8(), SDL_ReadU16LE(), SDL_ReadU16BE(), SDL_ReadU32LE(), SDL_ReadU32BE(), SDL_ReadU64LE(), and SDL_ReadU64BE() now return true if the read succeeded and false if it didn't, and store the data in a pointer passed in as a parameter.

The following functions have been renamed:
* SDL_RWFromConstMem() => SDL_IOFromConstMem()
* SDL_RWFromFile() => SDL_IOFromFile()
* SDL_RWFromMem() => SDL_IOFromMem()
* SDL_RWclose() => SDL_CloseIO(), returns bool
* SDL_RWread() => SDL_ReadIO()
* SDL_RWseek() => SDL_SeekIO()
* SDL_RWsize() => SDL_GetIOSize()
* SDL_RWtell() => SDL_TellIO()
* SDL_RWwrite() => SDL_WriteIO()
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


The following structures have been renamed:
* SDL_RWops => SDL_IOStream

## SDL_scancode.h

The following symbols have been removed:
* SDL_SCANCODE_WWW
* SDL_SCANCODE_MAIL
* SDL_SCANCODE_CALCULATOR
* SDL_SCANCODE_COMPUTER
* SDL_SCANCODE_BRIGHTNESSDOWN
* SDL_SCANCODE_BRIGHTNESSUP
* SDL_SCANCODE_DISPLAYSWITCH
* SDL_SCANCODE_KBDILLUMTOGGLE
* SDL_SCANCODE_KBDILLUMDOWN
* SDL_SCANCODE_KBDILLUMUP
* SDL_SCANCODE_APP1
* SDL_SCANCODE_APP2

The following symbols have been renamed:
* SDL_NUM_SCANCODES => SDL_SCANCODE_COUNT
* SDL_SCANCODE_AUDIOFASTFORWARD => SDL_SCANCODE_MEDIA_FAST_FORWARD
* SDL_SCANCODE_AUDIOMUTE => SDL_SCANCODE_MUTE
* SDL_SCANCODE_AUDIONEXT => SDL_SCANCODE_MEDIA_NEXT_TRACK
* SDL_SCANCODE_AUDIOPLAY => SDL_SCANCODE_MEDIA_PLAY
* SDL_SCANCODE_AUDIOPREV => SDL_SCANCODE_MEDIA_PREVIOUS_TRACK
* SDL_SCANCODE_AUDIOREWIND => SDL_SCANCODE_MEDIA_REWIND
* SDL_SCANCODE_AUDIOSTOP => SDL_SCANCODE_MEDIA_STOP
* SDL_SCANCODE_EJECT => SDL_SCANCODE_MEDIA_EJECT
* SDL_SCANCODE_MEDIASELECT => SDL_SCANCODE_MEDIA_SELECT

## SDL_sensor.h

SDL_SensorID has changed from Sint32 to Uint32, with an invalid ID being 0.

Rather than iterating over sensors using device index, there is a new function SDL_GetSensors() to get the current list of sensors, and new functions to get information about sensors from their instance ID:
```c
{
    if (SDL_InitSubSystem(SDL_INIT_SENSOR)) {
        int i, num_sensors;
        SDL_SensorID *sensors = SDL_GetSensors(&num_sensors);
        if (sensors) {
            for (i = 0; i < num_sensors; ++i) {
                SDL_Log("Sensor %" SDL_PRIu32 ": %s, type %d, platform type %d\n",
                        sensors[i],
                        SDL_GetSensorNameForID(sensors[i]),
                        SDL_GetSensorTypeForID(sensors[i]),
                        SDL_GetSensorNonPortableTypeForID(sensors[i]));
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
* SDL_SensorFromInstanceID() => SDL_GetSensorFromID()
* SDL_SensorGetData() => SDL_GetSensorData(), returns bool
* SDL_SensorGetInstanceID() => SDL_GetSensorID()
* SDL_SensorGetName() => SDL_GetSensorName()
* SDL_SensorGetNonPortableType() => SDL_GetSensorNonPortableType()
* SDL_SensorGetType() => SDL_GetSensorType()
* SDL_SensorOpen() => SDL_OpenSensor()
* SDL_SensorUpdate() => SDL_UpdateSensors()

The following functions have been removed:
* SDL_LockSensors()
* SDL_NumSensors() - replaced with SDL_GetSensors()
* SDL_SensorGetDeviceInstanceID()
* SDL_SensorGetDeviceName() - replaced with SDL_GetSensorNameForID()
* SDL_SensorGetDeviceNonPortableType() - replaced with SDL_GetSensorNonPortableTypeForID()
* SDL_SensorGetDeviceType() - replaced with SDL_GetSensorTypeForID()
* SDL_UnlockSensors()

## SDL_shape.h

This header has been removed and a simplified version of this API has been added as SDL_SetWindowShape() in SDL_video.h. See test/testshape.c for an example.

## SDL_stdinc.h

The standard C headers like stdio.h and stdlib.h are no longer included, you should include them directly in your project if you use non-SDL C runtime functions.
M_PI is no longer defined in SDL_stdinc.h, you can use the new symbols SDL_PI_D (double) and SDL_PI_F (float) instead.

bool is now defined as bool, and is 1 byte instead of the size of an int.

SDL3 attempts to apply consistency to case-insensitive string functions. In SDL2, things like SDL_strcasecmp() would usually only work on English letters, and depending on the user's locale, possibly not even those. In SDL3, consistency is applied:

- Many things that don't care about case-insensitivity, like SDL_strcmp(), continue to work with any null-terminated string of bytes, even if it happens to be malformed UTF-8.
- SDL_strcasecmp() expects valid UTF-8 strings, and will attempt to support _most_ Unicode characters with a technique known as "case-folding," which is to say it can match 'A' and 'a', and also 'Η' and 'η', but ALSO 'ß' and "ss". This is _probably_ how most apps assumed it worked in SDL2 and won't need any changes.
- SDL_strncasecmp() works the same, but the third parameter takes _bytes_, as before, so SDL_strlen() can continue to be used with it. If a string hits the limit in the middle of a codepoint, the half-processed bytes of the codepoint will be treated as a collection of U+0xFFFD (REPLACEMENT CHARACTER) codepoints, which you probably don't want.
- SDL_wcscasecmp() and SDL_wcsncasecmp() work the same way but operate on UTF-16 or UTF-32 encoded strings, depending on what the platform considers "wchar_t" to be. SDL_wcsncasecmp's third parameter is number of wchar_t values, not bytes, but UTF-16 has the same concerns as UTF-8 for variable-length codepoints.
- SDL_strcasestr() expects valid UTF-8 strings, and will compare codepoints using case-folding.
- SDL_tolower() and SDL_toupper() continue to only work on single bytes (even though the parameter is an `int`) and _only_ converts low-ASCII English A through Z.
- SDL_strlwr() and SDL_strupr() operates on individual bytes (not UTF-8 codepoints) and only change low-ASCII English 'A' through 'Z'. These functions do not check the string for valid UTF-8 encoding.
- The ctype.h replacement SDL_is*() functions (SDL_isalpha, SDL_isdigit, etc) only work on low-ASCII characters and ignore user locale, assuming English. This makes these functions consistent in SDL3, but applications need to be careful to understand their limits.

Please note that the case-folding technique used by SDL3 will not produce correct results for the "Turkish 'I'"; this one letter is a surprisingly hard problem in the Unicode world, and since these functions do not specify the human language in use, we have chosen to ignore this problem.

SDL_strtoll(), SDL_strtoull(), SDL_lltoa(), and SDL_ulltoa() use long long values instead of 64-bit values, to match their C runtime counterparts.

SDL_setenv() is not thread-safe and has been renamed SDL_setenv_unsafe().

The following macros have been removed:
* SDL_TABLESIZE() - use SDL_arraysize() instead

The following functions have been renamed:
* SDL_size_add_overflow() => SDL_size_add_check_overflow(), returns bool
* SDL_size_mul_overflow() => SDL_size_mul_check_overflow(), returns bool
* SDL_strtokr() => SDL_strtok_r()

The following functions have been removed:
* SDL_memcpy4()

The following symbols have been renamed:
* SDL_FALSE => false
* SDL_TRUE => true
* SDL_bool => bool

## SDL_surface.h

SDL_Surface has been simplified and internal details are no longer in the public structure.

The `format` member of SDL_Surface is now an enumerated pixel format value. You can get the full details of the pixel format by calling `SDL_GetPixelFormatDetails(surface->format)`. You can get the palette associated with the surface by calling SDL_GetSurfacePalette(). You can get the clip rectangle by calling SDL_GetSurfaceClipRect().

The userdata member of SDL_Surface has been replaced with a more general properties interface, which can be queried with SDL_GetSurfaceProperties()

Indexed format surfaces no longer have a palette by default. Surfaces without a palette will copy the pixels untranslated between surfaces.

Code that used to look like this:
```c
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 8, SDL_PIXELFORMAT_INDEX8);
    SDL_Palette *palette = surface->format->palette;
    ...
```
should be changed to:
```c
    SDL_Surface *surface = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_INDEX8);
    SDL_Palette *palette = SDL_CreateSurfacePalette(surface);
    ...
```

Removed the unused 'flags' parameter from SDL_ConvertSurface.

SDL_CreateRGBSurface() and SDL_CreateRGBSurfaceWithFormat() have been combined into a new function SDL_CreateSurface().
SDL_CreateRGBSurfaceFrom() and SDL_CreateRGBSurfaceWithFormatFrom() have been combined into a new function SDL_CreateSurfaceFrom(), and the parameter order has changed for consistency with SDL_CreateSurface().

You can implement the old functions in your own code easily:
```c
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    return SDL_CreateSurface(width, height,
            SDL_GetPixelFormatForMasks(depth, Rmask, Gmask, Bmask, Amask));
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, Uint32 format)
{
    return SDL_CreateSurface(width, height, format);
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    return SDL_CreateSurfaceFrom(width, height,
                                 SDL_GetPixelFormatForMasks(depth, Rmask, Gmask, Bmask, Amask),
                                 pixels, pitch);
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int width, int height, int depth, int pitch, Uint32 format)
{
    return SDL_CreateSurfaceFrom(width, height, format, pixels, pitch);
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

SDL_BlitSurface() and SDL_BlitSurfaceScaled() now have a const `dstrect` parameter and do not fill it in with the final destination rectangle.

SDL_BlitSurfaceScaled() and SDL_BlitSurfaceUncheckedScaled() now take a scale parameter.

SDL_SoftStretch() now takes a scale parameter.

SDL_PixelFormat is used instead of Uint32 for API functions that refer to pixel format by enumerated value.

SDL_SetSurfaceColorKey() takes an bool to enable and disable colorkey. RLE acceleration isn't controlled by the parameter, you should use SDL_SetSurfaceRLE() to change that separately.

SDL_SetSurfaceRLE() takes an bool to enable and disable RLE acceleration.

The following functions have been renamed:
* SDL_BlitScaled() => SDL_BlitSurfaceScaled(), returns bool
* SDL_ConvertSurfaceFormat() => SDL_ConvertSurface()
* SDL_FillRect() => SDL_FillSurfaceRect(), returns bool
* SDL_FillRects() => SDL_FillSurfaceRects(), returns bool
* SDL_FreeSurface() => SDL_DestroySurface()
* SDL_GetClipRect() => SDL_GetSurfaceClipRect(), returns bool
* SDL_GetColorKey() => SDL_GetSurfaceColorKey(), returns bool
* SDL_HasColorKey() => SDL_SurfaceHasColorKey()
* SDL_HasSurfaceRLE() => SDL_SurfaceHasRLE()
* SDL_LoadBMP_RW() => SDL_LoadBMP_IO()
* SDL_LowerBlit() => SDL_BlitSurfaceUnchecked(), returns bool
* SDL_LowerBlitScaled() => SDL_BlitSurfaceUncheckedScaled(), returns bool
* SDL_SaveBMP_RW() => SDL_SaveBMP_IO(), returns bool
* SDL_SetClipRect() => SDL_SetSurfaceClipRect()
* SDL_SetColorKey() => SDL_SetSurfaceColorKey(), returns bool
* SDL_UpperBlit() => SDL_BlitSurface(), returns bool
* SDL_UpperBlitScaled() => SDL_BlitSurfaceScaled(), returns bool

The following symbols have been removed:
* SDL_SWSURFACE

The following functions have been removed:
* SDL_FreeFormat()
* SDL_GetYUVConversionMode()
* SDL_GetYUVConversionModeForResolution()
* SDL_SetYUVConversionMode() - use SDL_SetSurfaceColorspace() to set the surface colorspace and SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER with SDL_CreateTextureWithProperties() to set the texture colorspace. The default colorspace for YUV pixel formats is SDL_COLORSPACE_JPEG.
* SDL_SoftStretch() - use SDL_BlitSurfaceScaled() with SDL_SCALEMODE_NEAREST
* SDL_SoftStretchLinear() - use SDL_BlitSurfaceScaled() with SDL_SCALEMODE_LINEAR

The following symbols have been renamed:
* SDL_PREALLOC => SDL_SURFACE_PREALLOCATED
* SDL_SIMD_ALIGNED => SDL_SURFACE_SIMD_ALIGNED

## SDL_system.h

SDL_WindowsMessageHook has changed signatures so the message may be modified and it can block further message processing.

SDL_RequestAndroidPermission is no longer a blocking call; the caller now provides a callback function that fires when a response is available.

SDL_iPhoneSetAnimationCallback() and SDL_iPhoneSetEventPump() have been renamed to SDL_SetiOSAnimationCallback() and SDL_SetiOSEventPump(), respectively. SDL2 has had macros to provide this new name with the old symbol since the introduction of the iPad, but now the correctly-named symbol is the only option.

The following functions have been removed:
* SDL_GetWinRTFSPathUNICODE() - WinRT support was removed in SDL3.
* SDL_GetWinRTFSPathUTF8() - WinRT support was removed in SDL3.
* SDL_RenderGetD3D11Device() - replaced with the "SDL.renderer.d3d11.device" property
* SDL_RenderGetD3D12Device() - replaced with the "SDL.renderer.d3d12.device" property
* SDL_RenderGetD3D9Device() - replaced with the "SDL.renderer.d3d9.device" property
* SDL_WinRTGetDeviceFamily() - WinRT support was removed in SDL3.

The following functions have been renamed:
* SDL_AndroidBackButton() => SDL_SendAndroidBackButton()
* SDL_AndroidGetActivity() => SDL_GetAndroidActivity()
* SDL_AndroidGetExternalStoragePath() => SDL_GetAndroidExternalStoragePath()
* SDL_AndroidGetExternalStorageState() => SDL_GetAndroidExternalStorageState()
* SDL_AndroidGetInternalStoragePath() => SDL_GetAndroidInternalStoragePath()
* SDL_AndroidGetJNIEnv() => SDL_GetAndroidJNIEnv()
* SDL_AndroidRequestPermission() => SDL_RequestAndroidPermission(), returns bool
* SDL_AndroidRequestPermissionCallback() => SDL_RequestAndroidPermissionCallback()
* SDL_AndroidSendMessage() => SDL_SendAndroidMessage(), returns bool
* SDL_AndroidShowToast() => SDL_ShowAndroidToast(), returns bool
* SDL_DXGIGetOutputInfo() => SDL_GetDXGIOutputInfo(), returns bool
* SDL_Direct3D9GetAdapterIndex() => SDL_GetDirect3D9AdapterIndex()
* SDL_GDKGetDefaultUser() => SDL_GetGDKDefaultUser(), returns bool
* SDL_GDKGetTaskQueue() => SDL_GetGDKTaskQueue(), returns bool
* SDL_LinuxSetThreadPriority() => SDL_SetLinuxThreadPriority(), returns bool
* SDL_LinuxSetThreadPriorityAndPolicy() => SDL_SetLinuxThreadPriorityAndPolicy(), returns bool
* SDL_OnApplicationDidBecomeActive() => SDL_OnApplicationDidEnterForeground()
* SDL_OnApplicationWillResignActive() => SDL_OnApplicationWillEnterBackground()
* SDL_iOSSetAnimationCallback() => SDL_SetiOSAnimationCallback(), returns bool
* SDL_iOSSetEventPump() => SDL_SetiOSEventPump()
* SDL_iPhoneSetAnimationCallback() => SDL_SetiOSAnimationCallback(), returns bool
* SDL_iPhoneSetEventPump() => SDL_SetiOSEventPump()

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
#elif defined(__LINUX__)
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
#elif defined(__IPHONEOS__)
    UIWindow *uiwindow = NULL;
    if (SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_UIKIT) {
        uiwindow = (__bridge UIWindow *)info.info.uikit.window;
    }
    if (uiwindow) {
        GLuint framebuffer = info.info.uikit.framebuffer;
        GLuint colorbuffer = info.info.uikit.colorbuffer;
        GLuint resolveFramebuffer = info.info.uikit.resolveFramebuffer;
        ...
    }
#endif
```
becomes:
```c
#if defined(SDL_PLATFORM_WIN32)
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (hwnd) {
        ...
    }
#elif defined(SDL_PLATFORM_MACOS)
    NSWindow *nswindow = (__bridge NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    if (nswindow) {
        ...
    }
#elif defined(SDL_PLATFORM_LINUX)
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        Display *xdisplay = (Display *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        Window xwindow = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (xdisplay && xwindow) {
            ...
        }
    } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        struct wl_display *display = (struct wl_display *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
        struct wl_surface *surface = (struct wl_surface *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
        if (display && surface) {
            ...
        }
    }
#elif defined(SDL_PLATFORM_IOS)
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    UIWindow *uiwindow = (__bridge UIWindow *)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    if (uiwindow) {
        GLuint framebuffer = (GLuint)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER, 0);
        GLuint colorbuffer = (GLuint)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_UIKIT_OPENGL_RENDERBUFFER_NUMBER, 0);
        GLuint resolveFramebuffer = (GLuint)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_UIKIT_OPENGL_RESOLVE_FRAMEBUFFER_NUMBER, 0);
        ...
    }
#endif
```

## SDL_thread.h

SDL_CreateThreadWithStackSize has been replaced with SDL_CreateThreadWithProperties.

SDL_CreateThread and SDL_CreateThreadWithProperties now take beginthread/endthread function pointers on all platforms (ignoring them on most), and have been replaced with macros that hide this detail on all platforms. This works the same as before at the source code level, but the actual function signature that is called in SDL has changed. The library's exported symbol is SDL_CreateThreadRuntime, and looking for "SDL_CreateThread" in the DLL/Shared Library/Dylib will fail. You should not call this directly, but instead always use the macro!

SDL_GetTLS() and SDL_SetTLS() take a pointer to a TLS ID, and will automatically initialize it in a thread-safe way as needed.

The following functions have been renamed:
* SDL_TLSCleanup() => SDL_CleanupTLS()
* SDL_TLSGet() => SDL_GetTLS()
* SDL_TLSSet() => SDL_SetTLS(), returns bool
* SDL_ThreadID() => SDL_GetCurrentThreadID()

The following functions have been removed:
* SDL_TLSCreate() - TLS IDs are automatically allocated as needed.

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

The callback passed to SDL_AddTimer() has changed parameters to:
```c
Uint32 SDLCALL TimerCallback(void *userdata, SDL_TimerID timerID, Uint32 interval);
````

## SDL_touch.h

SDL_GetTouchName is replaced with SDL_GetTouchDeviceName(), which takes an SDL_TouchID instead of an index.

SDL_TouchID and SDL_FingerID are now Uint64 with 0 being an invalid value.

Rather than iterating over touch devices using an index, there is a new function SDL_GetTouchDevices() to get the available devices.

Rather than iterating over touch fingers using an index, there is a new function SDL_GetTouchFingers() to get the current set of active fingers.

The following functions have been removed:
* SDL_GetNumTouchDevices() - replaced with SDL_GetTouchDevices()
* SDL_GetNumTouchFingers() - replaced with SDL_GetTouchFingers()
* SDL_GetTouchDevice() - replaced with SDL_GetTouchDevices()
* SDL_GetTouchFinger() - replaced with SDL_GetTouchFingers()


## SDL_version.h

SDL_GetRevisionNumber() has been removed from the API, it always returned 0 in SDL 2.0.

SDL_GetVersion() returns the version number, which can be directly compared with another version wrapped with SDL_VERSIONNUM().

The following structures have been removed:
* SDL_version

The following symbols have been renamed:
* SDL_COMPILEDVERSION => SDL_VERSION
* SDL_PATCHLEVEL => SDL_MICRO_VERSION

## SDL_video.h

Several video backends have had their names lower-cased ("kmsdrm", "rpi", "android", "psp", "ps2", "vita"). SDL already does a case-insensitive compare for SDL_HINT_VIDEO_DRIVER tests, but if your app is calling SDL_GetVideoDriver() or SDL_GetCurrentVideoDriver() and doing case-sensitive compares on those strings, please update your code.

SDL_VideoInit() and SDL_VideoQuit() have been removed. Instead you can call SDL_InitSubSystem() and SDL_QuitSubSystem() with SDL_INIT_VIDEO, which will properly refcount the subsystems. You can choose a specific video driver using SDL_HINT_VIDEO_DRIVER.

Rather than iterating over displays using display index, there is a new function SDL_GetDisplays() to get the current list of displays, and functions which used to take a display index now take SDL_DisplayID, with an invalid ID being 0.
```c
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
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

The refresh rate in SDL_DisplayMode is now a float, as well as being represented as a precise fraction with numerator and denominator.

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

SDL_GL_DeleteContext() has been renamed to SDL_GL_DestroyContext to match SDL naming conventions (and glX/EGL!).

SDL_GL_GetSwapInterval() takes the interval as an output parameter and returns true if the function succeeds or false if there was an error.

SDL_GL_GetDrawableSize() has been removed. SDL_GetWindowSizeInPixels() can be used in its place.

The SDL_WINDOW_TOOLTIP and SDL_WINDOW_POPUP_MENU window flags are now supported on Windows, Mac (Cocoa), X11, and Wayland. Creating windows with these flags must happen via the `SDL_CreatePopupWindow()` function. This function requires passing in the handle to a valid parent window for the popup, and the popup window is positioned relative to the parent.

SDL_WindowFlags is used instead of Uint32 for API functions that refer to window flags, and has been extended to 64 bits.

SDL_GetWindowOpacity() directly returns the opacity instead of using an out parameter.

The following functions have been renamed:
* SDL_GL_DeleteContext() => SDL_GL_DestroyContext(), returns bool
* SDL_GetClosestDisplayMode() => SDL_GetClosestFullscreenDisplayMode(), returns bool
* SDL_GetDisplayOrientation() => SDL_GetCurrentDisplayOrientation()
* SDL_GetPointDisplayIndex() => SDL_GetDisplayForPoint()
* SDL_GetRectDisplayIndex() => SDL_GetDisplayForRect()
* SDL_GetWindowDisplayIndex() => SDL_GetDisplayForWindow()
* SDL_GetWindowDisplayMode() => SDL_GetWindowFullscreenMode()
* SDL_HasWindowSurface() => SDL_WindowHasSurface()
* SDL_IsScreenSaverEnabled() => SDL_ScreenSaverEnabled()
* SDL_SetWindowDisplayMode() => SDL_SetWindowFullscreenMode(), returns bool

The following functions have been removed:
* SDL_GetClosestFullscreenDisplayMode()
* SDL_GetDisplayDPI() - not reliable across platforms, approximately replaced by multiplying SDL_GetWindowDisplayScale() times 160 on iPhone and Android, and 96 on other platforms.
* SDL_GetDisplayMode()
* SDL_GetNumDisplayModes() - replaced with SDL_GetFullscreenDisplayModes()
* SDL_GetNumVideoDisplays() - replaced with SDL_GetDisplays()
* SDL_SetWindowGrab() - use SDL_SetWindowMouseGrab() instead, along with SDL_SetWindowKeyboardGrab() if you also set SDL_HINT_GRAB_KEYBOARD.
* SDL_GetWindowGrab() - use SDL_GetWindowMouseGrab() instead, along with SDL_GetWindowKeyboardGrab() if you also set SDL_HINT_GRAB_KEYBOARD.
* SDL_GetWindowData() - use SDL_GetWindowProperties() instead
* SDL_SetWindowData() - use SDL_GetWindowProperties() instead
* SDL_CreateWindowFrom() - use SDL_CreateWindowWithProperties() with the properties that allow you to wrap an existing window
* SDL_SetWindowInputFocus() - use SDL_RaiseWindow() instead
* SDL_SetWindowModalFor() - use SDL_SetWindowParent() with SDL_SetWindowModal() instead

The SDL_Window id type is named SDL_WindowID

The following symbols have been renamed:
* SDL_DISPLAYEVENT_DISCONNECTED => SDL_EVENT_DISPLAY_REMOVED
* SDL_DISPLAYEVENT_MOVED => SDL_EVENT_DISPLAY_MOVED
* SDL_DISPLAYEVENT_ORIENTATION => SDL_EVENT_DISPLAY_ORIENTATION
* SDL_WINDOWEVENT_CLOSE => SDL_EVENT_WINDOW_CLOSE_REQUESTED
* SDL_WINDOWEVENT_DISPLAY_CHANGED => SDL_EVENT_WINDOW_DISPLAY_CHANGED
* SDL_WINDOWEVENT_ENTER => SDL_EVENT_WINDOW_MOUSE_ENTER
* SDL_WINDOWEVENT_EXPOSED => SDL_EVENT_WINDOW_EXPOSED
* SDL_WINDOWEVENT_FOCUS_GAINED => SDL_EVENT_WINDOW_FOCUS_GAINED
* SDL_WINDOWEVENT_FOCUS_LOST => SDL_EVENT_WINDOW_FOCUS_LOST
* SDL_WINDOWEVENT_HIDDEN => SDL_EVENT_WINDOW_HIDDEN
* SDL_WINDOWEVENT_HIT_TEST => SDL_EVENT_WINDOW_HIT_TEST
* SDL_WINDOWEVENT_ICCPROF_CHANGED => SDL_EVENT_WINDOW_ICCPROF_CHANGED
* SDL_WINDOWEVENT_LEAVE => SDL_EVENT_WINDOW_MOUSE_LEAVE
* SDL_WINDOWEVENT_MAXIMIZED => SDL_EVENT_WINDOW_MAXIMIZED
* SDL_WINDOWEVENT_MINIMIZED => SDL_EVENT_WINDOW_MINIMIZED
* SDL_WINDOWEVENT_MOVED => SDL_EVENT_WINDOW_MOVED
* SDL_WINDOWEVENT_RESIZED => SDL_EVENT_WINDOW_RESIZED
* SDL_WINDOWEVENT_RESTORED => SDL_EVENT_WINDOW_RESTORED
* SDL_WINDOWEVENT_SHOWN => SDL_EVENT_WINDOW_SHOWN
* SDL_WINDOWEVENT_SIZE_CHANGED => SDL_EVENT_WINDOW_SIZE_CHANGED
* SDL_WINDOW_ALLOW_HIGHDPI => SDL_WINDOW_HIGH_PIXEL_DENSITY
* SDL_WINDOW_INPUT_GRABBED => SDL_WINDOW_MOUSE_GRABBED

The following symbols have been removed:
* SDL_WINDOWEVENT_TAKE_FOCUS

The following window operations are now considered to be asynchronous requests and should not be assumed to succeed unless
a corresponding event has been received:
* SDL_SetWindowSize() (SDL_EVENT_WINDOW_RESIZED)
* SDL_SetWindowPosition() (SDL_EVENT_WINDOW_MOVED)
* SDL_MinimizeWindow() (SDL_EVENT_WINDOW_MINIMIZED)
* SDL_MaximizeWindow() (SDL_EVENT_WINDOW_MAXIMIZED)
* SDL_RestoreWindow() (SDL_EVENT_WINDOW_RESTORED)
* SDL_SetWindowFullscreen() (SDL_EVENT_WINDOW_ENTER_FULLSCREEN / SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)

If it is required that operations be applied immediately after one of the preceding calls, the `SDL_SyncWindow()` function
will attempt to wait until all pending window operations have completed. The `SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS` hint
can also be set to automatically synchronize after all calls to an asynchronous window operation, mimicking the behavior
of SDL 2. Be aware that synchronizing can potentially block for long periods of time, as it may have to wait for window
animations to complete. Also note that windowing systems can deny or not precisely obey these requests (e.g. windows may
not be allowed to be larger than the usable desktop space or placed offscreen), so a corresponding event may never arrive
or not contain the expected values.

## SDL_vulkan.h

SDL_Vulkan_GetInstanceExtensions() no longer takes a window parameter, and no longer makes the app allocate query/allocate space for the result, instead returning a static const internal string.

SDL_Vulkan_GetVkGetInstanceProcAddr() now returns `SDL_FunctionPointer` instead of `void *`, and should be cast to PFN_vkGetInstanceProcAddr.

SDL_Vulkan_CreateSurface() now takes a VkAllocationCallbacks pointer as its third parameter. If you don't have an allocator to supply, pass a NULL here to use the system default allocator (SDL2 always used the system default allocator here).

SDL_Vulkan_GetDrawableSize() has been removed. SDL_GetWindowSizeInPixels() can be used in its place.

SDL_vulkanInstance and SDL_vulkanSurface have been removed. They were for compatibility with Tizen, who had built their own Vulkan interface into SDL2, but these apps will need changes for the SDL3 API if they are upgraded anyhow.
