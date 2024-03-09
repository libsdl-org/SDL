#!/usr/bin/env python3
#
# This script renames symbols in the API, updating SDL_oldnames.h and
# adding documentation for the change.

import argparse
import os
import pathlib
import pprint
import re
import sys
from rename_symbols import create_regex_from_replacements, replace_symbols_in_path

SDL_ROOT = pathlib.Path(__file__).resolve().parents[1]

SDL_INCLUDE_DIR = SDL_ROOT / "include/SDL3"
SDL_BUILD_SCRIPTS = SDL_ROOT / "build-scripts"


def main():
    if len(args.args) == 0 or (len(args.args) % 2) != 0:
        print("Usage: %s [-h] [--skip-header-check] header {enum,function,hint,structure,symbol} [old new ...]" % sys.argv[0])
        exit(1)

    # Check whether we can still modify the ABI
    version_header = pathlib.Path( SDL_INCLUDE_DIR / "SDL_version.h" ).read_text()
    if not re.search("SDL_MINOR_VERSION\s+[01]\s", version_header):
        raise Exception("ABI is frozen, symbols cannot be renamed")

    # Find the symbol in the headers
    if pathlib.Path(args.header).is_file():
        header = pathlib.Path(args.header)
    else:
        header = pathlib.Path(SDL_INCLUDE_DIR / args.header)

    if not header.exists():
        raise Exception("Couldn't find header %s" % header)

    header_name = header.name
    if (header.name == "SDL_gamepad.h"):
        header_name = "SDL_gamecontroller.h"

    header_text = header.read_text()

    # Replace the symbols in source code
    replacements = {}
    i = 0
    while i < len(args.args):
        oldname = args.args[i + 0]
        newname = args.args[i + 1]

        if not args.skip_header_check and not re.search((r"\b%s\b" % oldname), header_text):
            raise Exception("Couldn't find %s in %s" % (oldname, header))

        replacements[ oldname ] = newname
        replacements[ oldname + "_REAL" ] = newname + "_REAL"
        i += 2

    regex = create_regex_from_replacements(replacements)
    for dir in ["src", "test", "include", "docs", "cmake/test"]:
        replace_symbols_in_path(SDL_ROOT / dir, regex, replacements)

    # Replace the symbols in documentation
    i = 0
    while i < len(args.args):
        oldname = args.args[i + 0]
        newname = args.args[i + 1]

        add_symbol_to_oldnames(header_name, oldname, newname)
        add_symbol_to_migration(header_name, args.type, oldname, newname)
        add_symbol_to_coccinelle(args.type, oldname, newname)
        i += 2


def add_line(lines, i, section):
    lines.insert(i, section)
    i += 1
    return i


def add_content(lines, i, content, add_trailing_line):
    if lines[i - 1] == "":
        lines[i - 1] = content
    else:
        i = add_line(lines, i, content)

    if add_trailing_line:
        i = add_line(lines, i, "")
    return i


def add_symbol_to_coccinelle(symbol_type, oldname, newname):
    file = open(SDL_BUILD_SCRIPTS / "SDL_migration.cocci", "a")
    # Append-adds at last

    if symbol_type == "function":
        file.write("@@\n")
        file.write("@@\n")
        file.write("- %s\n" % oldname)
        file.write("+ %s\n" % newname)
        file.write("  (...)\n")

    if symbol_type == "symbol":
        file.write("@@\n")
        file.write("@@\n")
        file.write("- %s\n" % oldname)
        file.write("+ %s\n" % newname)

    # double check ?
    if symbol_type == "hint":
        file.write("@@\n")
        file.write("@@\n")
        file.write("- %s\n" % oldname)
        file.write("+ %s\n" % newname)

    if symbol_type == "enum" or symbol_type == "structure":
        file.write("@@\n")
        file.write("typedef %s, %s;\n" % (oldname, newname))
        file.write("@@\n")
        file.write("- %s\n" % oldname)
        file.write("+ %s\n" % newname)

    file.close()


def add_symbol_to_oldnames(header, oldname, newname):
    file = (SDL_INCLUDE_DIR / "SDL_oldnames.h")
    lines = file.read_text().splitlines()
    mode = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        if line == "#ifdef SDL_ENABLE_OLD_NAMES":
            if mode == 0:
                mode = 1
                section = ("/* ##%s */" % header)
                section_added = False
                content = ("#define %s %s" % (oldname, newname))
                content_added = False
            else:
                raise Exception("add_symbol_to_oldnames(): expected mode 0")
        elif line == "#elif !defined(SDL_DISABLE_OLD_NAMES)":
            if mode == 1:
                if not section_added:
                    i = add_line(lines, i, section)

                if not content_added:
                    i = add_content(lines, i, content, True)

                mode = 2
                section = ("/* ##%s */" % header)
                section_added = False
                content = ("#define %s %s_renamed_%s" % (oldname, oldname, newname))
                content_added = False
            else:
                raise Exception("add_symbol_to_oldnames(): expected mode 1")
        elif line == "#endif /* SDL_ENABLE_OLD_NAMES */":
            if mode == 2:
                if not section_added:
                    i = add_line(lines, i, section)

                if not content_added:
                    i = add_content(lines, i, content, True)

                mode = 3
            else:
                raise Exception("add_symbol_to_oldnames(): expected mode 2")
        elif line != "" and (mode == 1 or mode == 2):
            if line.startswith("/* ##"):
                if section_added:
                    if not content_added:
                        i = add_content(lines, i, content, True)
                        content_added = True
                elif line == section:
                    section_added = True
                elif section < line:
                    i = add_line(lines, i, section)
                    section_added = True
                    i = add_content(lines, i, content, True)
                    content_added = True
            elif line != "" and section_added and not content_added:
                if content == line:
                    content_added = True
                elif content < line:
                    i = add_content(lines, i, content, False)
                    content_added = True
        i += 1

    file.write_text("\n".join(lines) + "\n")


def add_symbol_to_migration(header, symbol_type, oldname, newname):
    file = (SDL_ROOT / "docs/README-migration.md")
    lines = file.read_text().splitlines()
    section = ("## %s" % header)
    section_added = False
    note = ("The following %ss have been renamed:" % symbol_type)
    note_added = False
    if symbol_type == "function":
        content = ("* %s() => %s()" % (oldname, newname))
    else:
        content = ("* %s => %s" % (oldname, newname))
    content_added = False
    mode = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("##") and line.endswith(".h"):
            if line == section:
                section_added = True
            elif section < line:
                break

        elif section_added and not note_added:
            if note == line:
                note_added = True
        elif note_added and not content_added:
            if content == line:
                content_added = True
            elif line == "" or content < line:
                i = add_line(lines, i, content)
                content_added = True
        i += 1

    if not section_added:
        i = add_line(lines, i, section)
        i = add_line(lines, i, "")

    if not note_added:
        i = add_line(lines, i, note)

    if not content_added:
        i = add_content(lines, i, content, True)

    file.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":

    parser = argparse.ArgumentParser(fromfile_prefix_chars='@')
    parser.add_argument("--skip-header-check", action="store_true")
    parser.add_argument("header");
    parser.add_argument("type", choices=["enum", "function", "hint", "structure", "symbol"]);
    parser.add_argument("args", nargs="*")
    args = parser.parse_args()

    try:
        main()
    except Exception as e:
        print(e)
        exit(-1)

    exit(0)

