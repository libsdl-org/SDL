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
SDL2main.lua

	This file defines the SDL2main project which builds the SDL2main static
	library for Windows and Mac. This project is primarily for everything but
	Linux.
]]

SDL_project "SDL2main"
	SDL_kind "StaticLib"
	SDL_language "C"
	SDL_sourcedir "../src"
	SDL_dependency "windows"
		SDL_os "windows|mingw"
		SDL_paths { "/main/windows/" }
	SDL_dependency "macosx or ios"
		SDL_os "macosx|ios|cygwin"
		SDL_paths { "/main/dummy/" }
