dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([unittest/testquit.c])

dnl Detect the canonical build and host environments
AC_CONFIG_AUX_DIR([../build-scripts])
AC_CANONICAL_HOST

dnl Check for tools
AC_PROG_CC

dnl Figure out which math or extra library to use
case "$host" in
    *-*-cygwin* | *-*-mingw*)
        EXE=".exe"
        EXTRALIB="-lshlwapi"
        ;;
    *)
        EXE=""
        EXTRALIB=""
        ;;
esac
AC_SUBST(EXE)

dnl Check for SDL
SDL_VERSION=2.0.0
AM_PATH_SDL2($SDL_VERSION,
            :,
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)
CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$LIBS -lSDL2_test $SDL_LIBS $EXTRALIB"

PKG_CHECK_MODULES(LIBUNWIND, libunwind, have_libunwind=yes, have_libunwind=no)
if test x$have_libunwind = xyes ; then
   LIBS="$LIBS $LIBUNWIND_LIBS"
fi

dnl Finally create all the generated files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
