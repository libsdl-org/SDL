
/**
 * Automated SDL_IOStream test.
 *
 * Original code written by Edgar Simo "bobbens"
 * Ported by Markus Kauppila (markus.kauppila@gmail.com)
 * Updated and extended for SDL_test by aschiffler at ferzkopp dot net
 *
 * Released under Public Domain.
 */

/* quiet windows compiler warnings */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

static const char *IOStreamReadTestFilename = "iostrm_read";
static const char *IOStreamWriteTestFilename = "iostrm_write";
static const char *IOStreamAlphabetFilename = "iostrm_alphabet";

static const char IOStreamHelloWorldTestString[] = "Hello World!";
static const char IOStreamHelloWorldCompString[] = "Hello World!";
static const char IOStreamAlphabetString[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/* Fixture */

static void SDLCALL IOStreamSetUp(void **arg)
{
    size_t fileLen;
    FILE *handle;
    size_t writtenLen;
    int result;

    /* Clean up from previous runs (if any); ignore errors */
    (void)remove(IOStreamReadTestFilename);
    (void)remove(IOStreamWriteTestFilename);
    (void)remove(IOStreamAlphabetFilename);

    /* Create a test file */
    handle = fopen(IOStreamReadTestFilename, "w");
    SDLTest_AssertCheck(handle != NULL, "Verify creation of file '%s' returned non NULL handle", IOStreamReadTestFilename);
    if (handle == NULL) {
        return;
    }

    /* Write some known text into it */
    fileLen = SDL_strlen(IOStreamHelloWorldTestString);
    writtenLen = fwrite(IOStreamHelloWorldTestString, 1, fileLen, handle);
    SDLTest_AssertCheck(fileLen == writtenLen, "Verify number of written bytes, expected %i, got %i", (int)fileLen, (int)writtenLen);
    result = fclose(handle);
    SDLTest_AssertCheck(result == 0, "Verify result from fclose, expected 0, got %i", result);

    /* Create a second test file */
    handle = fopen(IOStreamAlphabetFilename, "w");
    SDLTest_AssertCheck(handle != NULL, "Verify creation of file '%s' returned non NULL handle", IOStreamAlphabetFilename);
    if (handle == NULL) {
        return;
    }

    /* Write alphabet text into it */
    fileLen = SDL_strlen(IOStreamAlphabetString);
    writtenLen = fwrite(IOStreamAlphabetString, 1, fileLen, handle);
    SDLTest_AssertCheck(fileLen == writtenLen, "Verify number of written bytes, expected %i, got %i", (int)fileLen, (int)writtenLen);
    result = fclose(handle);
    SDLTest_AssertCheck(result == 0, "Verify result from fclose, expected 0, got %i", result);

    SDLTest_AssertPass("Creation of test file completed");
}

static void SDLCALL IOStreamTearDown(void *arg)
{
    int result;

    /* Remove the created files to clean up; ignore errors for write filename */
    result = remove(IOStreamReadTestFilename);
    SDLTest_AssertCheck(result == 0, "Verify result from remove(%s), expected 0, got %i", IOStreamReadTestFilename, result);
    (void)remove(IOStreamWriteTestFilename);
    result = remove(IOStreamAlphabetFilename);
    SDLTest_AssertCheck(result == 0, "Verify result from remove(%s), expected 0, got %i", IOStreamAlphabetFilename, result);

    SDLTest_AssertPass("Cleanup of test files completed");
}

/**
 * Makes sure parameters work properly. Local helper function.
 *
 * \sa SDL_SeekIO
 * \sa SDL_ReadIO
 */
static void testGenericIOStreamValidations(SDL_IOStream *rw, bool write)
{
    char buf[sizeof(IOStreamHelloWorldTestString)];
    Sint64 i;
    size_t s;
    int seekPos = SDLTest_RandomIntegerInRange(4, 8);

    /* Clear buffer */
    SDL_zeroa(buf);

    /* Set to start. */
    i = SDL_SeekIO(rw, 0, SDL_IO_SEEK_SET);
    SDLTest_AssertPass("Call to SDL_SeekIO succeeded");
    SDLTest_AssertCheck(i == (Sint64)0, "Verify seek to 0 with SDL_SeekIO (SDL_IO_SEEK_SET), expected 0, got %" SDL_PRIs64, i);

    /* Test write */
    s = SDL_WriteIO(rw, IOStreamHelloWorldTestString, sizeof(IOStreamHelloWorldTestString) - 1);
    SDLTest_AssertPass("Call to SDL_WriteIO succeeded");
    if (write) {
        SDLTest_AssertCheck(s == sizeof(IOStreamHelloWorldTestString) - 1, "Verify result of writing with SDL_WriteIO, expected %i, got %i", (int)sizeof(IOStreamHelloWorldTestString) - 1, (int)s);
    } else {
        SDLTest_AssertCheck(s == 0, "Verify result of writing with SDL_WriteIO, expected: 0, got %i", (int)s);
    }

    /* Test seek to random position */
    i = SDL_SeekIO(rw, seekPos, SDL_IO_SEEK_SET);
    SDLTest_AssertPass("Call to SDL_SeekIO succeeded");
    SDLTest_AssertCheck(i == (Sint64)seekPos, "Verify seek to %i with SDL_SeekIO (SDL_IO_SEEK_SET), expected %i, got %" SDL_PRIs64, seekPos, seekPos, i);

    /* Test seek back to start */
    i = SDL_SeekIO(rw, 0, SDL_IO_SEEK_SET);
    SDLTest_AssertPass("Call to SDL_SeekIO succeeded");
    SDLTest_AssertCheck(i == (Sint64)0, "Verify seek to 0 with SDL_SeekIO (SDL_IO_SEEK_SET), expected 0, got %" SDL_PRIs64, i);

    /* Test read */
    s = SDL_ReadIO(rw, buf, sizeof(IOStreamHelloWorldTestString) - 1);
    SDLTest_AssertPass("Call to SDL_ReadIO succeeded");
    SDLTest_AssertCheck(
        s == (sizeof(IOStreamHelloWorldTestString) - 1),
        "Verify result from SDL_ReadIO, expected %i, got %i",
        (int)(sizeof(IOStreamHelloWorldTestString) - 1),
        (int)s);
    SDLTest_AssertCheck(
        SDL_memcmp(buf, IOStreamHelloWorldTestString, sizeof(IOStreamHelloWorldTestString) - 1) == 0,
        "Verify read bytes match expected string, expected '%s', got '%s'", IOStreamHelloWorldTestString, buf);

    /* Test seek back to start */
    i = SDL_SeekIO(rw, 0, SDL_IO_SEEK_SET);
    SDLTest_AssertPass("Call to SDL_SeekIO succeeded");
    SDLTest_AssertCheck(i == (Sint64)0, "Verify seek to 0 with SDL_SeekIO (SDL_IO_SEEK_SET), expected 0, got %" SDL_PRIs64, i);

    /* Test printf */
    s = SDL_IOprintf(rw, "%s", IOStreamHelloWorldTestString);
    SDLTest_AssertPass("Call to SDL_IOprintf succeeded");
    if (write) {
        SDLTest_AssertCheck(s == sizeof(IOStreamHelloWorldTestString) - 1, "Verify result of writing with SDL_IOprintf, expected %i, got %i", (int)sizeof(IOStreamHelloWorldTestString) - 1, (int)s);
    } else {
        SDLTest_AssertCheck(s == 0, "Verify result of writing with SDL_WriteIO, expected: 0, got %i", (int)s);
    }

    /* Test seek back to start */
    i = SDL_SeekIO(rw, 0, SDL_IO_SEEK_SET);
    SDLTest_AssertPass("Call to SDL_SeekIO succeeded");
    SDLTest_AssertCheck(i == (Sint64)0, "Verify seek to 0 with SDL_SeekIO (SDL_IO_SEEK_SET), expected 0, got %" SDL_PRIs64, i);

    /* Test read */
    s = SDL_ReadIO(rw, buf, sizeof(IOStreamHelloWorldTestString) - 1);
    SDLTest_AssertPass("Call to SDL_ReadIO succeeded");
    SDLTest_AssertCheck(
        s == (sizeof(IOStreamHelloWorldTestString) - 1),
        "Verify result from SDL_ReadIO, expected %i, got %i",
        (int)(sizeof(IOStreamHelloWorldTestString) - 1),
        (int)s);
    SDLTest_AssertCheck(
        SDL_memcmp(buf, IOStreamHelloWorldTestString, sizeof(IOStreamHelloWorldTestString) - 1) == 0,
        "Verify read bytes match expected string, expected '%s', got '%s'", IOStreamHelloWorldTestString, buf);

    /* More seek tests. */
    i = SDL_SeekIO(rw, -4, SDL_IO_SEEK_CUR);
    SDLTest_AssertPass("Call to SDL_SeekIO(...,-4,SDL_IO_SEEK_CUR) succeeded");
    SDLTest_AssertCheck(
        i == (Sint64)(sizeof(IOStreamHelloWorldTestString) - 5),
        "Verify seek to -4 with SDL_SeekIO (SDL_IO_SEEK_CUR), expected %i, got %i",
        (int)(sizeof(IOStreamHelloWorldTestString) - 5),
        (int)i);

    i = SDL_SeekIO(rw, -1, SDL_IO_SEEK_END);
    SDLTest_AssertPass("Call to SDL_SeekIO(...,-1,SDL_IO_SEEK_END) succeeded");
    SDLTest_AssertCheck(
        i == (Sint64)(sizeof(IOStreamHelloWorldTestString) - 2),
        "Verify seek to -1 with SDL_SeekIO (SDL_IO_SEEK_END), expected %i, got %i",
        (int)(sizeof(IOStreamHelloWorldTestString) - 2),
        (int)i);

    /* Invalid whence seek */
    i = SDL_SeekIO(rw, 0, (SDL_IOWhence)999);
    SDLTest_AssertPass("Call to SDL_SeekIO(...,0,invalid_whence) succeeded");
    SDLTest_AssertCheck(
        i == (Sint64)(-1),
        "Verify seek with SDL_SeekIO (invalid_whence); expected: -1, got %i",
        (int)i);
}

/**
 * Negative test for SDL_IOFromFile parameters
 *
 * \sa SDL_IOFromFile
 *
 */
static int SDLCALL iostrm_testParamNegative(void *arg)
{
    SDL_IOStream *iostrm;

    /* These should all fail. */
    iostrm = SDL_IOFromFile(NULL, NULL);
    SDLTest_AssertPass("Call to SDL_IOFromFile(NULL, NULL) succeeded");
    SDLTest_AssertCheck(iostrm == NULL, "Verify SDL_IOFromFile(NULL, NULL) returns NULL");

    iostrm = SDL_IOFromFile(NULL, "ab+");
    SDLTest_AssertPass("Call to SDL_IOFromFile(NULL, \"ab+\") succeeded");
    SDLTest_AssertCheck(iostrm == NULL, "Verify SDL_IOFromFile(NULL, \"ab+\") returns NULL");

    iostrm = SDL_IOFromFile(NULL, "sldfkjsldkfj");
    SDLTest_AssertPass("Call to SDL_IOFromFile(NULL, \"sldfkjsldkfj\") succeeded");
    SDLTest_AssertCheck(iostrm == NULL, "Verify SDL_IOFromFile(NULL, \"sldfkjsldkfj\") returns NULL");

    iostrm = SDL_IOFromFile("something", "");
    SDLTest_AssertPass("Call to SDL_IOFromFile(\"something\", \"\") succeeded");
    SDLTest_AssertCheck(iostrm == NULL, "Verify SDL_IOFromFile(\"something\", \"\") returns NULL");

    iostrm = SDL_IOFromFile("something", NULL);
    SDLTest_AssertPass("Call to SDL_IOFromFile(\"something\", NULL) succeeded");
    SDLTest_AssertCheck(iostrm == NULL, "Verify SDL_IOFromFile(\"something\", NULL) returns NULL");

    iostrm = SDL_IOFromMem(NULL, 10);
    SDLTest_AssertPass("Call to SDL_IOFromMem(NULL, 10) succeeded");
    SDLTest_AssertCheck(iostrm == NULL, "Verify SDL_IOFromMem(NULL, 10) returns NULL");

    iostrm = SDL_IOFromMem((void *)IOStreamAlphabetString, 0);
    SDLTest_AssertPass("Call to SDL_IOFromMem(data, 0) succeeded");
    SDLTest_AssertCheck(iostrm == NULL, "Verify SDL_IOFromMem(data, 0) returns NULL");

    iostrm = SDL_IOFromConstMem((const void *)IOStreamAlphabetString, 0);
    SDLTest_AssertPass("Call to SDL_IOFromConstMem(data, 0) succeeded");
    SDLTest_AssertCheck(iostrm == NULL, "Verify SDL_IOFromConstMem(data, 0) returns NULL");

    return TEST_COMPLETED;
}

/**
 * Tests opening from memory.
 *
 * \sa SDL_IOFromMem
 * \sa SDL_CloseIO
 */
static int SDLCALL iostrm_testMem(void *arg)
{
    char mem[sizeof(IOStreamHelloWorldTestString)];
    SDL_IOStream *rw;
    int result;

    /* Clear buffer */
    SDL_zeroa(mem);

    /* Open */
    rw = SDL_IOFromMem(mem, sizeof(IOStreamHelloWorldTestString) - 1);
    SDLTest_AssertPass("Call to SDL_IOFromMem() succeeded");
    SDLTest_AssertCheck(rw != NULL, "Verify opening memory with SDL_IOFromMem does not return NULL");

    /* Bail out if NULL */
    if (rw == NULL) {
        return TEST_ABORTED;
    }

    /* Run generic tests */
    testGenericIOStreamValidations(rw, true);

    /* Close */
    result = SDL_CloseIO(rw);
    SDLTest_AssertPass("Call to SDL_CloseIO() succeeded");
    SDLTest_AssertCheck(result == true, "Verify result value is true; got: %d", result);

    return TEST_COMPLETED;
}

/**
 * Tests opening from memory.
 *
 * \sa SDL_IOFromConstMem
 * \sa SDL_CloseIO
 */
static int SDLCALL iostrm_testConstMem(void *arg)
{
    SDL_IOStream *rw;
    int result;

    /* Open handle */
    rw = SDL_IOFromConstMem(IOStreamHelloWorldCompString, sizeof(IOStreamHelloWorldCompString) - 1);
    SDLTest_AssertPass("Call to SDL_IOFromConstMem() succeeded");
    SDLTest_AssertCheck(rw != NULL, "Verify opening memory with SDL_IOFromConstMem does not return NULL");

    /* Bail out if NULL */
    if (rw == NULL) {
        return TEST_ABORTED;
    }

    /* Run generic tests */
    testGenericIOStreamValidations(rw, false);

    /* Close handle */
    result = SDL_CloseIO(rw);
    SDLTest_AssertPass("Call to SDL_CloseIO() succeeded");
    SDLTest_AssertCheck(result == true, "Verify result value is true; got: %d", result);

    return TEST_COMPLETED;
}

/**
 * Tests dynamic memory
 *
 * \sa SDL_IOFromDynamicMem
 * \sa SDL_CloseIO
 */
static int SDLCALL iostrm_testDynamicMem(void *arg)
{
    SDL_IOStream *rw;
    SDL_PropertiesID props;
    char *mem;
    int result;

    /* Open */
    rw = SDL_IOFromDynamicMem();
    SDLTest_AssertPass("Call to SDL_IOFromDynamicMem() succeeded");
    SDLTest_AssertCheck(rw != NULL, "Verify opening memory with SDL_IOFromDynamicMem does not return NULL");

    /* Bail out if NULL */
    if (rw == NULL) {
        return TEST_ABORTED;
    }

    /* Set the chunk size to 1 byte */
    props = SDL_GetIOProperties(rw);
    SDL_SetNumberProperty(props, SDL_PROP_IOSTREAM_DYNAMIC_CHUNKSIZE_NUMBER, 1);

    /* Run generic tests */
    testGenericIOStreamValidations(rw, true);

    /* Get the dynamic memory and verify it */
    mem = (char *)SDL_GetPointerProperty(props, SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
    SDLTest_AssertPass("Call to SDL_GetPointerProperty(props, SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL) succeeded");
    SDLTest_AssertCheck(mem != NULL, "Verify memory value is not NULL");
    mem[SDL_GetIOSize(rw)] = '\0';
    SDLTest_AssertCheck(SDL_strcmp(mem, IOStreamHelloWorldTestString) == 0, "Verify memory value is correct");

    /* Take the memory and free it ourselves */
    SDL_SetPointerProperty(props, SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
    SDL_free(mem);

    /* Close */
    result = SDL_CloseIO(rw);
    SDLTest_AssertPass("Call to SDL_CloseIO() succeeded");
    SDLTest_AssertCheck(result == true, "Verify result value is true; got: %d", result);

    return TEST_COMPLETED;
}

/**
 * Tests reading from file.
 *
 * \sa SDL_IOFromFile
 * \sa SDL_CloseIO
 */
static int SDLCALL iostrm_testFileRead(void *arg)
{
    SDL_IOStream *rw;
    int result;

    /* Read test. */
    rw = SDL_IOFromFile(IOStreamReadTestFilename, "r");
    SDLTest_AssertPass("Call to SDL_IOFromFile(..,\"r\") succeeded");
    SDLTest_AssertCheck(rw != NULL, "Verify opening file with SDL_IOFromFile in read mode does not return NULL");

    /* Bail out if NULL */
    if (rw == NULL) {
        return TEST_ABORTED;
    }

    /* Run generic tests */
    testGenericIOStreamValidations(rw, false);

    /* Close handle */
    result = SDL_CloseIO(rw);
    SDLTest_AssertPass("Call to SDL_CloseIO() succeeded");
    SDLTest_AssertCheck(result == true, "Verify result value is true; got: %d", result);

    return TEST_COMPLETED;
}

/**
 * Tests writing from file.
 *
 * \sa SDL_IOFromFile
 * \sa SDL_CloseIO
 */
static int SDLCALL iostrm_testFileWrite(void *arg)
{
    SDL_IOStream *rw;
    int result;

    /* Write test. */
    rw = SDL_IOFromFile(IOStreamWriteTestFilename, "w+");
    SDLTest_AssertPass("Call to SDL_IOFromFile(..,\"w+\") succeeded");
    SDLTest_AssertCheck(rw != NULL, "Verify opening file with SDL_IOFromFile in write mode does not return NULL");

    /* Bail out if NULL */
    if (rw == NULL) {
        return TEST_ABORTED;
    }

    /* Run generic tests */
    testGenericIOStreamValidations(rw, true);

    /* Close handle */
    result = SDL_CloseIO(rw);
    SDLTest_AssertPass("Call to SDL_CloseIO() succeeded");
    SDLTest_AssertCheck(result == true, "Verify result value is true; got: %d", result);

    return TEST_COMPLETED;
}

/**
 * Tests alloc and free RW context.
 *
 * \sa SDL_OpenIO
 * \sa SDL_CloseIO
 */
static int SDLCALL iostrm_testAllocFree(void *arg)
{
    /* Allocate context */
    SDL_IOStreamInterface iface;
    SDL_IOStream *rw;

    SDL_INIT_INTERFACE(&iface);
    rw = SDL_OpenIO(&iface, NULL);
    SDLTest_AssertPass("Call to SDL_OpenIO() succeeded");
    SDLTest_AssertCheck(rw != NULL, "Validate result from SDL_OpenIO() is not NULL");
    if (rw == NULL) {
        return TEST_ABORTED;
    }

    /* Free context again */
    SDL_CloseIO(rw);
    SDLTest_AssertPass("Call to SDL_CloseIO() succeeded");

    return TEST_COMPLETED;
}

/**
 * Compare memory and file reads
 *
 * \sa SDL_IOFromMem
 * \sa SDL_IOFromFile
 */
static int SDLCALL iostrm_testCompareRWFromMemWithRWFromFile(void *arg)
{
    int slen = 26;
    char buffer_file[27];
    char buffer_mem[27];
    size_t rv_file;
    size_t rv_mem;
    Uint64 sv_file;
    Uint64 sv_mem;
    SDL_IOStream *iostrm_file;
    SDL_IOStream *iostrm_mem;
    int size;
    int result;

    for (size = 5; size < 10; size++) {
        /* Terminate buffer */
        buffer_file[slen] = 0;
        buffer_mem[slen] = 0;

        /* Read/seek from memory */
        iostrm_mem = SDL_IOFromMem((void *)IOStreamAlphabetString, slen);
        SDLTest_AssertPass("Call to SDL_IOFromMem()");
        rv_mem = SDL_ReadIO(iostrm_mem, buffer_mem, size * 6);
        SDLTest_AssertPass("Call to SDL_ReadIO(mem, size=%d)", size * 6);
        sv_mem = SDL_SeekIO(iostrm_mem, 0, SEEK_END);
        SDLTest_AssertPass("Call to SDL_SeekIO(mem,SEEK_END)");
        result = SDL_CloseIO(iostrm_mem);
        SDLTest_AssertPass("Call to SDL_CloseIO(mem)");
        SDLTest_AssertCheck(result == true, "Verify result value is true; got: %d", result);

        /* Read/see from file */
        iostrm_file = SDL_IOFromFile(IOStreamAlphabetFilename, "r");
        SDLTest_AssertPass("Call to SDL_IOFromFile()");
        rv_file = SDL_ReadIO(iostrm_file, buffer_file, size * 6);
        SDLTest_AssertPass("Call to SDL_ReadIO(file, size=%d)", size * 6);
        sv_file = SDL_SeekIO(iostrm_file, 0, SEEK_END);
        SDLTest_AssertPass("Call to SDL_SeekIO(file,SEEK_END)");
        result = SDL_CloseIO(iostrm_file);
        SDLTest_AssertPass("Call to SDL_CloseIO(file)");
        SDLTest_AssertCheck(result == true, "Verify result value is true; got: %d", result);

        /* Compare */
        SDLTest_AssertCheck(rv_mem == rv_file, "Verify returned read blocks matches for mem and file reads; got: rv_mem=%d rv_file=%d", (int)rv_mem, (int)rv_file);
        SDLTest_AssertCheck(sv_mem == sv_file, "Verify SEEK_END position matches for mem and file seeks; got: sv_mem=%d sv_file=%d", (int)sv_mem, (int)sv_file);
        SDLTest_AssertCheck(buffer_mem[slen] == 0, "Verify mem buffer termination; expected: 0, got: %d", buffer_mem[slen]);
        SDLTest_AssertCheck(buffer_file[slen] == 0, "Verify file buffer termination; expected: 0, got: %d", buffer_file[slen]);
        SDLTest_AssertCheck(
            SDL_strncmp(buffer_mem, IOStreamAlphabetString, slen) == 0,
            "Verify mem buffer contain alphabet string; expected: %s, got: %s", IOStreamAlphabetString, buffer_mem);
        SDLTest_AssertCheck(
            SDL_strncmp(buffer_file, IOStreamAlphabetString, slen) == 0,
            "Verify file buffer contain alphabet string; expected: %s, got: %s", IOStreamAlphabetString, buffer_file);
    }

    return TEST_COMPLETED;
}

/**
 * Tests writing and reading from file using endian aware functions.
 *
 * \sa SDL_IOFromFile
 * \sa SDL_CloseIO
 * \sa SDL_ReadU16BE
 * \sa SDL_WriteU16BE
 */
static int SDLCALL iostrm_testFileWriteReadEndian(void *arg)
{
    SDL_IOStream *rw;
    Sint64 result;
    int mode;
    Uint16 BE16value;
    Uint32 BE32value;
    Uint64 BE64value;
    Uint16 LE16value;
    Uint32 LE32value;
    Uint64 LE64value;
    Uint16 BE16test;
    Uint32 BE32test;
    Uint64 BE64test;
    Uint16 LE16test;
    Uint32 LE32test;
    Uint64 LE64test;
    bool bresult;
    int cresult;

    for (mode = 0; mode < 3; mode++) {

        /* Create test data */
        switch (mode) {
        default:
        case 0:
            SDLTest_Log("All 0 values");
            BE16value = 0;
            BE32value = 0;
            BE64value = 0;
            LE16value = 0;
            LE32value = 0;
            LE64value = 0;
            break;
        case 1:
            SDLTest_Log("All 1 values");
            BE16value = 1;
            BE32value = 1;
            BE64value = 1;
            LE16value = 1;
            LE32value = 1;
            LE64value = 1;
            break;
        case 2:
            SDLTest_Log("Random values");
            BE16value = SDLTest_RandomUint16();
            BE32value = SDLTest_RandomUint32();
            BE64value = SDLTest_RandomUint64();
            LE16value = SDLTest_RandomUint16();
            LE32value = SDLTest_RandomUint32();
            LE64value = SDLTest_RandomUint64();
            break;
        }

        /* Write test. */
        rw = SDL_IOFromFile(IOStreamWriteTestFilename, "w+");
        SDLTest_AssertPass("Call to SDL_IOFromFile(..,\"w+\")");
        SDLTest_AssertCheck(rw != NULL, "Verify opening file with SDL_IOFromFile in write mode does not return NULL");

        /* Bail out if NULL */
        if (rw == NULL) {
            return TEST_ABORTED;
        }

        /* Write test data */
        bresult = SDL_WriteU16BE(rw, BE16value);
        SDLTest_AssertPass("Call to SDL_WriteU16BE()");
        SDLTest_AssertCheck(bresult == true, "Validate object written, expected: true, got: false");
        bresult = SDL_WriteU32BE(rw, BE32value);
        SDLTest_AssertPass("Call to SDL_WriteU32BE()");
        SDLTest_AssertCheck(bresult == true, "Validate object written, expected: true, got: false");
        bresult = SDL_WriteU64BE(rw, BE64value);
        SDLTest_AssertPass("Call to SDL_WriteU64BE()");
        SDLTest_AssertCheck(bresult == true, "Validate object written, expected: true, got: false");
        bresult = SDL_WriteU16LE(rw, LE16value);
        SDLTest_AssertPass("Call to SDL_WriteU16LE()");
        SDLTest_AssertCheck(bresult == true, "Validate object written, expected: true, got: false");
        bresult = SDL_WriteU32LE(rw, LE32value);
        SDLTest_AssertPass("Call to SDL_WriteU32LE()");
        SDLTest_AssertCheck(bresult == true, "Validate object written, expected: true, got: false");
        bresult = SDL_WriteU64LE(rw, LE64value);
        SDLTest_AssertPass("Call to SDL_WriteU64LE()");
        SDLTest_AssertCheck(bresult == true, "Validate object written, expected: true, got: false");

        /* Test seek to start */
        result = SDL_SeekIO(rw, 0, SDL_IO_SEEK_SET);
        SDLTest_AssertPass("Call to SDL_SeekIO succeeded");
        SDLTest_AssertCheck(result == 0, "Verify result from position 0 with SDL_SeekIO, expected 0, got %i", (int)result);

        /* Read test data */
        bresult = SDL_ReadU16BE(rw, &BE16test);
        SDLTest_AssertPass("Call to SDL_ReadU16BE()");
        SDLTest_AssertCheck(bresult == true, "Validate object read, expected: true, got: false");
        SDLTest_AssertCheck(BE16test == BE16value, "Validate object read from SDL_ReadU16BE, expected: %hu, got: %hu", BE16value, BE16test);
        bresult = SDL_ReadU32BE(rw, &BE32test);
        SDLTest_AssertPass("Call to SDL_ReadU32BE()");
        SDLTest_AssertCheck(bresult == true, "Validate object read, expected: true, got: false");
        SDLTest_AssertCheck(BE32test == BE32value, "Validate object read from SDL_ReadU32BE, expected: %" SDL_PRIu32 ", got: %" SDL_PRIu32, BE32value, BE32test);
        bresult = SDL_ReadU64BE(rw, &BE64test);
        SDLTest_AssertPass("Call to SDL_ReadU64BE()");
        SDLTest_AssertCheck(bresult == true, "Validate object read, expected: true, got: false");
        SDLTest_AssertCheck(BE64test == BE64value, "Validate object read from SDL_ReadU64BE, expected: %" SDL_PRIu64 ", got: %" SDL_PRIu64, BE64value, BE64test);
        bresult = SDL_ReadU16LE(rw, &LE16test);
        SDLTest_AssertPass("Call to SDL_ReadU16LE()");
        SDLTest_AssertCheck(bresult == true, "Validate object read, expected: true, got: false");
        SDLTest_AssertCheck(LE16test == LE16value, "Validate object read from SDL_ReadU16LE, expected: %hu, got: %hu", LE16value, LE16test);
        bresult = SDL_ReadU32LE(rw, &LE32test);
        SDLTest_AssertPass("Call to SDL_ReadU32LE()");
        SDLTest_AssertCheck(bresult == true, "Validate object read, expected: true, got: false");
        SDLTest_AssertCheck(LE32test == LE32value, "Validate object read from SDL_ReadU32LE, expected: %" SDL_PRIu32 ", got: %" SDL_PRIu32, LE32value, LE32test);
        bresult = SDL_ReadU64LE(rw, &LE64test);
        SDLTest_AssertPass("Call to SDL_ReadU64LE()");
        SDLTest_AssertCheck(bresult == true, "Validate object read, expected: true, got: false");
        SDLTest_AssertCheck(LE64test == LE64value, "Validate object read from SDL_ReadU64LE, expected: %" SDL_PRIu64 ", got: %" SDL_PRIu64, LE64value, LE64test);

        /* Close handle */
        cresult = SDL_CloseIO(rw);
        SDLTest_AssertPass("Call to SDL_CloseIO() succeeded");
        SDLTest_AssertCheck(cresult == true, "Verify result value is true; got: %d", cresult);
    }

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* IOStream test cases */
static const SDLTest_TestCaseReference iostrmTest1 = {
    iostrm_testParamNegative, "iostrm_testParamNegative", "Negative test for SDL_IOFromFile parameters", TEST_ENABLED
};

static const SDLTest_TestCaseReference iostrmTest2 = {
    iostrm_testMem, "iostrm_testMem", "Tests opening from memory", TEST_ENABLED
};

static const SDLTest_TestCaseReference iostrmTest3 = {
    iostrm_testConstMem, "iostrm_testConstMem", "Tests opening from (const) memory", TEST_ENABLED
};

static const SDLTest_TestCaseReference iostrmTest4 = {
    iostrm_testDynamicMem, "iostrm_testDynamicMem", "Tests opening dynamic memory", TEST_ENABLED
};

static const SDLTest_TestCaseReference iostrmTest5 = {
    iostrm_testFileRead, "iostrm_testFileRead", "Tests reading from a file", TEST_ENABLED
};

static const SDLTest_TestCaseReference iostrmTest6 = {
    iostrm_testFileWrite, "iostrm_testFileWrite", "Test writing to a file", TEST_ENABLED
};

static const SDLTest_TestCaseReference iostrmTest7 = {
    iostrm_testAllocFree, "iostrm_testAllocFree", "Test alloc and free of RW context", TEST_ENABLED
};

static const SDLTest_TestCaseReference iostrmTest8 = {
    iostrm_testFileWriteReadEndian, "iostrm_testFileWriteReadEndian", "Test writing and reading via the Endian aware functions", TEST_ENABLED
};

static const SDLTest_TestCaseReference iostrmTest9 = {
    iostrm_testCompareRWFromMemWithRWFromFile, "iostrm_testCompareRWFromMemWithRWFromFile", "Compare RWFromMem and RWFromFile IOStream for read and seek", TEST_ENABLED
};

/* Sequence of IOStream test cases */
static const SDLTest_TestCaseReference *iostrmTests[] = {
    &iostrmTest1, &iostrmTest2, &iostrmTest3, &iostrmTest4, &iostrmTest5, &iostrmTest6,
    &iostrmTest7, &iostrmTest8, &iostrmTest9, NULL
};

/* IOStream test suite (global) */
SDLTest_TestSuiteReference iostrmTestSuite = {
    "IOStream",
    IOStreamSetUp,
    iostrmTests,
    IOStreamTearDown
};
