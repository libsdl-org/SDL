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
testnative.lua

	This file defines the testnative test project. It depends on the SDL2main
	and SDL2 projects. It will not build on iOS or Cygwin. This project has
	specialized dependencies separate to Windows/MinGW, Mac OS X, and Linux.
]]

SDL_project "testnative"
	SDL_kind "ConsoleApp"
	SDL_notos "ios|cygwin"
	SDL_language "C"
	SDL_sourcedir "../test"
	SDL_projectLocation "tests"
	SDL_projectDependencies { "SDL2main", "SDL2" }
	SDL_files { "/testnative.*" }
	SDL_copy { "icon.bmp" }
	SDL_dependency "windows"
		SDL_os "windows|mingw"
		SDL_files { "/testnativew32.*" }
	SDL_dependency "macosx"
		SDL_os "macosx"
		SDL_files { "/testnativecocoa.*" }
	SDL_dependency "linux"
		SDL_os "linux"
		SDL_depfunc "X11"
		SDL_files { "/testnativex11.*" }
