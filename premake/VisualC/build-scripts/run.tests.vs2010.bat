@echo off

cd ..\VS2010\tests

call :pass checkkeys
call :pass loopwave
call :pass testatomic
call :pass testaudioinfo
call :pass testautomation
call :pass testdraw2
call :pass testchessboard
call :pass testerror
call :pass testfile
call :pass testfilesystem
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
call :pass testresample sample.wav newsample.wav 44100
call :pass testrumble
call :pass testscale
call :pass testsem 1
call :pass testshader
call :testspecial testshape .\shapes
call :testspecial testshape .\shapes
call :testspecial testshape .\shapes
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

:testspecial
if not exist %1\Win32\Debug goto :eof
cd %1\Win32\Debug
call :randomfile %2
cd ..\..\..
call :pass %1 %RETURN%
goto :eof

:: pass label (similar to pass function in the Xcode tests command script)
:pass
setlocal enabledelayedexpansion
set args=
set /A count=0
for %%x IN (%*) DO (
  if NOT !count! EQU 0 set args=!args! %%x
  set /A count=%count% + 1
)
endlocal & set callargs=%args%
:: if it does not exist, break procedure
if not exist %1\Win32\Debug goto endfunc
:: goto directory
echo Testing: %1
title Testing: %1
cd %1\Win32\Debug
:: execute test
".\%1.exe"%callargs%
cd ..\..\..
pause
:endfunc
goto :eof

:randomfile
setlocal enabledelayedexpansion
set count=0
if not exist %1 goto :eof
for %%d in (%1\*.*) DO (
  set /A count=count + 1
)
set /A count=%RANDOM% %% %count%
for %%d in (%1\*.*) DO (
  if !count! EQU 0 (
    set rfile=%%d
    goto endrfile
  )
  set /A count=count-1
)
:endrfile
set tmprfile=!rfile!
endlocal & set RETURN=%tmprfile%
goto :eof