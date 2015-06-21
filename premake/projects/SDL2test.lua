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
SDL2test.lua

	This file defines the SDL2test library. It depends on the SDL2main and SDL2
	projects. This library contains a series of test functions used by many of the
	other test projects, so it is one of the main dependencies for much of the
	test suite.
]]

SDL_project "SDL2test"
	SDL_kind "StaticLib"
	SDL_language "C"
	SDL_sourcedir "../src"
	SDL_projectDependencies { "SDL2main", "SDL2" }
	SDL_paths { "/test/" }
