#!/usr/bin/env python3
#
#  Simple DirectMedia Layer
#  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>
#
#  This software is provided 'as-is', without any express or implied
#  warranty.  In no event will the authors be held liable for any damages
#  arising from the use of this software.
#
#  Permission is granted to anyone to use this software for any purpose,
#  including commercial applications, and to alter it and redistribute it
#  freely, subject to the following restrictions:
#
#  1. The origin of this software must not be misrepresented; you must not
#     claim that you wrote the original software. If you use this software
#     in a product, an acknowledgment in the product documentation would be
#     appreciated but is not required.
#  2. Altered source versions must be plainly marked as such, and must not be
#     misrepresented as being the original software.
#  3. This notice may not be removed or altered from any source distribution.
#
# This script detects use of stdlib function in SDL code

import argparse
import os
import pathlib
import re
import sys

SDL_ROOT = pathlib.Path(__file__).resolve().parents[1]

STDLIB_SYMBOLS = [
    'abs',
    'acos',
    'acosf',
    'asin',
    'asinf',
    'asprintf',
    'atan',
    'atan2',
    'atan2f',
    'atanf',
    'atof',
    'atoi',
    'bsearch',
    'calloc',
    'ceil',
    'ceilf',
    'copysign',
    'copysignf',
    'cos',
    'cosf',
    'crc32',
    'exp',
    'expf',
    'fabs',
    'fabsf',
    'floor',
    'floorf',
    'fmod',
    'fmodf',
    'free',
    'getenv',
    'isalnum',
    'isalpha',
    'isblank',
    'iscntrl',
    'isdigit',
    'isgraph',
    'islower',
    'isprint',
    'ispunct',
    'isspace',
    'isupper',
    'isxdigit',
    'itoa',
    'lltoa',
    'log10',
    'log10f',
    'logf',
    'lround',
    'lroundf',
    'ltoa',
    'malloc',
    'memalign',
    'memcmp',
    'memcpy',
    'memcpy4',
    'memmove',
    'memset',
    'pow',
    'powf',
    'qsort',
    'qsort_r',
    'qsort_s',
    'realloc',
    'round',
    'roundf',
    'scalbn',
    'scalbnf',
    'setenv',
    'sin',
    'sinf',
    'snprintf',
    'sqrt',
    'sqrtf',
    'sscanf',
    'strcasecmp',
    'strchr',
    'strcmp',
    'strdup',
    'strlcat',
    'strlcpy',
    'strlen',
    'strlwr',
    'strncasecmp',
    'strncmp',
    'strrchr',
    'strrev',
    'strstr',
    'strtod',
    'strtokr',
    'strtol',
    'strtoll',
    'strtoul',
    'strupr',
    'tan',
    'tanf',
    'tolower',
    'toupper',
    'trunc',
    'truncf',
    'uitoa',
    'ulltoa',
    'ultoa',
    'utf8strlcpy',
    'utf8strlen',
    'vasprintf',
    'vsnprintf',
    'vsscanf',
    'wcscasecmp',
    'wcscmp',
    'wcsdup',
    'wcslcat',
    'wcslcpy',
    'wcslen',
    'wcsncasecmp',
    'wcsncmp',
    'wcsstr',
]
RE_STDLIB_SYMBOL = re.compile(rf"\b(?P<symbol>{'|'.join(STDLIB_SYMBOLS)})\b\(")


def find_symbols_in_file(file: pathlib.Path) -> int:
    match_count = 0

    allowed_extensions = [ ".c", ".cpp", ".m", ".h",  ".hpp", ".cc" ]

    excluded_paths = [
        "src/stdlib",
        "src/libm",
        "src/hidapi",
        "src/video/khronos",
        "include/SDL3",
        "build-scripts/gen_audio_resampler_filter.c",
        "build-scripts/gen_audio_channel_conversion.c",
        "test/win32/sdlprocdump.c",
    ]

    filename = pathlib.Path(file)

    for ep in excluded_paths:
        if ep in filename.as_posix():
            # skip
            return 0

    if filename.suffix not in allowed_extensions:
        # skip
        return 0

    # print("Parse %s" % file)

    try:
        with file.open("r", encoding="UTF-8", newline="") as rfp:
            parsing_comment = False
            for line_i, original_line in enumerate(rfp, start=1):
                line = original_line.strip()

                line_comment = ""

                # Get the comment block /* ... */ across several lines
                while True:
                    if parsing_comment:
                        pos_end_comment = line.find("*/")
                        if pos_end_comment >= 0:
                            line = line[pos_end_comment+2:]
                            parsing_comment = False
                        else:
                            break
                    else:
                        pos_start_comment = line.find("/*")
                        if pos_start_comment >= 0:
                            pos_end_comment = line.find("*/", pos_start_comment+2)
                            if pos_end_comment >= 0:
                                line_comment += line[pos_start_comment:pos_end_comment+2]
                                line = line[:pos_start_comment] + line[pos_end_comment+2:]
                            else:
                                line_comment += line[pos_start_comment:]
                                line = line[:pos_start_comment]
                                parsing_comment = True
                                break
                        else:
                            break
                if parsing_comment:
                    continue
                pos_line_comment = line.find("//")
                if pos_line_comment >= 0:
                    line_comment += line[pos_line_comment:]
                    line = line[:pos_line_comment]

                if m := RE_STDLIB_SYMBOL.match(line):
                    override_string = f"This should NOT be SDL_{m['symbol']}()"
                    if override_string not in line_comment:
                        print(f"{filename}:{line_i}")
                        print(f"    {line}")
                        print(f"")
                        match_count += 1

    except UnicodeDecodeError:
        print(f"{file} is not text, skipping", file=sys.stderr)

    return match_count

def find_symbols_in_dir(path: pathlib.Path) -> int:
    match_count = 0
    for entry in path.glob("*"):
        if entry.is_dir():
            match_count += find_symbols_in_dir(entry)
        else:
            match_count += find_symbols_in_file(entry)
    return match_count

def main():
    parser = argparse.ArgumentParser(fromfile_prefix_chars="@")
    parser.add_argument("path", default=SDL_ROOT, nargs="?", type=pathlib.Path, help="Path to look for stdlib symbols")
    args = parser.parse_args()

    print(f"Looking for stdlib usage in {args.path}...")

    match_count = find_symbols_in_dir(args.path)

    if match_count:
        print("If the stdlib usage is intentional, add a '// This should NOT be SDL_<symbol>()' line comment.")
        print("")
        print("NOT OK")
    else:
        print("OK")
    return 1 if match_count else 0

if __name__ == "__main__":
    raise SystemExit(main())
