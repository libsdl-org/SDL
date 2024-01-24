#!/usr/bin/env python3
#
# This script renames SDL macros in the specified paths

import argparse
import pathlib
import re


class PlatformMacrosCheck:
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
        "__NGAGE__": "SDL_PLATFORM_NGAGE",
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
        "__WINRT__": "SDL_PLATFORM_WINRT",
        "__XBOXSERIES__": "SDL_PLATFORM_XBOXSERIES",
        "__XBOXONE__": "SDL_PLATFORM_XBOXONE",
        "__WINDOWS__": "SDL_PLATFORM_WINDOWS",
        "__WIN32__": "SDL_PLATFORM_WIN32",
        # "__CYGWIN_": "SDL_PLATFORM_CYGWIN",
        "__WINGDK__": "SDL_PLATFORM_WINGDK",
        "__GDK__": "SDL_PLATFORM_GDK",
        # "__EMSCRIPTEN__": "SDL_PLATFORM_EMSCRIPTEN",
    }

    DEPRECATED_MACROS = {
        "__DREAMCAST__",
        "__NACL__",
        "__PNACL__",
        "__WINDOWS__",
    }

    def __init__(self):
        self.re_pp_command = re.compile(r"^[ \t]*#[ \t]*(\w+).*")
        self.re_platform_macros = re.compile(r"\W(" + "|".join(self.RENAMED_MACROS.keys()) + r")(?:\W|$)")
        self.re_deprecated_macros = re.compile(r"\W(" + "|".join(self.DEPRECATED_MACROS) + r")(?:\W|$)")

    def run(self, contents):
        def cb(m):
            macro = m.group(1)
            original = m.group(0)
            match_start, _ = m.span(0)
            platform_start, platform_end = m.span(1)
            new_text = self.RENAMED_MACROS[macro]
            r = original[:(platform_start-match_start)] + new_text + original[platform_end-match_start:]
            return r
        contents, _ = self.re_platform_macros.subn(cb, contents)

        def cb(m):
            macro = m.group(1)
            original = m.group(0)
            match_start, _ = m.span(0)
            platform_start, platform_end = m.span(1)
            new_text = "{0} /* FIXME: {0} has been removed in SDL3 */".format(macro)
            r = original[:(platform_start-match_start)] + new_text + original[platform_end-match_start:]
            return r
        contents, _ = self.re_deprecated_macros.subn(cb, contents)
        return contents


def apply_checks(paths):
    checks = (
        PlatformMacrosCheck(),
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

if __name__ == "__main__":
    raise SystemExit(main())
