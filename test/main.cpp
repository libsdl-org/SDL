/*
 * This file is supposed to be used to build tests on platforms that require
 * the main function to be implemented in C++, which means that SDL_main's
 * implementation needs C++ and thus can't be included in test*.c
 *
 * Placed in the public domain by Daniel Gibson, 2022-12-12
 */

#include <SDL3/SDL_main.h>

// that's all, folks!
