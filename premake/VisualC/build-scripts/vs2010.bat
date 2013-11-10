@echo off
cd ..
%~dp0\premake4.exe --file=..\premake4.lua --to=.\VisualC\VS2010 vs2010
pause