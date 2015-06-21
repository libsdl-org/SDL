-- Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>
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
sdl_projects.lua

	This file contains all the functions which are needed to define any project
	within the meta-build system. Many of these functions serve as
	pseudo-replacements for many similarly named premake functions, and that is
	intentional. Even the implementation of these functions are intended to look
	similar to regular premake code. These functions serve to dramatically
	simplify the project definition process to just a few lines of code, versus
	the many more needed for projects defined purely with premake.

	This approach is possible because this meta-build system adds another layer of
	indirection to the premake system, creating a sort of 'meta-meta-build'
	system. Nevertheless, there is a lot more flexibility because the meta-build
	system itself can be used to check for dependencies in a much more complex way
	than premake originally intended. All of the functions useful to the project
	definition system are contained in this file and are documented.
]]

projects = { }

local currentProject = nil
local currentDep = nil
local nextFuncCompat = true -- by default, unless state otherwise changed
local dependencyFunctions = { }
local dependencyResults = { } -- for when the dependencies are executed

-- query whether this function is compatible; resets internal state of
-- compatibility to true until SDL_isos is called again
local function oscompat()
	local compat = nextFuncCompat
	nextFuncCompat = true
	return compat
end

-- determine whether the specific OS name is within a pattern.
local function osmatch(name, pattern)
	local checks = pattern:explode('|')
	for i,v in pairs(checks) do
		if name == v then
			return true
		end
	end
	return false
end

-- Registers a dependency checker function based on a name. This function is
-- used in order to determine compatibility with the current system for a given
-- SDL_dependency. See SDL_depfunc for more information.
--
-- Specifies a function which will be invoked upon determining whether this
-- dependency is valid for the current system setup (ie, whether the system
-- has the right architecture, operating system, or even if it's installed).
-- The dependency function takes no arguments, but it must return the following
-- values:
--
--   <foundDep> [includePaths] [libPaths] [inputLibLibraries]
--
-- The last three are optional, unless foundDep is true. The name should be
-- descriptive of the outside dependency, since it may be shown to the user.
-- This function is intended to be used only after invoking SDL_dependency.
function SDL_registerDependencyChecker(name, func)
	dependencyFunctions[name:lower()] = func
end

-- Initializes the definition of a SDL project given the name of the project.
function SDL_project(name)
	if not oscompat() then return end
	currentProject = { }
	currentProject.name = name
	currentProject.compat = true
	projects[name] = currentProject
	currentProject.dependencyTree = { }
	-- stores which dependencies have already been checked on behalf of this
	-- project
	currentProject.dependencyValues = { }
	currentDep = nil
end

-- Specifies the build kind of the SDL project (e.g. StaticLib, SharedLib,
-- ConsoleApp, etc.), based on premake presets.
function SDL_kind(k)
	if not oscompat() then return end
	currentProject.kind = k
end

-- Specifies which platforms this project supports. Note: this list is not the
-- exact list of supported platforms in the generated project. The list of
-- platforms this project supports will be the unique list of all combined
-- projects for this SDL solution. Thus, only one project needs to actually
-- maintain a list. This function is additive, that is, everytime it is called
-- it adds it to a unique list of platforms
function SDL_platforms(tbl)
	if not oscompat() then return end
	if not currentProject.platforms then
		currentProject.platforms = { }
	end
	for k,v in pairs(tbl) do
		currentProject.platforms[#currentProject.platforms + 1] = v
	end
end

-- Specifies the programming language of the project, such as C or C++.
function SDL_language(k)
	if not oscompat() then return end
	currentProject.language = k
end

-- Specifies the root directory in which the meta-build system should search for
-- source files, given the paths and files added.
function SDL_sourcedir(src)
	if not oscompat() then return end
	currentProject.sourcedir = src
end

-- Specifies the destination location of where the IDE files related to the
-- project should be saved after generation.
function SDL_projectLocation(loc)
	if not oscompat() then return end
	currentProject.projectLocation = loc
end

-- Specifies a table of files that should be copied from the source directory
-- to the end result build directory of the binary file.
function SDL_copy(tbl)
	if not oscompat() then return end
	currentProject.copy = tbl
end

-- Specifies a list of other SDL projects in this workspace the currently active
-- project is dependent on. If the dependent project is a library, the binary
-- result will be copied from its directory to the build directory of the
-- currently active project automatically.
function SDL_projectDependencies(tbl)
	if not oscompat() then return end
	currentProject.projectDependencies = tbl
end

-- Specifies a list of compiler-level preprocessor definitions that should be
-- set in the resulting project upon compile time. This adds to the current
-- table of defines.
function SDL_defines(tbl)
	if not oscompat() then return end
	if not currentProject.defines then
		currentProject.defines = { }
	end
	for k,v in pairs(tbl) do
		currentProject.defines[#currentProject.defines + 1] = v
	end
end

-- Initializes an outside dependency this project has, such as X11 or DirectX.
-- This function, once invoked, may change the behavior of other SDL
-- project-related functions, so be sure to be familiar with all the functions
-- and any specified behavior when used around SDL_dependency.
function SDL_dependency(name)
	if not oscompat() then return end
	currentDep = { nil, compat = true, }
	currentDep.name = name
	table.insert(currentProject.dependencyTree, currentDep)
end

-- Special function for getting the current OS. This factors in whether the
-- metabuild system is in MinGW, Cygwin, or iOS mode.
function SDL_getos()
	if _OPTIONS["ios"] ~= nil then
		return "ios"
	elseif _OPTIONS["mingw"] ~= nil then
		return "mingw"
	elseif _OPTIONS["cygwin"] ~= nil then
		return "cygwin"
	end
	return os.get()
end

-- Specifies which operating system this dependency targets, such as windows or
-- macosx, as per premake presets.
function SDL_os(name)
	if not oscompat() then return end
	if not currentProject then return end
	if not currentDep then
		currentProject.opsys = name
		currentProject.compat = osmatch(SDL_getos(), name)
	else
		currentDep.opsys = name
		currentDep.compat = osmatch(SDL_getos(), name)
	end
end

-- Specifies which operating system this dependency does not targets. This is
-- for nearly platform-independent projects or dependencies that will not work
-- on specific systems, such as ios.
function SDL_notos(name)
	if not oscompat() then return end
	if not currentProject then return end
	if not currentDep then
		currentProject.opsys = "~" .. name
		currentProject.compat = not osmatch(SDL_getos(), name)
	else
		currentDep.opsys = "~" .. name
		currentDep.compat = not osmatch(SDL_getos(), name)
	end
end

-- Changes the internal state of function compatibility based on whether the
-- current os is the one expected; the next function will be affected by this
-- change, but no others. The name can be a pattern using '|' to separate
-- multiple operating systems, such as:
--   SDL_isos("windows|macosx")
function SDL_isos(name)
	nextFuncCompat = osmatch(SDL_getos(), name)
end

-- Same as SDL_isos, except it negates the internal state for exclusion
-- checking.
function SDL_isnotos(name)
	nextFuncCompat = not osmatch(SDL_getos(), name)
end

-- Changes the internal state of function compatibility based on whether the
-- current system is running a 64bit Operating System and architecture; the
-- next function will be affected by this change, but none thereafter.
function SDL_is64bit()
	nextFuncCompat = os.is64bit()
end

-- Same as SDL_is64bit, except it negates the internal state for
-- exclusion checking.
function SDL_isnot64bit()
	nextFuncCompat = not os.is64bit()
end

-- Look at SDL_depfunc and SDL_notdepfunc for detailed information about this
-- function.
local function SDL_depfunc0(funcname, exclude)
	if not oscompat() then return end
	if not currentDep.compat then return end
	local force = _OPTIONS[funcname:lower()] ~= nil
	local func = dependencyFunctions[funcname:lower()]
	if not func then
		print("Warning: could not find dependency function named: " .. funcname)
		currentDep.compat = false
		return
	end
	local cachedFuncResults = dependencyResults[funcname:lower()]
	local depFound, depInc, depLib, depInput
	if cachedFuncResults then
		depFound = cachedFuncResults.depFound
		-- just skip the rest of the function, the user was already warned
		-- exclude mode varies the compatibility slightly
		if force then
			depFound = true
		end
		if not depFound and not exclude then
			currentDep.compat = false
			return
		elseif depFound and exclude then
			currentDep.compat = false
			return
		end
		depInc = cachedFuncResults.depInc
		depLib = cachedFuncResults.depLib
		depInput = cachedFuncResults.depInput
	else
		local result = func()
		if result.found then
			depFound = result.found
		else
			depFound = false
		end
		if force then
			depFound = true
		end
		if result.incDirs then
			depInc = result.incDirs
		else
			depInc = { }
		end
		if result.libDirs then
			depLib = result.libDirs
		else
			depLib = { }
		end
		if result.libs then
			depInput = result.libs
		else
			depInput = { }
		end
		cachedFuncResults = { }
		cachedFuncResults.depFound = depFound
		cachedFuncResults.depInc = depInc
		cachedFuncResults.depLib = depLib
		cachedFuncResults.depInput = depInput
		dependencyResults[funcname:lower()] = cachedFuncResults
		if not depFound and not exclude then
			currentDep.compat = false
			return
		elseif depFound and exclude then
			currentDep.compat = false
			return
		end
	end
	-- we only want to embed this dependency if we're not in exclude mode
	if depFound and not exclude then
		local dependency = { }
		if not currentDep.includes then
			currentDep.includes = { }
		end
		for k,v in pairs(depInc) do
			currentDep.includes[v] = v
		end
		if not currentDep.libs then
			currentDep.libs = { }
		end
		for k,v in pairs(depLib) do
			currentDep.libs[v] = v
		end
		if not currentDep.links then
			currentDep.links = { }
		end
		for k,v in pairs(depInput) do
			currentDep.links[v] = v
		end
	else -- end of dependency found check
		-- if we are not excluding this dependency, then print a warning
		-- if not found
		if not exclude then
			print("Warning: could not find dependency: " .. funcname)
		end
		currentDep.compat = exclude
	end
end

-- Given a dependency name, this function will register the dependency and try
-- to pair it with a dependency function that was registered through
-- SDL_registerDependencyChecker. If the function is not found, compatibility
-- will automatically be dropped for this project and a warning will be printed
-- to the standard output. Otherwise, the dependency function will be invoked
-- and compatibility for the project will be updated. If the project currently
-- is not compatible based on the Operating System or previous dependency, the
-- dependency function will not be checked at all and this function will
-- silently return.
function SDL_depfunc(funcname)
	SDL_depfunc0(funcname, false)
end

-- Same as SDL_depfunc, except this forces dependency on the function failing,
-- rather than succeeding. This is useful for situations where two different
-- files are required based on whether a dependency is found (such as the
-- joystick and haptic systems).
function SDL_notdepfunc(funcname)
	SDL_depfunc0(funcname, true)
end

-- Determines whether the specified dependency is supported without actually
-- executing the dependency or changing the internal states of the current
-- project or dependency definition. This function will only work if the
-- dependency has already been checked and its results cached within the
-- definition system. This function returns true if the dependency is known to
-- be supported, or false if otherwise (or if it cannot be known at this time).
function SDL_assertdepfunc(funcname)
	-- if forced, then of course it's on
	if _OPTIONS[funcname:lower()] then
		return true
	end
	local results = dependencyResults[funcname:lower()]
	if not results or not results.depFound then
		-- either not excuted yet, doesn't exist, or wasn't found
		print("Warning: required dependency not found: " .. funcname ..
			". Make sure your dependencies are in a logical order.")
		return false
	end
	return true
end

-- Returns a list of currently registered dependencies. The values within the
-- table will be sorted, but their names will be lowercased due to internal
-- handling of case-insensitive dependency names.
function SDL_getDependencies()
	local deps = { }
	for k,_ in pairs(dependencyFunctions) do
		deps[#deps + 1] = k
	end
	table.sort(deps)
	return deps
end

-- Specifies a list of libraries that should always be linked to in this
-- project, regardless of a dependency function. If after a dependency
-- declaration, these files will only be included in the project if the
-- dependency is compatible with the native system, given SDL_os usage and any
-- sort of custom dependency function.
function SDL_links(tbl)
	if not oscompat() then return end
	if currentDep and not currentDep.compat then return end
	if currentProject.customLinks == nil then
		currentProject.customLinks = { }
	end
	for i,v in ipairs(tbl) do
		currentProject.customLinks[#currentProject.customLinks + 1] = v
	end
end

-- Specifies a list of configuration values that are assigned as preprocessor
-- definitions in the SDL configuration header, used to globally configure
-- features during the building of the SDL library. If after a dependency
-- declaration, these files will only be included in the project if the
-- dependency is compatible with the native system, given SDL_os usage and any
-- sort of custom dependency function.
function SDL_config(tbl)
	if not oscompat() then return end
	if not currentDep then
		currentProject.config = tbl
		return
	end
	if not currentDep.compat then return end
	currentDep.config = tbl
end

-- Specifies a list of paths where all .c, .h, and .m files should be included
-- for compiling, where the source directory is the root. If after a dependency
-- declaration, these files will only be included in the project if the
-- dependency is compatible with the native system, given SDL_os usage and any
-- sort of custom dependency function.
function SDL_paths(tbl)
	if not oscompat() then return end
	if not currentDep then
		currentProject.paths = tbl
		return
	end
	if not currentDep.compat then return end
	currentDep.paths = tbl
end

-- Specifies a list of files found within the source directory that this project
-- should include during compile time. If after a dependency declaration, these
-- files will only be included in the project if the dependency is compatible
-- with the native system, given SDL_os usage and any sort of custom dependency
-- function.
function SDL_files(tbl)
	if not oscompat() then return end
	if not currentDep then
		currentProject.files = tbl
		return
	end
	if not currentDep.compat then return end
	currentDep.files = tbl
end
