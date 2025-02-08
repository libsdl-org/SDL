/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* sanity tests on SDL_iostream.c (useful for alternative implementations of stdio iostream) */

/* quiet windows compiler warnings */
#if defined(_MSC_VER) && !defined(_CRT_NONSTDC_NO_WARNINGS)
#define _CRT_NONSTDC_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

/* WARNING ! those 2 files will be destroyed by this test program */

#ifdef SDL_PLATFORM_IOS
#define FBASENAME1 "../Documents/sdldata1" /* this file will be created during tests */
#define FBASENAME2 "../Documents/sdldata2" /* this file should not exist before starting test */
#else
#define FBASENAME1 "sdldata1" /* this file will be created during tests */
#define FBASENAME2 "sdldata2" /* this file should not exist before starting test */
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

static SDLTest_CommonState *state;

static void
cleanup(void)
{
    unlink(FBASENAME1);
    unlink(FBASENAME2);
}

static void
iostrm_error_quit(unsigned line, SDL_IOStream *iostrm)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "testfile.c(%d): failed", line);
    if (iostrm) {
        SDL_CloseIO(iostrm);
    }
    cleanup();
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    exit(1); /* quit with iostrm error (test failed) */
}

#define RWOP_ERR_QUIT(x) iostrm_error_quit(__LINE__, (x))

int main(int argc, char *argv[])
{
    SDL_IOStream *iostrm = NULL;
    char test_buf[30];

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    cleanup();

    /* test 1 : basic argument test: all those calls to SDL_IOFromFile should fail */

    iostrm = SDL_IOFromFile(NULL, NULL);
    if (iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    iostrm = SDL_IOFromFile(NULL, "ab+");
    if (iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    iostrm = SDL_IOFromFile(NULL, "sldfkjsldkfj");
    if (iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    iostrm = SDL_IOFromFile("something", "");
    if (iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    iostrm = SDL_IOFromFile("something", NULL);
    if (iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    SDL_Log("test1 OK");

    /* test 2 : check that inexistent file is not successfully opened/created when required */
    /* modes : r, r+ imply that file MUST exist
       modes : a, a+, w, w+ checks that it succeeds (file may not exists)

     */
    iostrm = SDL_IOFromFile(FBASENAME2, "rb"); /* this file doesn't exist that call must fail */
    if (iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    iostrm = SDL_IOFromFile(FBASENAME2, "rb+"); /* this file doesn't exist that call must fail */
    if (iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    iostrm = SDL_IOFromFile(FBASENAME2, "wb");
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    SDL_CloseIO(iostrm);
    unlink(FBASENAME2);
    iostrm = SDL_IOFromFile(FBASENAME2, "wb+");
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    SDL_CloseIO(iostrm);
    unlink(FBASENAME2);
    iostrm = SDL_IOFromFile(FBASENAME2, "ab");
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    SDL_CloseIO(iostrm);
    unlink(FBASENAME2);
    iostrm = SDL_IOFromFile(FBASENAME2, "ab+");
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    SDL_CloseIO(iostrm);
    unlink(FBASENAME2);
    SDL_Log("test2 OK");

    /* test 3 : creation, writing , reading, seeking,
                test : w mode, r mode, w+ mode
     */
    iostrm = SDL_IOFromFile(FBASENAME1, "wb"); /* write only */
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (10 != SDL_WriteIO(iostrm, "1234567890", 10)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (10 != SDL_WriteIO(iostrm, "1234567890", 10)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (7 != SDL_WriteIO(iostrm, "1234567", 7)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm); /* we are in write only mode */
    }

    SDL_CloseIO(iostrm);

    iostrm = SDL_IOFromFile(FBASENAME1, "rb"); /* read mode, file must exist */
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (20 != SDL_SeekIO(iostrm, -7, SDL_IO_SEEK_END)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (7 != SDL_ReadIO(iostrm, test_buf, 7)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (SDL_memcmp(test_buf, "1234567", 7) != 0) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1000)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, -27, SDL_IO_SEEK_CUR)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (27 != SDL_ReadIO(iostrm, test_buf, 30)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (SDL_memcmp(test_buf, "12345678901234567890", 20) != 0) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_WriteIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm); /* readonly mode */
    }

    SDL_CloseIO(iostrm);

    /* test 3: same with w+ mode */
    iostrm = SDL_IOFromFile(FBASENAME1, "wb+"); /* write + read + truncation */
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (10 != SDL_WriteIO(iostrm, "1234567890", 10)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (10 != SDL_WriteIO(iostrm, "1234567890", 10)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (7 != SDL_WriteIO(iostrm, "1234567", 7)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (1 != SDL_ReadIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm); /* we are in read/write mode */
    }

    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (20 != SDL_SeekIO(iostrm, -7, SDL_IO_SEEK_END)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (7 != SDL_ReadIO(iostrm, test_buf, 7)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (SDL_memcmp(test_buf, "1234567", 7) != 0) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1000)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, -27, SDL_IO_SEEK_CUR)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (27 != SDL_ReadIO(iostrm, test_buf, 30)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (SDL_memcmp(test_buf, "12345678901234567890", 20) != 0) {
        RWOP_ERR_QUIT(iostrm);
    }
    SDL_CloseIO(iostrm);
    SDL_Log("test3 OK");

    /* test 4: same in r+ mode */
    iostrm = SDL_IOFromFile(FBASENAME1, "rb+"); /* write + read + file must exists, no truncation */
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (10 != SDL_WriteIO(iostrm, "1234567890", 10)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (10 != SDL_WriteIO(iostrm, "1234567890", 10)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (7 != SDL_WriteIO(iostrm, "1234567", 7)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (1 != SDL_ReadIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm); /* we are in read/write mode */
    }

    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (20 != SDL_SeekIO(iostrm, -7, SDL_IO_SEEK_END)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (7 != SDL_ReadIO(iostrm, test_buf, 7)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (SDL_memcmp(test_buf, "1234567", 7) != 0) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1000)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, -27, SDL_IO_SEEK_CUR)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (27 != SDL_ReadIO(iostrm, test_buf, 30)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (SDL_memcmp(test_buf, "12345678901234567890", 20) != 0) {
        RWOP_ERR_QUIT(iostrm);
    }
    SDL_CloseIO(iostrm);
    SDL_Log("test4 OK");

    /* test5 : append mode */
    iostrm = SDL_IOFromFile(FBASENAME1, "ab+"); /* write + read + append */
    if (!iostrm) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (10 != SDL_WriteIO(iostrm, "1234567890", 10)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (10 != SDL_WriteIO(iostrm, "1234567890", 10)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (7 != SDL_WriteIO(iostrm, "1234567", 7)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }

    if (1 != SDL_ReadIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }

    if (20 + 27 != SDL_SeekIO(iostrm, -7, SDL_IO_SEEK_END)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (7 != SDL_ReadIO(iostrm, test_buf, 7)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (SDL_memcmp(test_buf, "1234567", 7) != 0) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (0 != SDL_ReadIO(iostrm, test_buf, 1000)) {
        RWOP_ERR_QUIT(iostrm);
    }

    if (27 != SDL_SeekIO(iostrm, -27, SDL_IO_SEEK_CUR)) {
        RWOP_ERR_QUIT(iostrm);
    }

    if (0 != SDL_SeekIO(iostrm, 0L, SDL_IO_SEEK_SET)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (30 != SDL_ReadIO(iostrm, test_buf, 30)) {
        RWOP_ERR_QUIT(iostrm);
    }
    if (SDL_memcmp(test_buf, "123456789012345678901234567123", 30) != 0) {
        RWOP_ERR_QUIT(iostrm);
    }
    SDL_CloseIO(iostrm);
    SDL_Log("test5 OK");
    cleanup();
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0; /* all ok */
}
