/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_ANDROID

#include "SDL_androidevents.h"
#include "SDL_androidkeyboard.h"
#include "SDL_androidwindow.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"

#include "../../audio/android/SDL_androidaudio.h"
#include "../../audio/aaudio/SDL_aaudio.h"
#include "../../audio/openslES/SDL_openslES.h"


#ifdef SDL_VIDEO_OPENGL_EGL
static void android_egl_context_restore(SDL_Window *window)
{
    if (window) {
        SDL_Event event;
        SDL_WindowData *data = window->internal;
        SDL_GL_MakeCurrent(window, NULL);
        if (SDL_GL_MakeCurrent(window, (SDL_GLContext)data->egl_context) < 0) {
            /* The context is no longer valid, create a new one */
            data->egl_context = (EGLContext)SDL_GL_CreateContext(window);
            SDL_GL_MakeCurrent(window, (SDL_GLContext)data->egl_context);
            event.type = SDL_EVENT_RENDER_DEVICE_RESET;
            event.common.timestamp = 0;
            SDL_PushEvent(&event);
        }
        data->backup_done = SDL_FALSE;

        if (data->has_swap_interval) {
            SDL_GL_SetSwapInterval(data->swap_interval);
        }

    }
}

static void android_egl_context_backup(SDL_Window *window)
{
    if (window) {
        int interval = 0;
        /* Keep a copy of the EGL Context so we can try to restore it when we resume */
        SDL_WindowData *data = window->internal;
        data->egl_context = SDL_GL_GetCurrentContext();

        /* Save/Restore the swap interval / vsync */
        if (SDL_GL_GetSwapInterval(&interval) == 0) {
            data->has_swap_interval = 1;
            data->swap_interval = interval;
        }

        /* We need to do this so the EGLSurface can be freed */
        SDL_GL_MakeCurrent(window, NULL);
        data->backup_done = SDL_TRUE;
    }
}
#endif

/*
 * Android_ResumeSem and Android_PauseSem are signaled from Java_org_libsdl_app_SDLActivity_nativePause and Java_org_libsdl_app_SDLActivity_nativeResume
 */
static SDL_bool Android_EventsInitialized;
static SDL_bool Android_Paused;
static SDL_bool Android_PausedAudio;
static Sint32 Android_PausedWaitTime = -1;

void Android_InitEvents(void)
{
    if (!Android_EventsInitialized) {
        if (SDL_GetHintBoolean(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, SDL_TRUE)) {
            Android_PausedWaitTime = -1;
        } else {
            Android_PausedWaitTime = 100;
        }
        Android_Paused = SDL_FALSE;
        Android_EventsInitialized = SDL_TRUE;
    }
}

void Android_PumpEvents(void)
{
restart:
    if (Android_Paused) {
        if (SDL_WaitSemaphoreTimeout(Android_ResumeSem, Android_PausedWaitTime) == 0) {

            Android_Paused = SDL_FALSE;

            /* Android_ResumeSem was signaled */
            SDL_OnApplicationWillEnterForeground();

            if (Android_PausedAudio) {
                ANDROIDAUDIO_ResumeDevices();
                OPENSLES_ResumeDevices();
                AAUDIO_ResumeDevices();
            }

#ifdef SDL_VIDEO_OPENGL_EGL
            /* Restore the GL Context from here, as this operation is thread dependent */
            if (Android_Window && !Android_Window->external_graphics_context && !SDL_HasEvent(SDL_EVENT_QUIT)) {
                Android_LockActivityMutex();
                android_egl_context_restore(Android_Window);
                Android_UnlockActivityMutex();
            }
#endif

            /* Make sure SW Keyboard is restored when an app becomes foreground */
            if (Android_Window) {
                Android_RestoreScreenKeyboardOnResume(SDL_GetVideoDevice(), Android_Window);
            }

            SDL_OnApplicationDidEnterForeground();
        }
    } else {
        if (SDL_TryWaitSemaphore(Android_PauseSem) == 0) {

            /* Android_PauseSem was signaled */
            SDL_OnApplicationWillEnterBackground();
            SDL_OnApplicationDidEnterBackground();

            /* Make sure we handle potentially multiple pause/resume sequences */
            while (SDL_GetSemaphoreValue(Android_PauseSem) > 0) {
                SDL_WaitSemaphore(Android_ResumeSem);
                SDL_WaitSemaphore(Android_PauseSem);
            }

            /* The semantics are that as soon as the enter background event
             * has been queued, the app will block. The application should
             * do any life cycle handling in an event filter while the event
             * was being queued.
             */
#ifdef SDL_VIDEO_OPENGL_EGL
            if (Android_Window && !Android_Window->external_graphics_context) {
                Android_LockActivityMutex();
                android_egl_context_backup(Android_Window);
                Android_UnlockActivityMutex();
            }
#endif
            if (Android_PausedWaitTime < 0) {
                /* We're blocking, also pause audio */
                ANDROIDAUDIO_PauseDevices();
                OPENSLES_PauseDevices();
                AAUDIO_PauseDevices();
                Android_PausedAudio = SDL_TRUE;
            }

            Android_Paused = SDL_TRUE;
            goto restart;
        }
    }
}

void Android_QuitEvents(void)
{
    Android_EventsInitialized = SDL_FALSE;
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */
