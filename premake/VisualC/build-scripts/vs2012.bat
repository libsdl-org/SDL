@echo off
cd ..
%~dp0\premake4.exe --file=..\premake4.lua --to=.\VisualC\VS2012 vs2012
pause