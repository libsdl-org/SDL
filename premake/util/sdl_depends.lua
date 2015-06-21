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

-- This is kept just for windows because the other platforms use different means
-- for determining dependence or compatibility.

--[[
sdl_depends.lua

	This file simply contains a function for determining whether a dependency
	exists on the Windows platform, given a possible environmental variable,
	delimited search paths, and a main and/or sub-directory paths for more
	elaborate pattern matching.
]]

-- find_dependency_dir_windows(env, main_search_path, main_dir_path)
--   Attempt to resolve a dependency (true or false) folder based on either an
--   environmental variable, start search path, or both. If both are present,
--   the environmental variable will be preferred. If neither are present, this
--   function returns false.
--
--   Arguments:
--     env                The name of the environmental variable to treat as a path
--     main_search_paths  Paths to look for the main directory in
--     main_dir_path      The a path that must be contained between main_search_path and sub_dir_path
--     sub_dir_path       The path of the directories that should exist at the searched path
function find_dependency_dir_windows(env, main_search_paths, main_dir_path, sub_dir_path)
	if not os.is("windows") then -- if not windows, then fail
		return false
	end
	if env == nil and (main_search_paths == nil or #main_search_paths == 0) then
		return false
	end
	local env_path = nil
	local main_path = nil
	if env ~= nil then env_path = os.getenv(env) end
	local search_table = { n = 0 }
	if main_search_paths ~= nil then
		for k,main_search_path in ipairs(explode(main_search_paths, ";")) do
			local directories = os.matchdirs(main_search_path .. "/**" .. main_dir_path .. "*")
			for k,v in pairs(directories) do
				table.insert(search_table, v)
			end
		end
	end
	if env_path ~= nil then table.insert(search_table, env_path) end
	local search_path = table.concat(search_table, ";")
	local result_path = os.dirpathsearch(sub_dir_path, search_path, ";")
	if result_path == nil then
		return false
	end
	local found_dir = os.isdir(result_path)
	local abs_path = path.getabsolute(result_path)
	if found_dir and env_path ~= nil then
		abs_path = abs_path:gsub("\\", "/")
		env_path = env_path:gsub("\\", "/")
		local pos = abs_path:indexOf(env_path)
		if pos ~= nil then
			abs_path = abs_path:sub(1, pos - 1) .. "$(" .. env .. ")/" .. abs_path:sub(pos + #env_path)
		end
	end
	-- we want the path in terms of '/'
	return found_dir, abs_path
end
