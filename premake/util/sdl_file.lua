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
sdl_file.lua

	This function contains a wrapper for the I/O file operations, providing a few
	custom functions which simplify the file I/O process (especially useful for
	the vast amount of generation used by the meta-build system).
]]

-- Given a filename and open mode (look at io.open for more information), opens
-- the file with various contained functions for printing to the file, writing
-- to the file, reading from the file, or closing the file. If the filename is
-- nil, then this will open a file in a special text mode. In that case, the
-- mode is ignored. Returned is an instanced table with all of the
-- aforementioned functions.
--
-- The print function is associated with textprint/fileprint, the write function
-- with textwrite/filewrite, the read function with fileread, and the close
-- function with textclose/fileclose.
function fileopen(file, mode)
	if file == nil then
		return { texth = "", print = textprint, write = textwrite, read = nil, close = textclose }
	else
		return { fileh = io.open(file, mode), print = fileprint, write = filewrite, read = fileread, close = fileclose }
	end
end

-- Given a filename and file mode, reads the entire contents of the file and
-- returns the contents as a string.
function readfile(file, mode)
	local file = fileopen(file, mode)
	local content = file:read()
	file:close()
	return content
end

-- Given a file, the number of tabs to indent, and a line to print, append the
-- line tabbed n times with an appended newline to the end of the input text.
function textprint(f, tabs, line)
	for i = 0, tabs - 1, 1 do
		f.texth = f.texth .. "\t"
	end
	f.texth = f.texth .. line .. "\n"
end

-- Given a file, the number of tabs to indent, and a line to print, append the
-- line tabbed n times with an appended newline to the end of the input file.
function fileprint(f, tabs, line)
	for i = 0, tabs - 1, 1 do
		f.fileh:write("\t")
	end
	f.fileh:write(line .. "\n")
end

-- Given a file and some text, append the text to the end of the input text.
function textwrite(f, text)
	f.texth = f.texth .. text
end

-- Given a file and some text, append the text to the end of the input file.
function filewrite(f, text)
	f.fileh:write(text)
end

-- Given a file, read all the contents of the file and return them as a string.
function fileread(file)
	return file.fileh:read("*all")
end

-- Given a file opened in text mode, return the result of the current file
-- operations as a text string.
function textclose(file)
	return file.texth
end

-- Given a file opened regularly, close the file handle resource, preventing
-- any future I/O operations.
function fileclose(file)
	file.fileh:close()
end

-- Given a source path, builds a table containing all directories and recursive
-- subdirectories which contain files, and returns the table. Each entry in the
-- table will have a '/' at the end of its path, plus they will all be relative
-- to the parent source path. The table will contain a single entry with the
-- value '/' to indicate the source path itself.
function createDirTable(sourcePath)
	local dirs = os.matchdirs(sourcePath.."/**")
	for k,d in pairs(dirs) do
		dirs[k] = string.sub(d, #sourcePath + 1) .. "/"
	end
	table.insert(dirs, "/")
	return dirs
end

-- This works like os.pathsearch, but for directories. Look at the premake
-- documentation for os.pathsearch for more information.
os.dirpathsearch = function(subdir, path, path_delimiter)
	for i,p in ipairs(explode(path, path_delimiter)) do
		local needle = p .. "/" .. subdir
		if os.isdir(needle) then
			return needle
		end
	end
	return nil
end

-- Given a variable number of environmental variable names, this will join them
-- together based on the current OS path delimeter and quietly ignoring those
-- variables which do not exist on this system. The resulting path is always
-- normalized for Unix-based path separators, regardless of the system.
os.getenvpath = function(...)
	local path = ""
	local pathDelimeter = ":"
	if os.is("windows") then
		pathDelimeter = ";"
	end
	for i,a in ipairs(arg) do
		local value = os.getenv(a)
		if value then
			if #path > 0 then
				path = path .. pathDelimeter
			end
			path = path .. value
		end
	end
	-- normalize path to unix
	return path:gsub("\\", "/"):gsub("//", "/")
end
