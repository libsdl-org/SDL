if %2.==one. goto setxboxone
rem Xbox Series compile
set DXC="%GameDKCoreLatest%xbox\bin\gen9\DXC.exe"
set SUFFIX=_Series.h
goto startbuild

:setxboxone
set DXC="%GameDKCoreLatest%xbox\bin\gen8\DXC.exe"
set SUFFIX=_One.h

:startbuild

call "%~dp0\compile_shaders.bat"
