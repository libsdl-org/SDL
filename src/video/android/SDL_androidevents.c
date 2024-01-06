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

/* Number of 'type' events in the event queue */
static int SDL_NumberOfEvents(Uint32 type)
{
    return SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, type, type);
}

#ifdef SDL_VIDEO_OPENGL_EGL
static void android_egl_context_restore(SDL_Window *window)
{
    if (window) {
        SDL_Event event;
        SDL_WindowData *data = window->driverdata;
        SDL_GL_MakeCurrent(window, NULL);
        if (SDL_GL_MakeCurrent(window, (SDL_GLContext)data->egl_context) < 0) {
            /* The context is no longer valid, create a new one */
            data->egl_context = (EGLContext)SDL_GL_CreateContext(window);
            SDL_GL_MakeCurrent(window, (SDL_GLContext)data->egl_context);
            event.type = SDL_EVENT_RENDER_DEVICE_RESET;
            event.common.timestamp = 0;
            SDL_PushEvent(&event);
        }
        data->backup_done = 0;

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
        SDL_WindowData *data = window->driverdata;
        data->egl_context = SDL_GL_GetCurrentContext();

        /* Save/Restore the swap interval / vsync */
        if (SDL_GL_GetSwapInterval(&interval) == 0) {
            data->has_swap_interval = 1;
            data->swap_interval = interval;
        }

        /* We need to do this so the EGLSurface can be freed */
        SDL_GL_MakeCurrent(window, NULL);
        data->backup_done = 1;
    }
}
#endif

/*
 * Android_ResumeSem and Android_PauseSem are signaled from Java_org_libsdl_app_SDLActivity_nativePause and Java_org_libsdl_app_SDLActivity_nativeResume
 * When the pause semaphore is signaled, if Android_PumpEvents_Blocking is used, the event loop will block until the resume signal is emitted.
 *
 * No polling necessary
 */

void Android_PumpEvents_Blocking(SDL_VideoDevice *_this)
{
    SDL_VideoData *videodata = _this->driverdata;

    if (videodata->isPaused) {
        SDL_bool isContextExternal = SDL_IsVideoContextExternal();

#ifdef SDL_VIDEO_OPENGL_EGL
        /* Make sure this is the last thing we do before pausing */
        if (!isContextExternal) {
            SDL_LockMutex(Android_ActivityMutex);
            android_egl_context_backup(Android_Window);
            SDL_UnlockMutex(Android_ActivityMutex);
        }
#endif

        ANDROIDAUDIO_PauseDevices();
        OPENSLES_PauseDevices();
        AAUDIO_PauseDevices();

        if (SDL_WaitSemaphore(Android_ResumeSem) == 0) {

            videodata->isPaused = 0;

            /* Android_ResumeSem was signaled */
            SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_FOREGROUND);

            ANDROIDAUDIO_ResumeDevices();
            OPENSLES_ResumeDevices();
            AAUDIO_ResumeDevices();

            /* Restore the GL Context from here, as this operation is thread dependent */
#ifdef SDL_VIDEO_OPENGL_EGL
            if (!isContextExternal && !SDL_HasEvent(SDL_EVENT_QUIT)) {
                SDL_LockMutex(Android_ActivityMutex);
                android_egl_context_restore(Android_Window);
                SDL_UnlockMutex(Android_ActivityMutex);
            }
#endif

            /* Make sure SW Keyboard is restored when an app becomes foreground */
            if (SDL_TextInputActive() &&
                SDL_GetHintBoolean(SDL_HINT_ENABLE_SCREEN_KEYBOARD, SDL_TRUE)) {
                Android_ShowScreenKeyboard(_this, Android_Window); /* Only showTextInput */
            }

            SDL_SendAppEvent(SDL_EVENT_DID_ENTER_FOREGROUND);
            SDL_SendWindowEvent(Android_Window, SDL_EVENT_WINDOW_RESTORED, 0, 0);
        }
    } else {
        if (videodata->isPausing || SDL_TryWaitSemaphore(Android_PauseSem) == 0) {

            /* Android_PauseSem was signaled */
            if (videodata->isPausing == 0) {
                SDL_SendWindowEvent(Android_Window, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
                SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_BACKGROUND);
                SDL_SendAppEvent(SDL_EVENT_DID_ENTER_BACKGROUND);
            }

            /* We've been signaled to pause (potentially several times), but before we block ourselves,
             * we need to make sure that the very last event (of the first pause sequence, if several)
             * has reached the app */
            if (SDL_NumberOfEvents(SDL_EVENT_DID_ENTER_BACKGROUND) > SDL_GetSemaphoreValue(Android_PauseSem)) {
                videodata->isPausing = 1;
            } else {
                videodata->isPausing = 0;
                videodata->isPaused = 1;
            }
        }
    }
}

void Android_PumpEvents_NonBlocking(SDL_VideoDevice *_this)
{
    SDL_VideoData *videodata = _this->driverdata;
    static int backup_context = 0;

    if (videodata->isPaused) {

        SDL_bool isContextExternal = SDL_IsVideoContextExternal();
        if (backup_context) {

#ifdef SDL_VIDEO_OPENGL_EGL
            if (!isContextExternal) {
                SDL_LockMutex(Android_ActivityMutex);
                android_egl_context_backup(Android_Window);
                SDL_UnlockMutex(Android_ActivityMutex);
            }
#endif

            if (videodata->pauseAudio) {
                ANDROIDAUDIO_PauseDevices();
                OPENSLES_PauseDevices();
                AAUDIO_PauseDevices();
            }

            backup_context = 0;
        }

        if (SDL_TryWaitSemaphore(Android_ResumeSem) == 0) {

            videodata->isPaused = 0;

            /* Android_ResumeSem was signaled */
            SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_FOREGROUND);

            if (videodata->pauseAudio) {
                ANDROIDAUDIO_ResumeDevices();
                OPENSLES_ResumeDevices();
                AAUDIO_ResumeDevices();
            }

#ifdef SDL_VIDEO_OPENGL_EGL
            /* Restore the GL Context from here, as this operation is thread dependent */
            if (!isContextExternal && !SDL_HasEvent(SDL_EVENT_QUIT)) {
                SDL_LockMutex(Android_ActivityMutex);
                android_egl_context_restore(Android_Window);
                SDL_UnlockMutex(Android_ActivityMutex);
            }
#endif

            /* Make sure SW Keyboard is restored when an app becomes foreground */
            if (SDL_TextInputActive() &&
                SDL_GetHintBoolean(SDL_HINT_ENABLE_SCREEN_KEYBOARD, SDL_TRUE)) {
                Android_ShowScreenKeyboard(_this, Android_Window); /* Only showTextInput */
            }

            SDL_SendAppEvent(SDL_EVENT_DID_ENTER_FOREGROUND);
            SDL_SendWindowEvent(Android_Window, SDL_EVENT_WINDOW_RESTORED, 0, 0);
        }
    } else {
        if (videodata->isPausing || SDL_TryWaitSemaphore(Android_PauseSem) == 0) {

            /* Android_PauseSem was signaled */
            if (videodata->isPausing == 0) {
                SDL_SendWindowEvent(Android_Window, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
                SDL_SendAppEvent(SDL_EVENT_WILL_ENTER_BACKGROUND);
                SDL_SendAppEvent(SDL_EVENT_DID_ENTER_BACKGROUND);
            }

            /* We've been signaled to pause (potentially several times), but before we block ourselves,
             * we need to make sure that the very last event (of the first pause sequence, if several)
             * has reached the app */
            if (SDL_NumberOfEvents(SDL_EVENT_DID_ENTER_BACKGROUND) > SDL_GetSemaphoreValue(Android_PauseSem)) {
                videodata->isPausing = 1;
            } else {
                videodata->isPausing = 0;
                videodata->isPaused = 1;
                backup_context = 1;
            }
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */
