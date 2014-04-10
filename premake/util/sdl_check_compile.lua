-- Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>
--
-- This software is provided 'as-is', without any express or implied
-- warranty.  In no event will the authors be held liable for any damages
-- arising from the use of this software.
--
-- Permission is granted to anyone to use this software for any purpose,
-- including commercial applications, and to alter it and redistribute it
-- freely.
--
-- Meta-build system using premake created and maintained by
-- Benjamin Henning <b.henning@digipen.edu>

--[[
sdl_check_compile.lua

	This file provides various utility functions which allow the meta-build
	system to perform more complex dependency checking than premake initially
	allows. This is done using the (currently) GCC toolchain to build generated
	C files which try to import certain headers, link to certain functions, link
	to certain libraries, or a combination of the above. It supports providing a
	custom source to try and build, link, and/or run per the implementation's
	choice, so the possibilities are nearly endless with that this system is
	capable of, though it could always do with more flexibility.
]]


local cxx = "gcc"
local cxx_flags = ""
local cxx_io_flags = "-o premakecheck.o -c premakecheck.c 2> /dev/null"
local cxx_includes = { }

local link = "gcc"
local link_flags = ""
local link_io_flags = "-o premakecheck.out premakecheck.o"
local link_end = " 2> /dev/null"

local run = "./premakecheck.out"
local run_flags = ""
local run_io_flags = " > ./premakecheck.stdout"

local checked_printf = false
local has_printf = false

-- Set the application used to compile the generated files.
function set_cxx(compiler)
	cxx = compiler
end

-- Set custom flags for the compiler.
function set_cxx_flags(flags)
	cxx_flags = flags
end

-- Include a search directory for libraries.
local function include_library_dir(dir)
	link_flags = link_flags .. "-L" .. dir .. " "
end

-- Include a library to be linked durnig the link step.
local function link_library(lib)
	link_flags = link_flags .. "-l" .. lib .. " "
end

-- Reset the link flags.
local function reset_link_flags()
	link_flags = ""
end

-- Creates the build command line to be executed.
local function build_compile_line()
	return cxx .. " " .. cxx_flags .. " " .. cxx_io_flags
end

-- Creates the link command line to be executed.
local function build_link_line()
	return link .. " " .. link_io_flags .. " " .. link_flags .. link_end
end

-- Create the run line to be executed.
local function build_run_line()
	return run .. " " .. run_flags .. " " .. run_io_flags
end

-- Builds a list of preprocessor include directives for all the include files
-- successfully found so far by these functions, so as to perform automatic
-- feature checking for the clientside code.
local function build_includes()
	local includes = ""
	for _,v in ipairs(cxx_includes) do
		includes = includes .. '#include "' .. v .. '"\n'
	end
	return includes
end

-- Cleanup the generated build environment.
local function cleanup_build()
	os.remove("./premakecheck.c")
	os.remove("./premakecheck.o")
	os.remove("./premakecheck.out")
	os.remove("./premakecheck.stdout")
end

-- Check if a source builds, links, and or/runs, where running depends on
-- linking and linking depends on building. The return from this function is
-- a triple, where the first is a boolean value indicating if it successfully
-- was built, the second is a boolean value indicating if it successfully
-- linked, and the third represents nil if it was not run or run correctly, or
-- the output from the program executed (may be empty for no output).
local function check_build_source(source, link, run)
	local file = fileopen("./premakecheck.c", "wt")
	file:write(source)
	file:close()
	local result = os.execute(build_compile_line())
	if not link then
		cleanup_build()
		if result == 0 then
			return true, false, nil -- compile, no link, no run
		end
		return false, false, nil -- no compile, no link, no run
	end
	-- try linking, too
	if result ~= 0 then
		-- can't link if it doesn't compile
		cleanup_build()
		return false, false, nil -- no compile, no link, no run
	end
	result = os.execute(build_link_line())
	if not run or result ~= 0 then -- have to link to run
		cleanup_build()
		return true, result == 0, nil -- compile, maybe link, no run
	end
	result = os.execute(build_run_line())
	local output = readfile("./premakecheck.stdout", "rt")
	cleanup_build()
	return true, true, output -- compile, link, ran
end

-- Given C source code, determine whether the source code will compile in the
-- present environment. Returns true if the source was successfully compiled, or
-- false if otherwise.
function check_cxx_source_compiles(source)
	local r1, _, __ = check_build_source(source, false, false)
	return r1
end

-- Given C source code, determine whether the source code can be built into a
-- working executable. That is, it will check if the code both compiles and
-- links. Returns true if the code was successfully built (compiled and linked),
-- or false if otherwise.
function check_cxx_source_builds(source)
	local r1, r2, _ = check_build_source(source, true, false)
	return r1 and r2
end

-- Given C source code, attempt to compile, link, and execute the source code.
-- This function will return two values. The first is a boolean indicating
-- whether the source code was successfully run (meaning it was compiled, built,
-- and ran successfully), and the second value returned is the actual output
-- from running the application, or nil if it did not run correctly or was not
-- built. The output may be an empty string if the code does not print anything
-- to stdout.
function check_cxx_source_runs(source)
	local r1, r2, r3 = check_build_source(source, true, true)
	return r1 and r2 and (r3 ~= nil), r3
end

-- Given a header file, check whether the header file is visible to the compiler
-- in the given environment. Returns a boolean indicating thus. If a header file
-- is found in either of these functions, it will be added to a list of headers
-- that can be used in subsequent dependency checks.
function check_include_file(inc)
	return check_include_files(inc)
end

-- Given a variable list of header files, check whether all of the includes are
-- visible in the given environment. Every file must be included in order for
-- this function to return true.
function check_include_files(...)
	local source = ""
	for _, v in ipairs{...} do
		source = source .. '#include "' .. v .. '"\n'
	end
	local result = check_cxx_source_compiles(source)
	if result then
		for _, v in ipairs{...} do
			table.insert(cxx_includes, v)
		end
	end
	return result
end

-- Given a directory, determine whether the directory contains any header files.
-- Unfortunately it does assume the extension is .h, but this can be altered in
-- future versions of this software. The function returns true if the directory
-- (or any of its subdirectories) contain .h files, or false if otherwise (such
-- as if the directory does not exist).
function check_include_directory(incDir)
	incDir = incDir:gsub("\\", "/"):gsub("//", "/")
	if incDir:sub(#incDir, #incDir) ~= "/" then
		incDir = incDir .. "/"
	end
	return #os.matchfiles(incDir .. "**.h") > 0
end

-- Given a variable list of directories, iteratively check if each one contains
-- header files, per the functionality of check_include_directory. This function
-- returns true if and only if every listed directory or its subdirectories
-- contain .h files.
function check_include_directories(...)
	for _, v in ipairs{...} do
		if not check_include_directory(v) then
			return false
		end
	end
	return true
end

-- Given a function name, attempt to determine whether the function can be found
-- within all of the known include files. Known include files are derived from
-- the check_include_file(s) functions.
function check_function_exists(func)
	local source = build_includes()
	source = source .. 'int main(int argc, char **argv) {\n'
	source = source .. '\tvoid *check = (void *) ' .. func .. ';\n'
	source = source .. '\treturn 0;\n'
	return check_cxx_source_builds(source .. '}')
end

-- Given a library, a function that must exist within the library, and an
-- include file prototyping the function, this function determines whether those
-- three variables are able to build a working executable. That is, if a
-- function can be properly linked to using a given library, then the library
-- can be assumed to exist. Returns true if and only if the function was
-- correctly linked to.
function check_library_exists(lib, func, inc)
	local source = build_includes()
	if inc ~= nil then
		source = source .. '#include "' .. inc .. '"\n'
	end
	source = source .. 'int main(int argc, char **argv) {\n'
	source = source .. '\tvoid *check = (void *) ' .. func .. ';\n'
	source = source .. '\treturn 0;\n'
	if lib ~= nil then
		link_library(lib)
	end
	local result = check_cxx_source_builds(source .. '}')
	reset_link_flags()
	return result
end

-- This is a merge variable list version of the check_library_exists function.
-- The thing to note with this function is that it will return true for the
-- first library found to correctly link to the function. This function is used
-- to determine whether the function is found in a list of libraries, not if it
-- is found in every one of the libraries.
function check_library_exists_multiple(func, inc, ...)
	for _,v in ipairs{...} do
		if check_library_exists(v, func, inc) then
			return true
		end
	end
	return false
end

-- This is a wrapper for the check_library_exists function that will also
-- attempt to locate the library in question, in case it's not in a path the
-- compiler is already aware of. This function has the same return consequences
-- as check_library_exists.
function check_library_exists_lookup(lib, func, inc)
	local dir = os.findlib(lib)
	if dir == nil then
		return false
	end
	include_library_dir(dir)
	return check_library_exists(lib, func, inc)
end

-- Given a valid C type name, this function generates a program that will print
-- the size of the type using the sizeof operator to the console, then parse the
-- size to indicate the byte size of the type on this platform. The resulting
-- executable is dependent on stdio and the printf function, which it safely
-- checks for behind the scenes. If these dependencies are not found for
-- whatever reason, this function returns 0, otherwise it returns a proper
-- numerical value representing the size of the specified type.
function check_type_size(typename)
	if not checked_printf then
		checked_printf = true
		has_printf = check_include_file("stdio.h") and check_function_exists("printf")
		if not has_printf then
			print("Warning: cannot check the size of a type without stdio and printf.")
		end
	end
	if not has_printf then
		return 0
	end
	local source = '#include "stdio.h"\n'
	source = source .. 'int main(int argc, char **argv) {\n'
	source = source .. '\tprintf("%d", sizeof(' .. typename .. '));\n'
	source = source .. '\treturn 0;\n'
	local success, result = check_cxx_source_runs(source .. '}');
	if not success then
		print("Warning: could not get the size of type: " .. typename)
		return 0
	end
	return tonumber(result)
end
