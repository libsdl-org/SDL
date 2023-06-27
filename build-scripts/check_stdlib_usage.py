#!/usr/bin/env python3
#
#  Simple DirectMedia Layer
#  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>
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

words = [
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
    'wcsstr' ]


reg_comment_remove_content = re.compile('\/\*.*\*/')
reg_comment_remove_content2 = re.compile('".*"')
reg_comment_remove_content3 = re.compile(':strlen')
reg_comment_remove_content4 = re.compile('->free')

def find_symbols_in_file(file, regex):

    allowed_extensions = [ ".c", ".cpp", ".m", ".h",  ".hpp", ".cc" ]

    excluded_paths = [
        "src/stdlib",
        "src/libm",
        "src/hidapi",
        "src/video/khronos",
        "include/SDL3",
        "build-scripts/gen_audio_resampler_filter.c",
        "build-scripts/gen_audio_channel_conversion.c" ]

    filename = pathlib.Path(file)

    for ep in excluded_paths:
        if ep in filename.as_posix():
            # skip
            return

    if filename.suffix not in allowed_extensions:
        # skip
        return

    # print("Parse %s" % file)

    try:
        with file.open("r", encoding="UTF-8", newline="") as rfp:
            parsing_comment = False
            for l in rfp:
                l = l.strip()

                # Get the comment block /* ... */ across several lines
                match_start = "/*" in l
                match_end = "*/" in l
                if match_start and match_end:
                    continue
                if match_start:
                    parsing_comment = True
                    continue
                if match_end:
                    parsing_comment = False
                    continue
                if parsing_comment:
                    continue

                if regex.match(l):

                    # free() allowed here
                    if "This should NOT be SDL_" in l:
                        continue

                    # double check
                    # Remove one line comment /* ... */
                    # eg: extern DECLSPEC SDL_hid_device * SDLCALL SDL_hid_open_path(const char *path, int bExclusive /* = false */);
                    l = reg_comment_remove_content.sub('', l)

                    # Remove strings " ... "
                    l = reg_comment_remove_content2.sub('', l)

                    # :strlen
                    l = reg_comment_remove_content3.sub('', l)

                    # ->free
                    l = reg_comment_remove_content4.sub('', l)

                    if regex.match(l):
                        print("File %s" % filename)
                        print("        %s" % l)
                        print("")

    except UnicodeDecodeError:
        print("%s is not text, skipping" % file)
    except Exception as err:
        print("%s" % err)

def find_symbols_in_dir(path, regex):

    for entry in path.glob("*"):
        if entry.is_dir():
            find_symbols_in_dir(entry, regex)
        else:
            find_symbols_in_file(entry, regex)

def main():
    str = ".*\\b("
    for w in words:
        str += w + "|"
    str = str[:-1]
    str += ")\("
    regex = re.compile(str)
    find_symbols_in_dir(SDL_ROOT, regex)

if __name__ == "__main__":

    parser = argparse.ArgumentParser(fromfile_prefix_chars='@')
    args = parser.parse_args()

    try:
        main()
    except Exception as e:
        print(e)
        exit(-1)

    exit(0)


