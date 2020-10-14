/*
  Universal iconv implementation for OS/2.

  Andrey Vasilkin, 2016.
*/

#ifndef GENICONV_H
#define GENICONV_H

#include <iconv.h>

#ifdef iconv_open
#undef iconv_open
#endif
#define iconv_open libiconv_open

#ifdef iconv
#undef iconv
#endif
#define iconv libiconv

#ifdef iconv_close
#undef iconv_close
#endif
#define iconv_close libiconv_close

#define iconv_clean libiconv_clean

// Non-standard function for iconv to unload the used dynamic library.
void libiconv_clean();

iconv_t libiconv_open(const char* tocode, const char* fromcode);
size_t libiconv(iconv_t cd, char* * inbuf, size_t *inbytesleft,
                 char* * outbuf, size_t *outbytesleft);
int libiconv_close(iconv_t cd);

// System codepage <-> UTF-8.

// StrUTF8()
// Coverts string from system cp to UTF-8 (fToUTF8 is not 0) or from UTF-8 to
// the system cp (fToUTF8 is 0). Converted ASCIIZ string will be placed at the
// buffer pcDst, up to cbDst - 1 (for sys->utf8) or 2 (for utf8->sys) bytes.
// Returns the number of bytes written into pcDst, not counting the terminating
// 0 byte(s) or -1 on error.
int StrUTF8(int fToUTF8, char *pcDst, int cbDst, char *pcSrc, int cbSrc);

// StrUTF8New()
// Coverts string from system cp to UTF-8 (fToUTF8 is not 0) or from UTF-8 to
// the system cp (fToUTF8 is 0). Memory for the new string is obtained by
// using libc malloc().
// Returns converted string, terminating two bytes 0 is appended to the result.
// Returns null on error.
char *StrUTF8New(int fToUTF8, char *pcStr, int cbStr);

// StrUTF8Free()
// Deallocates the memory block located by StrUTF8New() (just libc free()).
void StrUTF8Free(char *pszStr);

#endif // GENICONV_H
