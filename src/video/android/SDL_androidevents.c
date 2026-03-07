/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#include "../../audio/aaudio/SDL_aaudio.h"
#include "../../audio/openslES/SDL_openslES.h"


#ifdef SDL_VIDEO_OPENGL_EGL
// Restore EGL context when we have both:
// nativeSurfaceCreated()
// and SDLActivity hasFocus();
void Android_egl_context_restore(SDL_Window *window)
{
    SDL_WindowData *data = window->internal;

    if (data->backup_done && data->hasFocus && data->egl_surface != EGL_NO_SURFACE) {
        SDL_GL_MakeCurrent(window, NULL);
        if (!SDL_GL_MakeCurrent(window, (SDL_GLContext)data->egl_context)) {
            // The context is no longer valid, create a new one
            data->egl_context = (EGLContext)SDL_GL_CreateContext(window);
            SDL_GL_MakeCurrent(window, (SDL_GLContext)data->egl_context);
            SDL_Event event;
            SDL_zero(event);
            event.type = SDL_EVENT_RENDER_DEVICE_RESET;
            event.render.windowID = SDL_GetWindowID(window);
            SDL_PushEvent(&event);
        }
        data->backup_done = false;

        SDL_GL_SetSwapInterval(data->swap_interval);
    }
}

static void Android_egl_context_backup(SDL_Window *window)
{
    int interval = 0;
    // Keep a copy of the EGL Context so we can try to restore it when we resume
    SDL_WindowData *data = window->internal;
    data->egl_context = SDL_GL_GetCurrentContext();

    // Save/Restore the swap interval / vsync
    if (SDL_GL_GetSwapInterval(&interval)) {
        data->has_swap_interval = 1;
        data->swap_interval = interval;
    }

    // We need to do this so the EGLSurface can be freed
    SDL_GL_MakeCurrent(window, NULL);
    data->backup_done = true;
}
#endif

/*
 * Android_ResumeSem and Android_PauseSem are signaled from Java_org_libsdl_app_SDLActivity_nativePause and Java_org_libsdl_app_SDLActivity_nativeResume
 */
static bool Android_EventsInitialized;
static bool Android_BlockOnPause = true;
static bool Android_Paused;
static bool Android_PausedAudio;
static bool Android_Destroyed;

void Android_InitEvents(void)
{
    if (!Android_EventsInitialized) {
        Android_BlockOnPause = SDL_GetHintBoolean(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, true);
        Android_Paused = false;
        Android_Destroyed = false;
        Android_EventsInitialized = true;
    }
}

static void Android_PauseAudio(void)
{
    OPENSLES_PauseDevices();
    AAUDIO_PauseDevices();
    Android_PausedAudio = true;
}

static void Android_ResumeAudio(void)
{

    if (Android_PausedAudio) {
        OPENSLES_ResumeDevices();
        AAUDIO_ResumeDevices();
        Android_PausedAudio = false;
    }
}

void Android_OnPause(SDL_Window *window)
{
    SDL_OnApplicationWillEnterBackground();
    SDL_OnApplicationDidEnterBackground();

    /* The semantics are that as soon as the enter background event
     * has been queued, the app will block. The application should
     * do any life cycle handling in an event filter while the event
     * was being queued.
     */
#ifdef SDL_VIDEO_OPENGL_EGL
    if (window && !window->external_graphics_context) {
        Android_egl_context_backup(window);
    }
#endif

    if (Android_BlockOnPause) {
        // We're blocking, also pause audio
        Android_PauseAudio();
    }

    Android_Paused = true;
}

void Android_OnResume(SDL_Window *window)
{
    Android_Paused = false;

    SDL_OnApplicationWillEnterForeground();

    Android_ResumeAudio();

#ifdef SDL_VIDEO_OPENGL_EGL
    // Restore the GL Context from here, as this operation is thread dependent
    if (window && !window->external_graphics_context && !SDL_HasEvent(SDL_EVENT_QUIT)) {
        Android_egl_context_restore(window);
    }
#endif

    SDL_OnApplicationDidEnterForeground();
}

void Android_OnDestroy(void)
{
    // Make sure we unblock any audio processing before we quit
    Android_ResumeAudio();

    /* Discard previous events. The user should have handled state storage
     * in SDL_EVENT_WILL_ENTER_BACKGROUND. After nativeSendQuit() is called, no
     * events other than SDL_EVENT_QUIT and SDL_EVENT_TERMINATING should fire */
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    SDL_SendQuit();
    SDL_SendAppEvent(SDL_EVENT_TERMINATING);

    Android_Destroyed = true;
}

void Android_PumpEvents()
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_Window *window = _this ? _this->windows : NULL;

    if (Android_Destroyed) {
        return;
    }

    Android_PumpRPC(window);
}


// return 'true' if app has been blocked
bool Android_BlockEventLoop()
{
    if (Android_Destroyed) {
        return false;
    }

    if (Android_Paused) {
        if (Android_BlockOnPause) {
            SDL_Log("BlockOnPause");
            Android_WaitForResume();
            SDL_Log("BlockOnPause...resume");

            // app has been blocked. now we're unblocked.
            // pump to get 'resume' command (eg RPC_cmd_nativeResume).

            SDL_VideoDevice *_this = SDL_GetVideoDevice();
            SDL_Window *window = _this ? _this->windows : NULL;
            Android_PumpRPC(window);

            return true;
        }
    }

    return false;
}


bool Android_WaitActiveAndLockActivity(SDL_Window *window)
{
retry:

    // Lock first.
    // So no new lifecycle event comes in the meantimes from the SDLActivity,
    // Hence SDLActivity won't change state.
    Android_LockActivityState();

    // Pump all events so that Android_Paused state is correct
    Android_PumpRPC(window);

    if (Android_Destroyed) {
        SDL_SetError("Android activity has been destroyed");
        Android_UnlockActivityState();
        return false;
    }

    if (!Android_Paused) {
        // SDLActivity is Active. return lock'ed
        return true;
    } else {
        // Still Paused
        // Unlock and wait for the SDLActivity to send new cycle events
        // or for the user to move the app to foreground.
        Android_UnlockActivityState();
        SDL_Delay(10);
        goto retry;
    }

    return true;
}

void Android_QuitEvents(void)
{
    Android_EventsInitialized = false;
}

#endif // SDL_VIDEO_DRIVER_ANDROID
