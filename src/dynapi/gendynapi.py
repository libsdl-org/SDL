#!/usr/bin/env python3

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

# WHAT IS THIS?
#  When you add a public API to SDL, please run this script, make sure the
#  output looks sane (git diff, it adds to existing files), and commit it.
#  It keeps the dynamic API jump table operating correctly.
#
#  OS-specific API:
#   After running the script, you have to manually add #ifdef __WIN32__
#   or similar around the function in 'SDL_dynapi_procs.h'
#

import argparse
import json
import os
import pathlib
import pprint
import re


SDL_ROOT = pathlib.Path(__file__).resolve().parents[2]

SDL_INCLUDE_DIR = SDL_ROOT / "include/SDL3"
SDL_DYNAPI_PROCS_H = SDL_ROOT / "src/dynapi/SDL_dynapi_procs.h"
SDL_DYNAPI_OVERRIDES_H = SDL_ROOT / "src/dynapi/SDL_dynapi_overrides.h"
SDL_DYNAPI_SYM = SDL_ROOT / "src/dynapi/SDL_dynapi.sym"

full_API = []


def main():

    # Parse 'sdl_dynapi_procs_h' file to find existing functions
    existing_procs = find_existing_procs()

    # Get list of SDL headers
    sdl_list_includes = get_header_list()

    reg_externC = re.compile('.*extern[ "]*C[ "].*')
    reg_comment_remove_content = re.compile('\/\*.*\*/')
    reg_parsing_function = re.compile('(.*SDLCALL[^\(\)]*) ([a-zA-Z0-9_]+) *\((.*)\) *;.*')

    #eg:
    # void (SDLCALL *callback)(void*, int)
    # \1(\2)\3
    reg_parsing_callback = re.compile('([^\(\)]*)\(([^\(\)]+)\)(.*)')

    for filename in sdl_list_includes:
        if args.debug:
            print("Parse header: %s" % filename)

        input = open(filename)

        parsing_function = False
        current_func = ""
        parsing_comment = False
        current_comment = ""

        for line in input:

            # Discard pre-processor directives ^#.*
            if line.startswith("#"):
                continue

            # Discard "extern C" line
            match = reg_externC.match(line)
            if match:
                continue

            # Remove one line comment /* ... */
            # eg: extern DECLSPEC SDL_hid_device * SDLCALL SDL_hid_open_path(const char *path, int bExclusive /* = false */);
            line = reg_comment_remove_content.sub('', line)

            # Get the comment block /* ... */ across several lines
            match_start = "/*" in line
            match_end = "*/" in line
            if match_start and match_end:
                continue
            if match_start:
                parsing_comment = True
                current_comment = line
                continue
            if match_end:
                parsing_comment = False
                current_comment += line
                continue
            if parsing_comment:
                current_comment += line
                continue

            # Get the function prototype across several lines
            if parsing_function:
                # Append to the current function
                current_func += " "
                current_func += line.strip()
            else:
                # if is contains "extern", start grabbing
                if "extern" not in line:
                    continue
                # Start grabbing the new function
                current_func = line.strip()
                parsing_function = True;

            # If it contains ';', then the function is complete
            if ";" not in current_func:
                continue

            # Got function/comment, reset vars
            parsing_function = False;
            func = current_func
            comment = current_comment
            current_func = ""
            current_comment = ""

            # Discard if it doesn't contain 'SDLCALL'
            if "SDLCALL" not in func:
                if args.debug:
                    print("  Discard, doesn't have SDLCALL: " + func)
                continue

            # Discard if it contains 'SDLMAIN_DECLSPEC' (these are not SDL symbols).
            if "SDLMAIN_DECLSPEC" in func:
                if args.debug:
                    print("  Discard, has SDLMAIN_DECLSPEC: " + func)
                continue

            if args.debug:
                print("  Raw data: " + func);

            # Replace unusual stuff...
            func = func.replace(" SDL_PRINTF_VARARG_FUNC(1)", "");
            func = func.replace(" SDL_PRINTF_VARARG_FUNC(2)", "");
            func = func.replace(" SDL_PRINTF_VARARG_FUNC(3)", "");
            func = func.replace(" SDL_PRINTF_VARARG_FUNCV(1)", "");
            func = func.replace(" SDL_PRINTF_VARARG_FUNCV(2)", "");
            func = func.replace(" SDL_PRINTF_VARARG_FUNCV(3)", "");
            func = func.replace(" SDL_WPRINTF_VARARG_FUNC(3)", "");
            func = func.replace(" SDL_SCANF_VARARG_FUNC(2)", "");
            func = func.replace(" SDL_SCANF_VARARG_FUNCV(2)", "");
            func = func.replace(" __attribute__((analyzer_noreturn))", "");
            func = func.replace(" SDL_MALLOC", "");
            func = func.replace(" SDL_ALLOC_SIZE2(1, 2)", "");
            func = func.replace(" SDL_ALLOC_SIZE(2)", "");
            func = re.sub(" SDL_ACQUIRE\(.*\)", "", func);
            func = re.sub(" SDL_ACQUIRE_SHARED\(.*\)", "", func);
            func = re.sub(" SDL_TRY_ACQUIRE\(.*\)", "", func);
            func = re.sub(" SDL_TRY_ACQUIRE_SHARED\(.*\)", "", func);
            func = re.sub(" SDL_RELEASE\(.*\)", "", func);
            func = re.sub(" SDL_RELEASE_SHARED\(.*\)", "", func);
            func = re.sub(" SDL_RELEASE_GENERIC\(.*\)", "", func);

            # Should be a valid function here
            match = reg_parsing_function.match(func)
            if not match:
                print("Cannot parse: "+ func)
                exit(-1)

            func_ret = match.group(1)
            func_name = match.group(2)
            func_params = match.group(3)

            #
            # Parse return value
            #
            func_ret = func_ret.replace('extern', ' ')
            func_ret = func_ret.replace('SDLCALL', ' ')
            func_ret = func_ret.replace('DECLSPEC', ' ')
            # Remove trailing spaces in front of '*'
            tmp = ""
            while func_ret != tmp:
                tmp = func_ret
                func_ret = func_ret.replace('  ', ' ')
            func_ret = func_ret.replace(' *', '*')
            func_ret = func_ret.strip()

            #
            # Parse parameters
            #
            func_params = func_params.strip()
            if func_params == "":
                func_params = "void"

            # Identify each function parameters with type and name
            # (eventually there are callbacks of several parameters)
            tmp = func_params.split(',')
            tmp2 = []
            param = ""
            for t in tmp:
                if param == "":
                    param = t
                else:
                    param = param + "," + t
                # Identify a callback or parameter when there is same count of '(' and ')'
                if param.count('(') == param.count(')'):
                    tmp2.append(param.strip())
                    param = ""

            # Process each parameters, separation name and type
            func_param_type = []
            func_param_name = []
            for t in tmp2:
                if t == "void":
                    func_param_type.append(t)
                    func_param_name.append("")
                    continue

                if t == "...":
                    func_param_type.append(t)
                    func_param_name.append("")
                    continue

                param_name = ""

                # parameter is a callback
                if '(' in t:
                    match = reg_parsing_callback.match(t)
                    if not match:
                        print("cannot parse callback: " + t);
                        exit(-1)
                    a = match.group(1).strip()
                    b = match.group(2).strip()
                    c = match.group(3).strip()

                    try:
                        (param_type, param_name) = b.rsplit('*', 1)
                    except:
                        param_type = t;
                        param_name = "param_name_not_specified"

                    # bug rsplit ??
                    if param_name == "":
                        param_name = "param_name_not_specified"

                    # reconstruct a callback name for future parsing
                    func_param_type.append(a + " (" + param_type.strip() + " *REWRITE_NAME)" + c)
                    func_param_name.append(param_name.strip())

                    continue

                # array like "char *buf[]"
                has_array = False
                if t.endswith("[]"):
                    t = t.replace("[]", "")
                    has_array = True

                # pointer
                if '*' in t:
                    try:
                        (param_type, param_name) = t.rsplit('*', 1)
                    except:
                        param_type = t;
                        param_name = "param_name_not_specified"

                    # bug rsplit ??
                    if param_name == "":
                        param_name = "param_name_not_specified"

                    val = param_type.strip() + "*REWRITE_NAME"

                    # Remove trailing spaces in front of '*'
                    tmp = ""
                    while val != tmp:
                        tmp = val
                        val = val.replace('  ', ' ')
                    val = val.replace(' *', '*')
                    # first occurrence
                    val = val.replace('*', ' *', 1)
                    val = val.strip()

                else: # non pointer
                    # cut-off last word on
                    try:
                        (param_type, param_name) = t.rsplit(' ', 1)
                    except:
                        param_type = t;
                        param_name = "param_name_not_specified"

                    val = param_type.strip() + " REWRITE_NAME"

                # set back array
                if has_array:
                    val += "[]"

                func_param_type.append(val)
                func_param_name.append(param_name.strip())

            new_proc = {}
            # Return value type
            new_proc['retval'] = func_ret
            # List of parameters (type + anonymized param name 'REWRITE_NAME')
            new_proc['parameter'] = func_param_type
            # Real parameter name, or 'param_name_not_specified'
            new_proc['parameter_name'] = func_param_name
            # Function name
            new_proc['name'] = func_name
            # Header file
            new_proc['header'] = os.path.basename(filename)
            # Function comment
            new_proc['comment'] = comment

            full_API.append(new_proc)

            if args.debug:
                pprint.pprint(new_proc);
                print("\n")

            if func_name not in existing_procs:
                print("NEW " + func)
                add_dyn_api(new_proc)

        # For-End line in input

        input.close()
    # For-End parsing all files of sdl_list_includes

    # Dump API into a json file
    full_API_json()

    # Check comment formatting
    check_comment();

# Dump API into a json file
def full_API_json():
    if args.dump:
        filename = 'sdl.json'
        with open(filename, 'w') as f:
            json.dump(full_API, f, indent=4, sort_keys=True)
            print("dump API to '%s'" % filename);

# Check public function comments are correct
def check_comment_header():
    if not check_comment_header.done:
        check_comment_header.done = True
        print("")
        print("Please fix following warning(s):")
        print("-------------------------------")


def check_comment():

    check_comment_header.done = False

    # Check \param
    for i in full_API:
        comment = i['comment']
        name = i['name']
        retval = i['retval']
        header = i['header']

        expected = len(i['parameter'])
        if expected == 1:
            if i['parameter'][0] == 'void':
                expected = 0;
        count = comment.count("\\param")
        if count != expected:
            # skip SDL_stdinc.h
            if header != 'SDL_stdinc.h':
                # Warning mismatch \param and function prototype
                check_comment_header()
                print("  In file %s: function %s() has %d '\\param' but expected %d" % (header, name, count, expected));

        # Warning check \param uses the correct parameter name
        # skip SDL_stdinc.h
        if header != 'SDL_stdinc.h':
            parameter_name = i['parameter_name']
            for n in parameter_name:
                if n != "" and "\\param " + n not in comment and "\\param[out] " + n not in comment:
                    check_comment_header()
                    print("  In file %s: function %s() missing '\\param %s'" % (header, name, n));


    # Check \returns
    for i in full_API:
        comment = i['comment']
        name = i['name']
        retval = i['retval']
        header = i['header']

        expected = 1
        if retval == 'void':
            expected = 0;

        count = comment.count("\\returns")
        if count != expected:
            # skip SDL_stdinc.h
            if header != 'SDL_stdinc.h':
                # Warning mismatch \param and function prototype
                check_comment_header()
                print("  In file %s: function %s() has %d '\\returns' but expected %d" % (header, name, count, expected));

    # Check \since
    for i in full_API:
        comment = i['comment']
        name = i['name']
        retval = i['retval']
        header = i['header']

        expected = 1
        count = comment.count("\\since")
        if count != expected:
            # skip SDL_stdinc.h
            if header != 'SDL_stdinc.h':
                # Warning mismatch \param and function prototype
                check_comment_header()
                print("  In file %s: function %s() has %d '\\since' but expected %d" % (header, name, count, expected));



# Parse 'sdl_dynapi_procs_h' file to find existing functions
def find_existing_procs():
    reg = re.compile('SDL_DYNAPI_PROC\([^,]*,([^,]*),.*\)')
    ret = []
    input = open(SDL_DYNAPI_PROCS_H)

    for line in input:
        match = reg.match(line)
        if not match:
            continue
        existing_func = match.group(1)
        ret.append(existing_func);
        # print(existing_func)
    input.close()

    return ret

# Get list of SDL headers
def get_header_list():
    reg = re.compile('^.*\.h$')
    ret = []
    tmp = os.listdir(SDL_INCLUDE_DIR)

    for f in tmp:
        # Only *.h files
        match = reg.match(f)
        if not match:
            if args.debug:
                print("Skip %s" % f)
            continue
        ret.append(SDL_INCLUDE_DIR / f)

    return ret

# Write the new API in files: _procs.h _overrivides.h and .sym
def add_dyn_api(proc):
    func_name = proc['name']
    func_ret = proc['retval']
    func_argtype = proc['parameter']


    # File: SDL_dynapi_procs.h
    #
    # Add at last
    # SDL_DYNAPI_PROC(SDL_EGLConfig,SDL_EGL_GetCurrentEGLConfig,(void),(),return)
    f = open(SDL_DYNAPI_PROCS_H, "a")
    dyn_proc = "SDL_DYNAPI_PROC(" + func_ret + "," + func_name + ",("

    i = ord('a')
    remove_last = False
    for argtype in func_argtype:

        # Special case, void has no parameter name
        if argtype == "void":
            dyn_proc += "void"
            continue

        # Var name: a, b, c, ...
        varname = chr(i)
        i += 1

        tmp = argtype.replace("REWRITE_NAME", varname)
        dyn_proc += tmp + ", "
        remove_last = True

    # remove last 2 char ', '
    if remove_last:
        dyn_proc = dyn_proc[:-1]
        dyn_proc = dyn_proc[:-1]

    dyn_proc += "),("

    i = ord('a')
    remove_last = False
    for argtype in func_argtype:

        # Special case, void has no parameter name
        if argtype == "void":
            continue

        # Special case, '...' has no parameter name
        if argtype == "...":
            continue

        # Var name: a, b, c, ...
        varname = chr(i)
        i += 1

        dyn_proc += varname + ","
        remove_last = True

    # remove last char ','
    if remove_last:
        dyn_proc = dyn_proc[:-1]

    dyn_proc += "),"

    if func_ret != "void":
        dyn_proc += "return"
    dyn_proc += ")"
    f.write(dyn_proc + "\n")
    f.close()

    # File: SDL_dynapi_overrides.h
    #
    # Add at last
    # "#define SDL_DelayNS SDL_DelayNS_REAL
    f = open(SDL_DYNAPI_OVERRIDES_H, "a")
    f.write("#define " + func_name + " " + func_name + "_REAL\n")
    f.close()

    # File: SDL_dynapi.sym
    #
    # Add before "extra symbols go here" line
    input = open(SDL_DYNAPI_SYM)
    new_input = []
    for line in input:
        if "extra symbols go here" in line:
            new_input.append("    " + func_name + ";\n")
        new_input.append(line)
    input.close()
    f = open(SDL_DYNAPI_SYM, 'w')
    for line in new_input:
        f.write(line)
    f.close()


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('--dump', help='output all SDL API into a .json file', action='store_true')
    parser.add_argument('--debug', help='add debug traces', action='store_true')
    args = parser.parse_args()

    try:
        main()
    except Exception as e:
        print(e)
        exit(-1)

    print("done!")
    exit(0)

