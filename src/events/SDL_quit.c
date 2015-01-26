/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../SDL_internal.h"

/* General quit handling code for SDL */

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include "SDL_events.h"
#include "SDL_events_c.h"


#ifdef HAVE_SIGNAL_H
static void
SDL_HandleSIG(int sig)
{
    /* Reset the signal handler */
    signal(sig, SDL_HandleSIG);

    /* Signal a quit interrupt */
    SDL_SendQuit();
}
#endif /* HAVE_SIGNAL_H */

/* Public functions */
int
SDL_QuitInit(void)
{
#ifdef HAVE_SIGACTION
    struct sigaction action;
    sigaction(SIGINT, NULL, &action);
#ifdef HAVE_SA_SIGACTION
    if ( action.sa_handler == SIG_DFL && action.sa_sigaction == (void*)SIG_DFL ) {
#else
    if ( action.sa_handler == SIG_DFL ) {
#endif
        action.sa_handler = SDL_HandleSIG;
        sigaction(SIGINT, &action, NULL);
    }
    sigaction(SIGTERM, NULL, &action);

#ifdef HAVE_SA_SIGACTION
    if ( action.sa_handler == SIG_DFL && action.sa_sigaction == (void*)SIG_DFL ) {
#else
    if ( action.sa_handler == SIG_DFL ) {
#endif
        action.sa_handler = SDL_HandleSIG;
        sigaction(SIGTERM, &action, NULL);
    }
#elif HAVE_SIGNAL_H
    void (*ohandler) (int);

    /* Both SIGINT and SIGTERM are translated into quit interrupts */
    ohandler = signal(SIGINT, SDL_HandleSIG);
    if (ohandler != SIG_DFL)
        signal(SIGINT, ohandler);
    ohandler = signal(SIGTERM, SDL_HandleSIG);
    if (ohandler != SIG_DFL)
        signal(SIGTERM, ohandler);
#endif /* HAVE_SIGNAL_H */

    /* That's it! */
    return (0);
}

void
SDL_QuitQuit(void)
{
#ifdef HAVE_SIGACTION
    struct sigaction action;
    sigaction(SIGINT, NULL, &action);
    if ( action.sa_handler == SDL_HandleSIG ) {
        action.sa_handler = SIG_DFL;
        sigaction(SIGINT, &action, NULL);
    }
    sigaction(SIGTERM, NULL, &action);
    if ( action.sa_handler == SDL_HandleSIG ) {
        action.sa_handler = SIG_DFL;
        sigaction(SIGTERM, &action, NULL);
    }
#elif HAVE_SIGNAL_H
    void (*ohandler) (int);

    ohandler = signal(SIGINT, SIG_DFL);
    if (ohandler != SDL_HandleSIG)
        signal(SIGINT, ohandler);
    ohandler = signal(SIGTERM, SIG_DFL);
    if (ohandler != SDL_HandleSIG)
        signal(SIGTERM, ohandler);
#endif /* HAVE_SIGNAL_H */
}

/* This function returns 1 if it's okay to close the application window */
int
SDL_SendQuit(void)
{
    return SDL_SendAppEvent(SDL_QUIT);
}

/* vi: set ts=4 sw=4 expandtab: */
