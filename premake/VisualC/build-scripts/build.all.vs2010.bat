@echo off
cd ..\VS2010
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
msbuild /m SDL.sln /property:Configuration=Debug
pause