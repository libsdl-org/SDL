#ifndef SDL_os2_h_
#define SDL_os2_h_

#include "SDL_log.h"
#include "SDL_stdinc.h"
#include "geniconv/geniconv.h"

#ifdef OS2DEBUG
#if (OS2DEBUG-0 >= 2)
# define debug(s,...) SDL_LogDebug( SDL_LOG_CATEGORY_APPLICATION, \
                                    __func__"(): "##s, ##__VA_ARGS__ )
#else
# define debug(s,...) printf( __func__"(): "##s"\n", ##__VA_ARGS__ )
#endif

#else

# define debug(s,...)

#endif /* OS2DEBUG */


// StrUTF8New() - geniconv/sys2utf8.c.
#define OS2_SysToUTF8(S) StrUTF8New( 1, S, SDL_strlen( S ) + 1 )
#define OS2_UTF8ToSys(S) StrUTF8New( 0, (char *)(S), SDL_strlen( S ) + 1 )

// SDL_OS2Quit() will be called from SDL_QuitSubSystem().
void SDL_OS2Quit();

#endif /* SDL_os2_h_ */
