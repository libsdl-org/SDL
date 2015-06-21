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
testshader.lua

	This file defines the testshader test project. It depends on the SDL2main
	and SDL2 projects. It will not build on iOS or Cygwin. This project has a
	dependency on OpenGL.
]]

SDL_project "testshader"
	SDL_kind "ConsoleApp"
	SDL_notos "ios|cygwin"
	SDL_language "C"
	SDL_sourcedir "../test"
	SDL_projectLocation "tests"
	-- a list of items to copy from the sourcedir to the destination
	SDL_copy { "icon.bmp" }
	SDL_projectDependencies { "SDL2main", "SDL2" }
	SDL_defines { "HAVE_OPENGL" }
	SDL_dependency "OpenGL"
		-- opengl is platform independent
		SDL_depfunc "OpenGL"
		SDL_files { "/testshader.*" }
