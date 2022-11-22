#!/bin/sh

if test "x$CC" = "x"; then
    CC=cc
fi

machine="$($CC -dumpmachine)"
case "$machine" in
    *mingw* )
        EXEPREFIX=""
        EXESUFFIX=".exe"
        ;;
    *android* )
        EXEPREFIX="lib"
        EXESUFFIX=".so"
        EXTRA_LDFLAGS="$EXTRA_LDFLAGS -shared"
        ;;
    * )
        EXEPREFIX=""
        EXESUFFIX=""
        ;;
esac

set -e

# Get the canonical path of the folder containing this script
testdir=$(cd -P -- "$(dirname -- "$0")" && printf '%s\n' "$(pwd -P)")
CFLAGS="$( pkg-config sdl3 --cflags )"
LDFLAGS="$( pkg-config sdl3 --libs )"
STATIC_LDFLAGS="$( pkg-config sdl3 --libs --static )"

compile_cmd="$CC -c "$testdir/main_gui.c" -o main_gui_pkgconfig.c.o $CFLAGS $EXTRA_CFLAGS"
link_cmd="$CC main_gui_pkgconfig.c.o -o ${EXEPREFIX}main_gui_pkgconfig${EXESUFFIX} $LDFLAGS $EXTRA_LDFLAGS"
static_link_cmd="$CC main_gui_pkgconfig.c.o -o ${EXEPREFIX}main_gui_pkgconfig_static${EXESUFFIX} $STATIC_LDFLAGS $EXTRA_LDFLAGS"

echo "-- CC:                $CC"
echo "-- CFLAGS:            $CFLAGS"
echo "-- EXTRA_CFLAGS:      $EXTRA_CFLAGS"
echo "-- LDFLASG:           $LDFLAGS"
echo "-- STATIC_LDFLAGS:    $STATIC_LDFLAGS"
echo "-- EXTRA_LDFLAGS:     $EXTRA_LDFLAGS"

echo "-- COMPILE:       $compile_cmd"
echo "-- LINK:          $link_cmd"
echo "-- STATIC_LINK:   $static_link_cmd"

set -x

$compile_cmd
$link_cmd
$static_link_cmd
