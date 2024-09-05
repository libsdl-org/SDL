#!/usr/bin/env python3
#
# This script generates struct definitions with different alignments for checking

import os
import pathlib
import re
import sys


SDL_ROOT = pathlib.Path(__file__).resolve().parents[1]

SDL_INCLUDE_DIR = SDL_ROOT / "include/SDL3"

ignore_list = [ "SDL_AssertData", "SDL_MessageBoxData" ]

def print_struct(type, name, body, output):
    print("typedef %s %s {%s} %s;\n" % (type, name, body, name), file=output)

def print_validate(name, alignment, name_aligned, name_packed, fields, output):
    print(f'''\
    if (sizeof({name_aligned}) != sizeof({name_packed})) {{
        SDL_Log("{name} has incorrect size with {alignment}-byte alignment, expected %d, got %d\\n", (int)sizeof({name_packed}), (int)sizeof({name_aligned}));
        result = SDL_FALSE;
    }}
''', file=output)

    for field in fields:
        print(f'''\
    if (SDL_OFFSETOF({name_aligned}, {field}) != SDL_OFFSETOF({name_packed}, {field})) {{
        SDL_Log("{name}.{field} has incorrect offset with {alignment}-byte alignment, expected %d, got %d\\n", SDL_OFFSETOF({name_packed}, {field}), SDL_OFFSETOF({name_aligned}, {field}));
        result = SDL_FALSE;
    }}
''', file=output)

def main():
    with open(SDL_ROOT / "test/checkstruct_align8.h", "w") as align8_header, \
         open(SDL_ROOT / "test/checkstruct_align4.h", "w") as align4_header, \
         open(SDL_ROOT / "test/checkstruct_align1.h", "w") as align1_header, \
         open(SDL_ROOT / "test/checkstruct_validate.h", "w") as validate_header:

        print("#pragma pack(push,8)\n", file=align8_header)
        print("#pragma pack(push,4)\n", file=align4_header)
        print("#pragma pack(push,1)\n", file=align1_header)
        print("""
#define SDL_OFFSETOF(type, member)  ((int)(uintptr_t)(&((type *)0)->member))

static SDL_bool ValidatePadding(void)
{
    SDL_bool result = SDL_TRUE;
""", file=validate_header)

        regex_struct = re.compile(r"(typedef (struct|union) )(SDL_\w+)\n{((.*\n)+?)^}",re.MULTILINE)
        regex_fields = re.compile(r"([\w_]+)(\[[\w_]+\])?;")
        regex_comments = re.compile(r"/\*.*\*/")
        for entry in SDL_INCLUDE_DIR.glob("*.h"):
            print("Processing %s" % entry)
            with open(entry, 'r') as file:
                text = file.read()
                for match in regex_struct.finditer(text):
                    start, type, name, body, end = match.groups()

                    if name in ignore_list:
                        continue

                    body = re.sub(regex_comments, '', body)

                    fields = []
                    if type == "struct" and "    union" not in body:
                        for match in regex_fields.finditer(body):
                            field, count = match.groups()
                            if count != None:
                                fields.append(field + "[0]")
                            else:
                                fields.append(field)

                    name_pack8 = name + "_pack8"
                    print_struct(type, name_pack8, body, align8_header)

                    name_pack4 = name + "_pack4"
                    print_struct(type, name_pack4, body, align4_header)

                    name_pack1 = name + "_pack1"
                    print_struct(type, name_pack1, body, align1_header)

                    print(f"""\
    /* {name} */
""", file=validate_header)

                    print_validate(name, 8, name_pack8, name_pack1, fields, validate_header)
                    print_validate(name, 4, name_pack4, name_pack1, fields, validate_header)

        print("#pragma pack(pop)\n", file=align8_header)
        print("#pragma pack(pop)\n", file=align4_header)
        print("#pragma pack(pop)\n", file=align1_header)
        print("""\
    return result;
}
""", file=validate_header)

if __name__ == "__main__":

    try:
        main()
    except Exception as e:
        print(e)
        exit(-1)

    exit(0)

