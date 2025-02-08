#!/usr/bin/env python3
#
# This script renames symbols in the specified paths

import argparse
import os
import pathlib
import re
import sys


def main():
    if len(args.args) < 1:
        print("Usage: %s files_or_directories ..." % sys.argv[0])
        exit(1)

    replacements = {
        "SDL_bool": "bool",
        "SDL_TRUE": "true",
        "SDL_FALSE": "false",
    }
    entries = args.args[0:]

    regex = create_regex_from_replacements(replacements)

    for entry in entries:
        path = pathlib.Path(entry)
        if not path.exists():
            print("%s doesn't exist, skipping" % entry)
            continue

        replace_symbols_in_path(path, regex, replacements)

def create_regex_from_replacements(replacements):
    return re.compile(r"\b(%s)\b" % "|".join(map(re.escape, replacements.keys())))

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
    parser.add_argument("args", nargs="*")
    args = parser.parse_args()

    try:
        main()
    except Exception as e:
        print(e)
        exit(-1)

    exit(0)

