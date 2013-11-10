@echo off
title Building Premake VS2010 Environment...
call build.all.vs2010.bat

title Building SDL VS2010 Environment...
cd %~dp0
cd ..\..\..\VisualC
msbuild /m SDL_VS2010.sln /t:Clean,Build /property:Configuration=Debug
pause

title Running SDL VS2010 tests with premake SDL2.dll...
cd tests

call :pass checkkeys
call :pass loopwave
call :pass testatomic
call :pass testaudioinfo
call :pass testautomation
call :pass testdraw2
call :pass testerror
call :pass testfile
call :pass testgamecontroller
call :pass testgesture
call :pass testgl2
call :pass testgles
call :pass testhaptic
call :pass testiconv
call :pass testime
call :pass testintersection
call :pass testjoystick
call :pass testkeys
::call :pass testloadso
call :pass testlock
call :pass testmessage
call :pass testmultiaudio
call :pass testnative
call :pass testoverlay2
call :pass testplatform
call :pass testpower
call :pass testrelative
call :pass testrendercopyex
call :pass testrendertarget
::call :pass testresample
call :pass testrumble
call :pass testscale
call :pass testsem 0
call :pass testshader
call :pass testshape "../../../../../../test/sample.bmp"
call :pass testsprite2
call :pass testspriteminimal
call :pass teststreaming
call :pass testthread
call :pass testtimer
call :pass testver
call :pass testwm2
call :pass torturethread

:: leave the tests directory
cd ..

:: exit batch
goto :eof

:: pass label (similar to pass function in the Xcode tests command script)
:pass
:: if it does not exist, break procedure
if not exist %1\Win32\Debug goto endfunc
:: goto directory
echo Running SDL VS2010 %1 with premake SDL2.dll...
title Running SDL VS2010 %1 with premake SDL2.dll...
cd %1\Win32\Debug
:: remove old SDL2.dll
rm SDL2.dll
:: copy new SDL2.dll (~dp0 is get directory of current location of batch file)
copy %~dp0\..\SDL2\Win32\Debug\SDL2.dll .\SDL2.dll
:: execute test
".\%1.exe" %2
cd ..\..\..
pause
:endfunc
goto :eof