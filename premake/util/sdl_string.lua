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
sdl_string.lua

	Contains a few convenient string utility functions which are not supported in
	Lua or not supported as intended.
]]

-- Performs a non-pattern based substring search of one string in another
-- string. It's of O(n^2) complexity. It returns nil if the result cannot be
-- found, otherwise it returns the starting index of the first found occurrence.
string.indexOf = function(str, substr)
	local pos = 1
	local i = 1
	for i = 1, str:len(), 1 do
		if str:sub(i, i) == substr:sub(pos, pos) then
			-- have we matched the complete string?
			if pos == substr:len() then
				return i - pos + 1-- starting pos
			end
			-- matched character...keep going
			pos = pos + 1
		else
			-- restart, no match
			pos = 0
		end
	end
	if pos == substr:len() then
		return i - pos + 1
	end
	return nil -- no match
end

-- This is a public-access version of the explode function defined below.
function explode(str, delim)
	return str:explode(delim)
end

-- Explodes a string into an array of elements, separated by a non-pattern
-- delimiter. This function is part of the string table, allowing for a
-- member-based invocation for strings.
string.explode = function(str, delim)
	local exploded = { }
	local needle = string.find(str, delim)
	while needle ~= nil do
		table.insert(exploded, string.sub(str, 0, needle - 1))
		str = string.sub(str, needle + 1)
		needle = string.find(str, delim)
	end
	table.insert(exploded, str)
	return exploded
end

-- Similar to table.concat, except it supports more advanced token pasting. This
-- function is vastly used by the main meta-build script (premake4.lua) to
-- generate all the main lines of code for various tables that need to be in the
-- generated lua file.
--  - tbl: table of values to implode into a string
--  - prefix: string to paste before entire result
--  - pre: string to always paste before each entry in table
--  - post: string to always paste after each entry in table
--  - join: string to paste between entries (inclusive)
--  - suffix: string to paste after entire result
-- Returns the imploded string.
function implode(tbl, prefix, pre, post, join, suffix)
	local result = ""
	-- not the most efficient way to do this, but...
	local itbl = { }
	for k,v in pairs(tbl) do
		itbl[#itbl + 1] = v
	end
	for i = 1, #itbl, 1 do
		if pre ~= nil then
			result = result .. pre
		end
		result = result .. itbl[i]
		if post ~= nil then
			result = result .. post
		end
		if i ~= #itbl then
			result = result .. join
		end
	end
	if prefix ~= nil then
		result = prefix .. result
	end
	if suffix ~= nil then
		result = result .. suffix
	end
	return result
end
