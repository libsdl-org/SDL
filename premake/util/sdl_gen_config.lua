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
sdl_gen_config.lua

	Given a series of set configuration values from the project definitions,
	this file contains a series of functions that generate valid preprocessor
	definitions to enable or disable various features of the SDL2 library. These
	definitions are pasted into a template SDL config header file, which is then
	saved in the local directory and referenced to in generated project files.

	This file depends on sdl_file.lua.
]]

-- The line that must exist in the template file in order to properly paste
-- the generated definitions.
local searchKey = "/%* Paste generated code here %*/\n"

local configFile, templateFileContents
local insertLocation

-- This function begins config header generation given the name of the generated
-- file and the name of the template file to use.
function startGeneration(file, template)
	configFile = fileopen(file, "wt")
	templateFileContents = readfile(template, "rt")
	insertLocation = templateFileContents:find(searchKey)
	if insertLocation then
		configFile:write(templateFileContents:sub(1, insertLocation - 1))
	end
end

-- Adds a table of configuration values to the generated file. Each
-- configuration line is wrapped around a preprocessor definition check, so they
-- can be manually overwritten by the developer if necessary. The definition
-- pastes string versions of both the key and the value on the line, where
-- either is allowed to be empty. That means the table stores key-value pairs.
function addConfig(tbl)
	-- if no insert location, don't paste anything
	if not insertLocation then return end
	for k,v in pairs(tbl) do
		configFile:print(0, "#ifndef " .. k)
		configFile:print(0, "#define " .. tostring(k) .. " " .. tostring(v))
		configFile:print(0, "#endif")
	end
end

-- Finishes the generation and writes the remains of the template file into the
-- generated config file.
function endGeneration()
	if insertLocation then
		configFile:write(templateFileContents:sub(insertLocation + #searchKey - 2))
	else -- write entire file since nothing is being pasted
		configFile:write(templateFileContents)
	end
	configFile:close()
end
