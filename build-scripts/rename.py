#!/usr/bin/env python3

# WHAT IS THIS?
#  This script renames symbols in the API, updating SDL_oldnames.h and
#  adding documentation for the change.

import argparse
import os
import pathlib
import pprint
import re


SDL_ROOT = pathlib.Path(__file__).resolve().parents[1]

SDL_INCLUDE_DIR = SDL_ROOT / "include/SDL3"


def main():
    # Check whether we can still modify the ABI
    version_header = pathlib.Path( SDL_INCLUDE_DIR / "SDL_version.h" ).read_text()
    if not re.search("SDL_MINOR_VERSION\s+[01]\s", version_header):
        raise Exception("ABI is frozen, symbols cannot be renamed")

    pattern = re.compile(r"\b%s\b" % args.oldname)

    # Find the symbol in the headers
    if pathlib.Path(args.header).is_file():
        header = pathlib.Path(args.header)
    else:
        header = pathlib.Path(SDL_INCLUDE_DIR / args.header)

    if not header.exists():
        raise Exception("Couldn't find header %s" % header)

    if not pattern.search(header.read_text()):
        raise Exception("Couldn't find %s in %s" % (args.oldname, header))

    # Replace the symbol in source code and documentation
    for dir in ['src', 'test', 'include', 'docs']:
        replace_symbol_recursive(SDL_ROOT / dir, pattern, args.newname)

    add_symbol_to_oldnames(header.name, args.oldname, args.newname)
    add_symbol_to_migration(header.name, args.type, args.oldname, args.newname)
    add_symbol_to_whatsnew(args.type, args.oldname, args.newname)


def replace_symbol_recursive(path, pattern, replacement):
    for entry in path.glob("*"):
        if entry.is_dir():
            replace_symbol_recursive(entry, pattern, replacement)
        elif not entry.name.endswith((".bmp", ".cur", ".dat", ".icns", ".png", ".strings", ".swp", ".wav")) and \
             entry.name != "utf8.txt":
            print("Processing %s" % entry)
            with entry.open('r', encoding='UTF-8', newline='') as rfp:
                contents = pattern.sub(replacement, rfp.read())
                with entry.open('w', encoding='UTF-8', newline='') as wfp:
                    wfp.write(contents)


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
        elif line == "#else /* !SDL_ENABLE_OLD_NAMES */":
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
    content = ("* %s -> %s" % (oldname, newname))
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


def add_symbol_to_whatsnew(symbol_type, oldname, newname):
    file = (SDL_ROOT / "WhatsNew.txt")
    lines = file.read_text().splitlines()
    note = ("* The following %ss have been renamed:" % symbol_type)
    note_added = False
    content = ("    * %s -> %s" % (oldname, newname))
    content_added = False
    mode = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        if not note_added:
            if note == line:
                note_added = True
        elif not content_added:
            if content == line:
                content_added = True
            elif not line.startswith("    *") or content < line:
                i = add_line(lines, i, content)
                content_added = True
        i += 1

    if not note_added:
        i = add_line(lines, i, note)

    if not content_added:
        i = add_line(lines, i, content)

    file.write_text("\r\n".join(lines) + "\r\n")


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('header');
    parser.add_argument('type', choices=['function', 'enum']);
    parser.add_argument('oldname');
    parser.add_argument('newname');
    args = parser.parse_args()

    try:
        main()
    except Exception as e:
        print(e)
        exit(-1)

    exit(0)

