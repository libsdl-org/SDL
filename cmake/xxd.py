#!/usr/bin/env python

import argparse
import os
import pathlib
import re

def main():
    parser = argparse.ArgumentParser(allow_abbrev=False, description="Convert file into includable C header")
    parser.add_argument("--in", "-i", type=pathlib.Path, metavar="INPUT", dest="input", required=True, help="Input file")
    parser.add_argument("--out", "-o", type=pathlib.Path, metavar="OUTPUT", dest="output", required=True, help="Output header")
    parser.add_argument("--columns", type=int, default=12, help="Column count")
    args = parser.parse_args()

    t = pathlib.Path()
    varname, _ = re.subn("[^a-zA-Z0-9]", "_", str(args.input.name))

    binary_data = args.input.open("rb").read()

    with args.output.open("w", newline="\n") as fout:
        fout.write("unsigned char {}[] = {{\n".format(varname))
        bytes_written = 0
        while bytes_written < len(binary_data):
            col = bytes_written % args.columns
            if col == 0:
                fout.write("  ")
            column_data = binary_data[bytes_written:bytes_written+args.columns]
            fout.write(", ".join("0x{:02x}".format(d) for d in column_data))
            bytes_written += len(column_data)
            if bytes_written < len(binary_data):
                fout.write(",\n")
            else:
                fout.write("\n")
        fout.write("}};\nunsigned int {}_len = {:d};\n".format(varname, len(binary_data)))

if __name__ == "__main__":
    raise SystemExit(main())
