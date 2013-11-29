@echo off
cd ..
%~dp0\premake4.exe --file=..\premake4.lua --to=.\VisualC\VS2008 clean
%~dp0\premake4.exe --file=..\premake4.lua --to=.\VisualC\VS2010 clean
%~dp0\premake4.exe --file=..\premake4.lua --to=.\VisualC\VS2012 clean
if exist VS2008 rmdir VS2008
if exist VS2010 rmdir VS2010
if exist VS2012 rmdir VS2012
pause