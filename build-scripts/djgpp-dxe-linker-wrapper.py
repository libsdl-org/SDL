#!/usr/bin/env python3

import argparse
import os
import sys
import subprocess

def parse_wrapper_args(args):
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("--library-paths", nargs="*", help="Additional DJGPP Library paths")
    parser.add_argument("--dxe3gen", default="dxe3gen", help="Path of dxe3gen")
    parser.add_argument("--import-library", help="Import library name")
    args = parser.parse_args(args)
    return args

def parse_linker_args(args):
    unknown_args = []
    output = None
    i = 1
    while i < len(args):
        arg = args[i]
        if arg in ("-fPIC", "-shared"):
            consumed = 1
        elif arg.startswith("-Wl,"):
            unknown_args.extend(arg[4:].split(","))
        elif arg.startswith("-I"):
            if arg == "-I":
                consumed = 2
            else:
                consumed = 1
        elif arg == "-o":
            output = args[i + 1]
            consumed = 2
        elif arg.startswith("-D"):
            consumed = 1
        elif arg.startswith("-O"):
            consumed = 1
        else:
            unknown_args.append(arg)
            consumed = 1
        i += consumed
    if not output:
        print("Missing \"-o\" argument", file=sys.stderr)
    return output, unknown_args

try:
    pos_double_dash = sys.argv.index("--")
    wrapper_args = sys.argv[1:pos_double_dash]
    original_args = sys.argv[pos_double_dash+1:]
except IndexError:
    wrapper_args = []
    original_args = sys.argv[1:]

wrapper_args = parse_wrapper_args(wrapper_args)
output_path, original_linker_args = parse_linker_args(original_args)

dxe3gen_command = [
    wrapper_args.dxe3gen,
    "-o", output_path,
    "-U",
]
if wrapper_args.import_library:
    dxe3gen_command.extend(["-Y", wrapper_args.import_library])
for libpath in wrapper_args.library_paths:
    dxe3gen_command.append("-L" + libpath)
dxe3gen_command.extend(original_linker_args)
if not "-nostlib" in original_linker_args:
    dxe3gen_command.append("-lgcc")

os.environ["DXE_LD_LIBRARY_PATH"] = "dontcare"
os.environ["DJDIR"] = "dontcare"

process_result = subprocess.run(dxe3gen_command)

raise SystemExit(process_result.returncode)
