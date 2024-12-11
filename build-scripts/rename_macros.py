#!/usr/bin/env python3
#
# This script renames SDL macros in the specified paths

import argparse
import pathlib
import re


class TextReplacer:
    def __init__(self, macros, repl_format):
        if isinstance(macros, dict):
            macros_keys = macros.keys()
        else:
            macros_keys = macros
        self.macros = macros
        self.re_macros = re.compile(r"\W(" + "|".join(macros_keys) + r")(?:\W|$)")
        self.repl_format = repl_format

    def apply(self, contents):
        def cb(m):
            macro = m.group(1)
            original = m.group(0)
            match_start, _ = m.span(0)
            platform_start, platform_end = m.span(1)
            if isinstance(self.macros, dict):
                repl_args = (macro, self.macros[macro])
            else:
                repl_args = macro,
            new_text = self.repl_format.format(*repl_args)
            r = original[:(platform_start-match_start)] + new_text + original[platform_end-match_start:]
            return r
        contents, _ = self.re_macros.subn(cb, contents)

        return contents


class MacrosCheck:
    def __init__(self):
        self.renamed_platform_macros = TextReplacer(RENAMED_MACROS, "{1}")
        self.deprecated_platform_macros = TextReplacer(DEPRECATED_PLATFORM_MACROS, "{0} /* {0} has been removed in SDL3 */")

    def run(self, contents):
        contents = self.renamed_platform_macros.apply(contents)
        contents = self.deprecated_platform_macros.apply(contents)
        return contents


def apply_checks(paths):
    checks = (
        MacrosCheck(),
    )

    for entry in paths:
        path = pathlib.Path(entry)
        if not path.exists():
            print("{} does not exist, skipping".format(entry))
            continue
        apply_checks_in_path(path, checks)


def apply_checks_in_file(file, checks):
    try:
        with file.open("r", encoding="UTF-8", newline="") as rfp:
            original = rfp.read()
            contents = original
            for check in checks:
                contents = check.run(contents)
            if contents != original:
                with file.open("w", encoding="UTF-8", newline="") as wfp:
                    wfp.write(contents)
    except UnicodeDecodeError:
        print("%s is not text, skipping" % file)
    except Exception as err:
        print("%s" % err)


def apply_checks_in_dir(path, checks):
    for entry in path.glob("*"):
        if entry.is_dir():
            apply_checks_in_dir(entry, checks)
        else:
            print("Processing %s" % entry)
            apply_checks_in_file(entry, checks)


def apply_checks_in_path(path, checks):
        if path.is_dir():
            apply_checks_in_dir(path, checks)
        else:
            apply_checks_in_file(path, checks)


def main():
    parser = argparse.ArgumentParser(fromfile_prefix_chars='@', description="Rename macros for SDL3")
    parser.add_argument("args", nargs="*", help="Input source files")
    args = parser.parse_args()

    try:
        apply_checks(args.args)
    except Exception as e:
        print(e)
        return 1


RENAMED_MACROS = {
    "__AIX__": "SDL_PLATFORM_AIX",
    "__HAIKU__": "SDL_PLATFORM_HAIKU",
    "__BSDI__": "SDL_PLATFORM_BSDI",
    "__FREEBSD__": "SDL_PLATFORM_FREEBSD",
    "__HPUX__": "SDL_PLATFORM_HPUX",
    "__IRIX__": "SDL_PLATFORM_IRIX",
    "__LINUX__": "SDL_PLATFORM_LINUX",
    "__OS2__": "SDL_PLATFORM_OS2",
    # "__ANDROID__": "SDL_PLATFORM_ANDROID,
    "__APPLE__": "SDL_PLATFORM_APPLE",
    "__TVOS__": "SDL_PLATFORM_TVOS",
    "__IPHONEOS__": "SDL_PLATFORM_IOS",
    "__MACOSX__": "SDL_PLATFORM_MACOS",
    "__NETBSD__": "SDL_PLATFORM_NETBSD",
    "__OPENBSD__": "SDL_PLATFORM_OPENBSD",
    "__OSF__": "SDL_PLATFORM_OSF",
    "__QNXNTO__": "SDL_PLATFORM_QNXNTO",
    "__RISCOS__": "SDL_PLATFORM_RISCOS",
    "__SOLARIS__": "SDL_PLATFORM_SOLARIS",
    "__PSP__": "SDL_PLATFORM_PSP",
    "__PS2__": "SDL_PLATFORM_PS2",
    "__VITA__": "SDL_PLATFORM_VITA",
    "__3DS__": "SDL_PLATFORM_3DS",
    # "__unix__": "SDL_PLATFORM_UNIX,
    "__XBOXSERIES__": "SDL_PLATFORM_XBOXSERIES",
    "__XBOXONE__": "SDL_PLATFORM_XBOXONE",
    "__WINDOWS__": "SDL_PLATFORM_WINDOWS",
    "__WIN32__": "SDL_PLATFORM_WIN32",
    # "__CYGWIN_": "SDL_PLATFORM_CYGWIN",
    "__WINGDK__": "SDL_PLATFORM_WINGDK",
    "__GDK__": "SDL_PLATFORM_GDK",
    # "__EMSCRIPTEN__": "SDL_PLATFORM_EMSCRIPTEN",
}

DEPRECATED_PLATFORM_MACROS = {
    "__DREAMCAST__",
    "__NACL__",
    "__PNACL__",
    "__WINDOWS__",
    "__WINRT__",
    "SDL_ALTIVEC_BLITTERS",
    "SDL_ARM_NEON_BLITTERS",
    "SDL_ARM_SIMD_BLITTERS",
    "SDL_ATOMIC_DISABLED",
    "SDL_AUDIO_DISABLED",
    "SDL_AUDIO_DRIVER_AAUDIO",
    "SDL_AUDIO_DRIVER_ALSA",
    "SDL_AUDIO_DRIVER_ALSA_DYNAMIC",
    "SDL_AUDIO_DRIVER_ANDROID",
    "SDL_AUDIO_DRIVER_ARTS",
    "SDL_AUDIO_DRIVER_ARTS_DYNAMIC",
    "SDL_AUDIO_DRIVER_COREAUDIO",
    "SDL_AUDIO_DRIVER_DISK",
    "SDL_AUDIO_DRIVER_DSOUND",
    "SDL_AUDIO_DRIVER_DUMMY",
    "SDL_AUDIO_DRIVER_EMSCRIPTEN",
    "SDL_AUDIO_DRIVER_ESD",
    "SDL_AUDIO_DRIVER_ESD_DYNAMIC",
    "SDL_AUDIO_DRIVER_FUSIONSOUND",
    "SDL_AUDIO_DRIVER_FUSIONSOUND_DYNAMIC",
    "SDL_AUDIO_DRIVER_HAIKU",
    "SDL_AUDIO_DRIVER_JACK",
    "SDL_AUDIO_DRIVER_JACK_DYNAMIC",
    "SDL_AUDIO_DRIVER_N3DS",
    "SDL_AUDIO_DRIVER_NAS",
    "SDL_AUDIO_DRIVER_NAS_DYNAMIC",
    "SDL_AUDIO_DRIVER_NETBSD",
    "SDL_AUDIO_DRIVER_OPENSLES",
    "SDL_AUDIO_DRIVER_OS2",
    "SDL_AUDIO_DRIVER_OSS",
    "SDL_AUDIO_DRIVER_PAUDIO",
    "SDL_AUDIO_DRIVER_PIPEWIRE",
    "SDL_AUDIO_DRIVER_PIPEWIRE_DYNAMIC",
    "SDL_AUDIO_DRIVER_PS2",
    "SDL_AUDIO_DRIVER_PSP",
    "SDL_AUDIO_DRIVER_PULSEAUDIO",
    "SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC",
    "SDL_AUDIO_DRIVER_QSA",
    "SDL_AUDIO_DRIVER_SNDIO",
    "SDL_AUDIO_DRIVER_SNDIO_DYNAMIC",
    "SDL_AUDIO_DRIVER_SUNAUDIO",
    "SDL_AUDIO_DRIVER_VITA",
    "SDL_AUDIO_DRIVER_WASAPI",
    "SDL_AUDIO_DRIVER_WINMM",
    "SDL_CPUINFO_DISABLED",
    "SDL_DEFAULT_ASSERT_LEVEL",
    "SDL_EVENTS_DISABLED",
    "SDL_FILESYSTEM_ANDROID",
    "SDL_FILESYSTEM_COCOA",
    "SDL_FILESYSTEM_DISABLED",
    "SDL_FILESYSTEM_DUMMY",
    "SDL_FILESYSTEM_EMSCRIPTEN",
    "SDL_FILESYSTEM_HAIKU",
    "SDL_FILESYSTEM_N3DS",
    "SDL_FILESYSTEM_OS2",
    "SDL_FILESYSTEM_PS2",
    "SDL_FILESYSTEM_PSP",
    "SDL_FILESYSTEM_RISCOS",
    "SDL_FILESYSTEM_UNIX",
    "SDL_FILESYSTEM_VITA",
    "SDL_FILESYSTEM_WINDOWS",
    "SDL_FILE_DISABLED",
    "SDL_HAPTIC_ANDROID",
    "SDL_HAPTIC_DINPUT",
    "SDL_HAPTIC_DISABLED",
    "SDL_HAPTIC_DUMMY",
    "SDL_HAPTIC_IOKIT",
    "SDL_HAPTIC_LINUX",
    "SDL_HAPTIC_XINPUT",
    "SDL_HAVE_LIBDECOR_GET_MIN_MAX",
    "SDL_HAVE_MACHINE_JOYSTICK_H",
    "SDL_HIDAPI_DISABLED",
    "SDL_INPUT_FBSDKBIO",
    "SDL_INPUT_LINUXEV",
    "SDL_INPUT_LINUXKD",
    "SDL_INPUT_WSCONS",
    "SDL_IPHONE_KEYBOARD",
    "SDL_IPHONE_LAUNCHSCREEN",
    "SDL_JOYSTICK_ANDROID",
    "SDL_JOYSTICK_DINPUT",
    "SDL_JOYSTICK_DISABLED",
    "SDL_JOYSTICK_DUMMY",
    "SDL_JOYSTICK_EMSCRIPTEN",
    "SDL_JOYSTICK_HAIKU",
    "SDL_JOYSTICK_HIDAPI",
    "SDL_JOYSTICK_IOKIT",
    "SDL_JOYSTICK_LINUX",
    "SDL_JOYSTICK_MFI",
    "SDL_JOYSTICK_N3DS",
    "SDL_JOYSTICK_OS2",
    "SDL_JOYSTICK_PS2",
    "SDL_JOYSTICK_PSP",
    "SDL_JOYSTICK_RAWINPUT",
    "SDL_JOYSTICK_USBHID",
    "SDL_JOYSTICK_VIRTUAL",
    "SDL_JOYSTICK_VITA",
    "SDL_JOYSTICK_WGI",
    "SDL_JOYSTICK_XINPUT",
    "SDL_LIBSAMPLERATE_DYNAMIC",
    "SDL_LIBUSB_DYNAMIC",
    "SDL_LOADSO_DISABLED",
    "SDL_LOADSO_DLOPEN",
    "SDL_LOADSO_DUMMY",
    "SDL_LOADSO_LDG",
    "SDL_LOADSO_OS2",
    "SDL_LOADSO_WINDOWS",
    "SDL_LOCALE_DISABLED",
    "SDL_LOCALE_DUMMY",
    "SDL_MISC_DISABLED",
    "SDL_MISC_DUMMY",
    "SDL_POWER_ANDROID",
    "SDL_POWER_DISABLED",
    "SDL_POWER_EMSCRIPTEN",
    "SDL_POWER_HAIKU",
    "SDL_POWER_HARDWIRED",
    "SDL_POWER_LINUX",
    "SDL_POWER_MACOSX",
    "SDL_POWER_N3DS",
    "SDL_POWER_PSP",
    "SDL_POWER_UIKIT",
    "SDL_POWER_VITA",
    "SDL_POWER_WINDOWS",
    "SDL_POWER_WINRT",
    "SDL_RENDER_DISABLED",
    "SDL_SENSOR_ANDROID",
    "SDL_SENSOR_COREMOTION",
    "SDL_SENSOR_DISABLED",
    "SDL_SENSOR_DUMMY",
    "SDL_SENSOR_N3DS",
    "SDL_SENSOR_VITA",
    "SDL_SENSOR_WINDOWS",
    "SDL_THREADS_DISABLED",
    "SDL_THREAD_GENERIC_COND_SUFFIX",
    "SDL_THREAD_N3DS",
    "SDL_THREAD_OS2",
    "SDL_THREAD_PS2",
    "SDL_THREAD_PSP",
    "SDL_THREAD_PTHREAD",
    "SDL_THREAD_PTHREAD_RECURSIVE_MUTEX",
    "SDL_THREAD_PTHREAD_RECURSIVE_MUTEX_NP",
    "SDL_THREAD_VITA",
    "SDL_THREAD_WINDOWS",
    "SDL_TIMERS_DISABLED",
    "SDL_TIMER_DUMMY",
    "SDL_TIMER_HAIKU",
    "SDL_TIMER_N3DS",
    "SDL_TIMER_OS2",
    "SDL_TIMER_PS2",
    "SDL_TIMER_PSP",
    "SDL_TIMER_UNIX",
    "SDL_TIMER_VITA",
    "SDL_TIMER_WINDOWS",
    "SDL_UDEV_DYNAMIC",
    "SDL_USE_IME",
    "SDL_USE_LIBICONV",
    "SDL_VIDEO_DISABLED",
    "SDL_VIDEO_DRIVER_ANDROID",
    "SDL_VIDEO_DRIVER_COCOA",
    "SDL_VIDEO_DRIVER_DIRECTFB",
    "SDL_VIDEO_DRIVER_DIRECTFB_DYNAMIC",
    "SDL_VIDEO_DRIVER_DUMMY",
    "SDL_VIDEO_DRIVER_EMSCRIPTEN",
    "SDL_VIDEO_DRIVER_HAIKU",
    "SDL_VIDEO_DRIVER_KMSDRM",
    "SDL_VIDEO_DRIVER_KMSDRM_DYNAMIC",
    "SDL_VIDEO_DRIVER_KMSDRM_DYNAMIC_GBM",
    "SDL_VIDEO_DRIVER_N3DS",
    "SDL_VIDEO_DRIVER_OFFSCREEN",
    "SDL_VIDEO_DRIVER_OS2",
    "SDL_VIDEO_DRIVER_PS2",
    "SDL_VIDEO_DRIVER_PSP",
    "SDL_VIDEO_DRIVER_QNX",
    "SDL_VIDEO_DRIVER_RISCOS",
    "SDL_VIDEO_DRIVER_RPI",
    "SDL_VIDEO_DRIVER_UIKIT",
    "SDL_VIDEO_DRIVER_VITA",
    "SDL_VIDEO_DRIVER_VIVANTE",
    "SDL_VIDEO_DRIVER_VIVANTE_VDK",
    "SDL_VIDEO_DRIVER_WAYLAND",
    "SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC",
    "SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_CURSOR",
    "SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_EGL",
    "SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_LIBDECOR",
    "SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_XKBCOMMON",
    "SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH",
    "SDL_VIDEO_DRIVER_WINDOWS",
    "SDL_VIDEO_DRIVER_WINRT",
    "SDL_VIDEO_DRIVER_X11",
    "SDL_VIDEO_DRIVER_X11_DYNAMIC",
    "SDL_VIDEO_DRIVER_X11_DYNAMIC_XCURSOR",
    "SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT",
    "SDL_VIDEO_DRIVER_X11_DYNAMIC_XFIXES",
    "SDL_VIDEO_DRIVER_X11_DYNAMIC_XINPUT2",
    "SDL_VIDEO_DRIVER_X11_DYNAMIC_XRANDR",
    "SDL_VIDEO_DRIVER_X11_DYNAMIC_XSS",
    "SDL_VIDEO_DRIVER_X11_HAS_XKBKEYCODETOKEYSYM",
    "SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS",
    "SDL_VIDEO_DRIVER_X11_XCURSOR",
    "SDL_VIDEO_DRIVER_X11_XDBE",
    "SDL_VIDEO_DRIVER_X11_XFIXES",
    "SDL_VIDEO_DRIVER_X11_XINPUT2",
    "SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH",
    "SDL_VIDEO_DRIVER_X11_XRANDR",
    "SDL_VIDEO_DRIVER_X11_XSCRNSAVER",
    "SDL_VIDEO_DRIVER_X11_XSHAPE",
    "SDL_VIDEO_METAL",
    "SDL_VIDEO_OPENGL",
    "SDL_VIDEO_OPENGL_BGL",
    "SDL_VIDEO_OPENGL_CGL",
    "SDL_VIDEO_OPENGL_EGL",
    "SDL_VIDEO_OPENGL_ES",
    "SDL_VIDEO_OPENGL_ES2",
    "SDL_VIDEO_OPENGL_GLX",
    "SDL_VIDEO_OPENGL_OSMESA",
    "SDL_VIDEO_OPENGL_OSMESA_DYNAMIC",
    "SDL_VIDEO_OPENGL_WGL",
    "SDL_VIDEO_RENDER_D3D",
    "SDL_VIDEO_RENDER_D3D11",
    "SDL_VIDEO_RENDER_D3D12",
    "SDL_VIDEO_RENDER_DIRECTFB",
    "SDL_VIDEO_RENDER_METAL",
    "SDL_VIDEO_RENDER_OGL",
    "SDL_VIDEO_RENDER_OGL_ES",
    "SDL_VIDEO_RENDER_OGL_ES2",
    "SDL_VIDEO_RENDER_PS2",
    "SDL_VIDEO_RENDER_PSP",
    "SDL_VIDEO_RENDER_VITA_GXM",
    "SDL_VIDEO_VITA_PIB",
    "SDL_VIDEO_VITA_PVR",
    "SDL_VIDEO_VITA_PVR_OGL",
    "SDL_VIDEO_VULKAN",
}

if __name__ == "__main__":
    raise SystemExit(main())

