#ifndef _SDL_os2_h
#define _SDL_os2_h

#include "SDL_log.h"
#include "SDL_stdinc.h"
#include ".\src\core\os2\geniconv\geniconv.h"

#if OS2DEBUG==SDLOUTPUT

# define debug(s,...) SDL_LogDebug( SDL_LOG_CATEGORY_APPLICATION, \
                                    __func__"(): "##s, ##__VA_ARGS__ )

#elif defined(OS2DEBUG)

# define debug(s,...) printf( __func__"(): "##s"\n", ##__VA_ARGS__ )

#else

# define debug(s,...)

#endif // OS2DEBUG


// StrUTF8New() - geniconv\sys2utf8.c.
#define OS2_SysToUTF8(S) StrUTF8New( 1, S, SDL_strlen( S ) + 1 )
#define OS2_UTF8ToSys(S) StrUTF8New( 0, (char *)(S), SDL_strlen( S ) + 1 )

// SDL_OS2Quit() will be called from SDL_QuitSubSystem().
void SDL_OS2Quit();

#endif // _SDL_os2_h
