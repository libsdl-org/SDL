#!/usr/bin/env python3
#
# This script renames SDL headers in the specified paths

import argparse
import pathlib
import re


def do_include_replacements(paths):
    replacements = [
        ( re.compile(r"(?:[\"<])(?:SDL2/)?SDL_image.h(?:[\">])"), r"<SDL3_image/SDL_image.h>" ),
        ( re.compile(r"(?:[\"<])(?:SDL2/)?SDL_mixer.h(?:[\">])"), r"<SDL3_mixer/SDL_mixer.h>" ),
        ( re.compile(r"(?:[\"<])(?:SDL2/)?SDL_net.h(?:[\">])"), r"<SDL3_net/SDL_net.h>" ),
        ( re.compile(r"(?:[\"<])(?:SDL2/)?SDL_rtf.h(?:[\">])"), r"<SDL3_rtf/SDL_rtf.h>" ),
        ( re.compile(r"(?:[\"<])(?:SDL2/)?SDL_ttf.h(?:[\">])"), r"<SDL3_ttf/SDL_ttf.h>" ),
        ( re.compile(r"(?:[\"<])(?:SDL2/)?SDL_gamecontroller.h(?:[\">])"), r"<SDL3/SDL_gamepad.h>" ),
        ( re.compile(r"(?:[\"<])(?:SDL2/)?begin_code.h(?:[\">])"), r"<SDL3/SDL_begin_code.h>" ),
        ( re.compile(r"(?:[\"<])(?:SDL2/)?close_code.h(?:[\">])"), r"<SDL3/SDL_close_code.h>" ),
        ( re.compile(r"(?:[\"<])(?:SDL2/)?(SDL[_a-z0-9]*\.h)(?:[\">])"), r"<SDL3/\1>" )
    ]
    for entry in paths:
        path = pathlib.Path(entry)
        if not path.exists():
            print("{} does not exist, skipping".format(entry))
            continue

        replace_headers_in_path(path, replacements)


def replace_headers_in_file(file, replacements):
    try:
        with file.open("r", encoding="UTF-8", newline="") as rfp:
            original = rfp.read()
            contents = original
            for regex, replacement in replacements:
                contents = regex.sub(replacement, contents)
            if contents != original:
                with file.open("w", encoding="UTF-8", newline="") as wfp:
                    wfp.write(contents)
    except UnicodeDecodeError:
        print("%s is not text, skipping" % file)
    except Exception as err:
        print("%s" % err)


def replace_headers_in_dir(path, replacements):
    for entry in path.glob("*"):
        if entry.is_dir():
            replace_headers_in_dir(entry, replacements)
        else:
            print("Processing %s" % entry)
            replace_headers_in_file(entry, replacements)


def replace_headers_in_path(path, replacements):
        if path.is_dir():
            replace_headers_in_dir(path, replacements)
        else:
            replace_headers_in_file(path, replacements)


def main():
    parser = argparse.ArgumentParser(fromfile_prefix_chars='@', description="Rename #include's for SDL3.")
    parser.add_argument("args", metavar="PATH", nargs="*", help="Input source file")
    args = parser.parse_args()

    try:
        do_include_replacements(args.args)
    except Exception as e:
        print(e)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
