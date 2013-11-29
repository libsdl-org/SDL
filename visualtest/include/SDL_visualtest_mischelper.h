/**
 * \file mischelper.c 
 *
 * Header with miscellaneous helper functions.
 */

#ifndef _SDL_visualtest_mischelper_h
#define _SDL_visualtest_mischelper_h

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Stores a 32 digit hexadecimal string representing the MD5 hash of the
 * string \c str in \c hash.
 */
void SDLVisualTest_HashString(char* str, char hash[33]);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* _SDL_visualtest_mischelper_h */