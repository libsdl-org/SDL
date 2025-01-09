@echo off

cd ..\tests

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
call :pass testsem 1
call :pass testshader
call :pass testshape sample.bmp
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
if not exist %1\Win32\Release goto endfunc
:: goto directory
echo Testing: %1
title Testing: %1
cd %1\Win32\Debug
:: execute test
".\%1.exe" %2
cd ..\..\..
pause
:endfunc
goto :eof