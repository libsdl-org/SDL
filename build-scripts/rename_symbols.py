#!/usr/bin/env python3
#
# This script renames symbols in the specified paths

import argparse
import os
import pathlib
import re
import sys


SDL_ROOT = pathlib.Path(__file__).resolve().parents[1]

SDL_INCLUDE_DIR = SDL_ROOT / "include/SDL3"


def main():
    if args.all_symbols:
        if len(args.args) < 1:
            print("Usage: %s --all-symbols files_or_directories ..." % sys.argv[0])
            exit(1)

        replacements = get_all_replacements()
        entries = args.args

    else:
        if len(args.args) < 3:
            print("Usage: %s [--substring] oldname newname files_or_directories ..." % sys.argv[0])
            exit(1)

        replacements = { args.args[0]: args.args[1] }
        entries = args.args[2:]

    if args.substring:
        regex = create_substring_regex_from_replacements(replacements)
    else:
        regex = create_regex_from_replacements(replacements)

    for entry in entries:
        path = pathlib.Path(entry)
        if not path.exists():
            print("%s doesn't exist, skipping" % entry)
            continue

        replace_symbols_in_path(path, regex, replacements)


def get_all_replacements():
    replacements = {}
    file = (SDL_INCLUDE_DIR / "SDL_oldnames.h")
    mode = 0
    for line in file.read_text().splitlines():
        if line == "#ifdef SDL_ENABLE_OLD_NAMES":
            if mode == 0:
                mode = 1
            else:
                raise Exception("get_all_replacements(): expected mode 0")
        elif line == "#elif !defined(SDL_DISABLE_OLD_NAMES)":
            if mode == 1:
                mode = 2
            else:
                raise Exception("get_all_replacements(): expected mode 1")
        elif line == "#endif /* SDL_ENABLE_OLD_NAMES */":
            if mode == 2:
                mode = 3
            else:
                raise Exception("add_symbol_to_oldnames(): expected mode 2")
        elif mode == 1 and line.startswith("#define "):
            words = line.split()
            replacements[words[1]] = words[2]
            # In case things are accidentally renamed to the "X_renamed_Y" symbol
            #replacements[words[1] + "_renamed_" + words[2]] = words[2]

    return replacements


def create_regex_from_replacements(replacements):
    return re.compile(r"\b(%s)\b" % "|".join(map(re.escape, replacements.keys())))


def create_substring_regex_from_replacements(replacements):
    return re.compile(r"(%s)" % "|".join(map(re.escape, replacements.keys())))


def replace_symbols_in_file(file, regex, replacements):
    try:
        with file.open("r", encoding="UTF-8", newline="") as rfp:
            original = rfp.read()
            contents = regex.sub(lambda mo: replacements[mo.string[mo.start():mo.end()]], original)
            if contents != original:
                with file.open("w", encoding="UTF-8", newline="") as wfp:
                    wfp.write(contents)
    except UnicodeDecodeError:
        print("%s is not text, skipping" % file)
    except Exception as err:
        print("%s" % err)


def replace_symbols_in_dir(path, regex, replacements):
    for entry in path.glob("*"):
        if entry.is_dir():
            replace_symbols_in_dir(entry, regex, replacements)
        else:
            print("Processing %s" % entry)
            replace_symbols_in_file(entry, regex, replacements)


def replace_symbols_in_path(path, regex, replacements):
        if path.is_dir():
            replace_symbols_in_dir(path, regex, replacements)
        else:
            replace_symbols_in_file(path, regex, replacements)


if __name__ == "__main__":

    parser = argparse.ArgumentParser(fromfile_prefix_chars='@')
    parser.add_argument("--all-symbols", action="store_true")
    parser.add_argument("--substring", action="store_true")
    parser.add_argument("args", nargs="*")
    args = parser.parse_args()

    try:
        main()
    except Exception as e:
        print(e)
        exit(-1)

    exit(0)

