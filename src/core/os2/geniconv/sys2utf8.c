#include "geniconv.h"
#include <stdlib.h>

int StrUTF8(int fToUTF8, char *pcDst, int cbDst, char *pcSrc, int cbSrc)
{
  size_t     rc;
  char       *pcDstStart = pcDst;
  iconv_t    cd;
  char       *pszToCP, *pszFromCP;
  int        fError = 0;

  if ( cbDst < 4 )
    return -1;

  if ( fToUTF8 )
  {
    pszToCP   = "UTF-8";
    pszFromCP = "";
  }
  else
  {
    pszToCP   = "";
    pszFromCP = "UTF-8";
  }

  cd = iconv_open( pszToCP, pszFromCP );
  if ( cd == (iconv_t)-1 )
    return -1;

  while( cbSrc > 0 )
  {
    rc = iconv( cd, &pcSrc, (size_t *)&cbSrc, &pcDst, (size_t *)&cbDst );
    if ( rc == (size_t)-1 )
    {
      if ( errno == EILSEQ )
      {
        // Try to skip invalid character.
        pcSrc++;
        cbSrc--;
        continue;
      }

      fError = 1;
      break;
    }
  }

  iconv_close( cd );

  // Write trailing ZERO (1 byte for UTF-8, 2 bytes for the system cp).
  if ( fToUTF8 )
  {
    if ( cbDst < 1 )
    {
      pcDst--;
      fError = 1;      // The destination buffer overflow.
    }
    *pcDst = '\0';
  }
  else
  {
    if ( cbDst < 2 )
    {
      pcDst -= ( cbDst == 0 ) ? 2 : 1;
      fError = 1;      // The destination buffer overflow.
    }
    *((short *)pcDst) = '\0';
  }

  return fError ? -1 : ( pcDst - pcDstStart );
}

char *StrUTF8New(int fToUTF8, char *pcStr, int cbStr)
{
  int        cbNewStr = ( ( cbStr > 4 ? cbStr : 4 ) + 1 ) * 2;
  char       *pszNewStr = malloc( cbNewStr );

  if ( pszNewStr == NULL )
    return NULL;

  cbNewStr = StrUTF8( fToUTF8, pszNewStr, cbNewStr, pcStr, cbStr );
  if ( cbNewStr != -1 )
  {
    pcStr = realloc( pszNewStr, cbNewStr + ( fToUTF8 ? 1 : sizeof(short) ) );
    if ( pcStr != NULL )
      return pcStr;
  }

  free( pszNewStr );
  return NULL;
}

void StrUTF8Free(char *pszStr)
{
  free( pszStr );
}
