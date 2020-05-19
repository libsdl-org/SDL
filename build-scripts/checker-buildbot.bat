rem This is checker-buildbot.sh, simplified into a Windows .bat file
rem We assume a lot of non-standard Windows stuff is in your PATH:
rem  cmake, ninja, clang, perl...

erase /q /f /s %1 checker-buildbot analysis
mkdir checker-buildbot
cd checker-buildbot

# We turn off deprecated declarations, because we don't care about these warnings during static analysis.
# The -Wno-liblto is new since our checker-279 upgrade, I think; checker otherwise warns "libLTO.dylib relative to clang installed dir not found"

scan-build -o analysis cmake -G Ninja -Wno-dev -DSDL_STATIC=OFF -DCMAKE_BUILD_TYPE=Debug -DASSERTIONS=enabled -DCMAKE_C_FLAGS="-Wno-deprecated-declarations" -DCMAKE_SHARED_LINKER_FLAGS="-Wno-liblto" ..

erase /q /f /s analysis
scan-build -o analysis ninja

for /F %%i in ('dir /b /a "analysis\*"') do (
    goto packageit
)

mkdir analysis\zarro
echo '<html><head><title>Zarro boogs</title></head><body>Static analysis: no issues to report.</body></html>' >analysis\zarro\index.html

packageit:
move analysis\* ..\analysis
rmdir analysis   # Make sure this is empty.
cd ..

move analysis %1

erase /q /f /s checker-buildbot

echo "Done"

rem end of checker-buildbot.bat ...

