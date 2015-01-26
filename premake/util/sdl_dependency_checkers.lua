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
sdl_dependency_checkers.lua

	This script contains a bunch of functions which determine whether certain
	dependencies exist on the current platform. These functions are able to use
	any and all available utilities for trying to determine both whether the
	dependency is available on this platform, and how to build to the dependency.
	There are a few limitations with these functions, but many of the limitations
	can be mitigated by using the dependency definition functions in the project
	definition files.
	
	Each function in this file, in order to be a valid dependency function, must
	return a table with the following entries:
	
	'found' = boolean value indicating whether the dependency was found
	'incDirs' = table of include directory strings, or nil if none are needed
	'libDirs' = table of library directory strings, or nil if none are needed
	'libs' = table of libraries to link to, or nil if none are needed
	
	All functions must be properly registered with the project definition system
	in order to be properly referenced by projects.
]]

-- dependency functions must return the following:
-- table with an element found, incDirs, libDirs, and libs
function openGLDep()
	print("Checking OpenGL dependencies...")
	if SDL_getos() == "macosx" then
		-- mac should always have support for OpenGL...
		return { found = true, libs = { "OpenGL.framework" } }
	elseif SDL_getos() == "ios" then
		--...unless on iOS
		print("Desktop OpenGL is not supported on iOS targets.")
		return { found = false, libs = { "OpenGL.framework" } }
	elseif SDL_getos() == "cygwin" then
		print("OpenGL is not currently supported on Cygwin.")
		return { found = false, libDirs = { }, libs = { "OpenGL32" } }
	end
	local libpath = nil
  local libname = nil
	if SDL_getos() == "windows" or SDL_getos() == "mingw" then
		libpath = os.findlib("OpenGL32")
		libname = "OpenGL32"
	else -- *nix
		libpath = os.findlib("libGL")
		libname = "GL"
	end
	local foundLib = libpath ~= nil
	-- another way to possibly find the dependency on windows
	--if not foundLib then
	--	foundLib, libpath = find_dependency_dir_windows(nil, "C:/Program Files (x86);C:/Program Files", "Microsoft SDKs", "Lib")
	--end
	if not foundLib then return { found = false } end
	if SDL_getos() == "mingw" then
		libpath = libpath:gsub("\\", "/"):gsub("//", "/")
	end
	return { found = foundLib, libDirs = { }, libs = { libname } }
end

function directXDep()
	print("Checking DirectX dependencies...")
	-- enable this for more correct searching, but it's much slower
	local searchPath = nil --os.getenvpath("ProgramFiles", "ProgramFiles(x86)")
	local foundInc, incpath = find_dependency_dir_windows("DXSDK_DIR", searchPath, "DirectX", "Include")
	local foundLib, libpath = find_dependency_dir_windows("DXSDK_DIR", searchPath, "DirectX", "Lib/x86")
	if not foundInc or not foundLib then return { found = false } end
	-- XXX: hacked mingw check...
	if foundInc and SDL_getos() == "mingw" then
		incpath = incpath:gsub("%$%(DXSDK_DIR%)", os.getenv("DXSDK_DIR")):gsub("\\", "/"):gsub("//", "/")
		libpath = libpath:gsub("%$%(DXSDK_DIR%)", os.getenv("DXSDK_DIR")):gsub("\\", "/"):gsub("//", "/")
	end
	if SDL_getos() == "mingw" then
		print("DirectX is not currently supported on MinGW targets.")
		return { found = false, incDirs = { incpath }, libDirs = { libpath } }
	end
	if SDL_getos() == "cygwin" then
		print("DirectX is not currently supported on Cygwin targets.")
		return { found = false, incDirs = { incpath }, libDirs = { libpath } }
	end
	return { found = true, incDirs = { incpath }, libDirs = { libpath } }
end

function dbusDep()
	print("Checking for D-Bus support...")
	if not check_include_directories("/usr/include/dbus-1.0", "/usr/lib/x86_64-linux-gnu/dbus-1.0/include") then
		print("Warning: D-Bus unsupported!")
		return { found = false }
	end
	return { found = true, incDirs = { "/usr/include/dbus-1.0", "/usr/lib/x86_64-linux-gnu/dbus-1.0/include" } }
end

function alsaDep()
	print("Checking for ALSA support...")
	if not check_include_files("alsa/asoundlib.h")
			or os.findlib("asound") == nil
			or not check_library_exists_lookup("asound", "snd_pcm_open", "alsa/asoundlib.h")
			or not SDL_assertdepfunc("DLOpen") then
		print("Warning: ALSA unsupported!")
		return { found = false }
	end
	return { found = true }
end

function pulseAudioDep()
	print("Checking for PulseAudio support...")
	if os.findlib("libpulse-simple") == nil
			or not SDL_assertdepfunc("DLOpen") then
		print("Warning: PulseAudio unsupported!")
		return { found = false }
	end
	return { found = true }
end

function esdDep()
	print("Checking for ESD support...")
	if os.findlib("esd") == nil
			or not SDL_assertdepfunc("DLOpen") then
		print("Warning: ESD unsupported!")
		return { found = false }
	end
	return { found = true }
end

function nasDep()
	print("Checking for NAS support...")
	if not check_include_file("audio/audiolib.h")
			or not SDL_assertdepfunc("DLOpen") then
		print("Warning: NAS unsupported!")
		return { found = false }
	end
	return { found = true }
end

function ossDep()
	print("Checking for OSS support...")
	if not check_cxx_source_compiles([[
				#include <sys/soundcard.h>
				int main() { int arg = SNDCTL_DSP_SETFRAGMENT; return 0; }]])
			and not check_cxx_source_compiles([[
				#include <soundcard.h>
				int main() { int arg = SNDCTL_DSP_SETFRAGMENT; return 0; }]]) then
		print("Warning: OSS unsupported!")
		return { found = false }
	end
	return { found = true }
end

function dlOpenDep()
	print("Checking for DLOpen support...")
	if not check_library_exists_multiple("dlopen", "dlfcn.h", "dl", "tdl") then
		print("Warning: DLOpen unsupported!")
		return { found = false }
	end
	return { found = true, libs = { "dl" } }
end

function x11Dep()
	print("Checking for X11 support...")
	for _, v in ipairs { "X11", "Xext", "Xcursor", "Xinerama", "Xi", "Xrandr", "Xrender", "Xss", "Xxf86vm" } do
		if os.findlib(v) == nil then
			print("Warning: X11 unsupported!")
			return { found = false }
		end
	end
	if not check_include_files("X11/Xcursor/Xcursor.h", "X11/extensions/Xinerama.h",
			"X11/extensions/XInput2.h", "X11/extensions/Xrandr.h", "X11/extensions/Xrender.h",
			"X11/extensions/scrnsaver.h", "X11/extensions/shape.h", "X11/Xlib.h",
			"X11/extensions/xf86vmode.h") then
		print("Warning: X11 unsupported!")
		return { found = false }
	end
	if not SDL_assertdepfunc("DLOpen") then
		print("Warning: X11 unsupported!")
		return { found = false }
	end
	-- XXX: shared memory check...
	-- there's a LOT more to check to properly configure X11...
	return { found = true, libs = { "X11" } }
end

-- register all of these dependency functions with the definition system
SDL_registerDependencyChecker("OpenGL", openGLDep)
SDL_registerDependencyChecker("DirectX", directXDep)
SDL_registerDependencyChecker("DBus", dbusDep)
SDL_registerDependencyChecker("ALSA", alsaDep)
SDL_registerDependencyChecker("PulseAudio", pulseAudioDep)
SDL_registerDependencyChecker("ESD", esdDep)
SDL_registerDependencyChecker("NAS", nasDep)
SDL_registerDependencyChecker("OSS", ossDep)
SDL_registerDependencyChecker("DLOpen", dlOpenDep)
SDL_registerDependencyChecker("X11", x11Dep)
