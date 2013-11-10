@echo off
cd ..
%~dp0\premake4.exe --file=..\premake4.lua --to=.\MinGW --mingw gmake
pause