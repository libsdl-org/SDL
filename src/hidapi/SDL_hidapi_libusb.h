/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

/* Define standard library functions in terms of SDL */
#define HIDAPI_USING_SDL_RUNTIME
#define free    SDL_free
#define iconv_t         SDL_iconv_t
#define ICONV_CONST
#define iconv(a,b,c,d,e) SDL_iconv(a, (const char **)b, c, d, e)
#define iconv_open      SDL_iconv_open
#define iconv_close     SDL_iconv_close
#define malloc          SDL_malloc
#define realloc         SDL_realloc
#define setlocale(X, Y) NULL
#define snprintf        SDL_snprintf
#define strcmp          SDL_strcmp
#define strdup          SDL_strdup
#define strncpy         SDL_strlcpy
#ifdef tolower
#undef tolower
#endif
#define tolower         SDL_tolower
#define wcsdup          SDL_wcsdup


#ifndef __FreeBSD__
/* this is awkwardly inlined, so we need to re-implement it here
 * so we can override the libusb_control_transfer call */
static int SDL_libusb_get_string_descriptor(libusb_device_handle *dev,
                                 uint8_t descriptor_index, uint16_t lang_id,
                                 unsigned char *data, int length)
{
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN | 0x0, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | descriptor_index, lang_id,
                                   data, (uint16_t)length, 1000); /* Endpoint 0 IN */
}
#define libusb_get_string_descriptor SDL_libusb_get_string_descriptor
#endif /* __FreeBSD__ */

#define HIDAPI_THREAD_STATE_DEFINED

/* Barrier implementation because Android/Bionic don't have pthread_barrier.
   This implementation came from Brent Priddy and was posted on
   StackOverflow. It is used with his permission. */

typedef struct _SDL_ThreadBarrier
{
    SDL_Mutex *mutex;
    SDL_Condition *cond;
    Uint32 count;
    Uint32 trip_count;
} SDL_ThreadBarrier;

static int SDL_CreateThreadBarrier(SDL_ThreadBarrier *barrier, Uint32 count)
{
    SDL_assert(barrier != NULL);
    SDL_assert(count != 0);

    barrier->mutex = SDL_CreateMutex();
    if (barrier->mutex == NULL) {
        return -1; /* Error set by CreateMutex */
    }
    barrier->cond = SDL_CreateCondition();
    if (barrier->cond == NULL) {
        return -1; /* Error set by CreateCond */
    }

    barrier->trip_count = count;
    barrier->count = 0;

    return 0;
}

static void SDL_DestroyThreadBarrier(SDL_ThreadBarrier *barrier)
{
    SDL_DestroyCondition(barrier->cond);
    SDL_DestroyMutex(barrier->mutex);
}

static int SDL_WaitThreadBarrier(SDL_ThreadBarrier *barrier)
{
    SDL_LockMutex(barrier->mutex);
    barrier->count += 1;
    if (barrier->count >= barrier->trip_count) {
        barrier->count = 0;
        SDL_BroadcastCondition(barrier->cond);
        SDL_UnlockMutex(barrier->mutex);
        return 1;
    }
    SDL_WaitCondition(barrier->cond, barrier->mutex);
    SDL_UnlockMutex(barrier->mutex);
    return 0;
}

#include "../thread/SDL_systhread.h"

#define THREAD_STATE_WAIT_TIMED_OUT SDL_MUTEX_TIMEDOUT

typedef struct
{
    SDL_Thread *thread;
    SDL_Mutex *mutex; /* Protects input_reports */
    SDL_Condition *condition;
    SDL_ThreadBarrier barrier; /* Ensures correct startup sequence */

} hid_device_thread_state;

static void thread_state_init(hid_device_thread_state *state)
{
    state->mutex = SDL_CreateMutex();
    state->condition = SDL_CreateCondition();
    SDL_CreateThreadBarrier(&state->barrier, 2);
}

static void thread_state_free(hid_device_thread_state *state)
{
    SDL_DestroyThreadBarrier(&state->barrier);
    SDL_DestroyCondition(state->condition);
    SDL_DestroyMutex(state->mutex);
}

static void thread_state_push_cleanup(void (*routine)(void *), void *arg)
{
    /* There isn't an equivalent in SDL, and it's only useful for threads calling hid_read_timeout() */
}

static void thread_state_pop_cleanup(int execute)
{
}

static void thread_state_lock(hid_device_thread_state *state)
{
    SDL_LockMutex(state->mutex);
}

static void thread_state_unlock(hid_device_thread_state *state)
{
    SDL_UnlockMutex(state->mutex);
}

static void thread_state_wait_condition(hid_device_thread_state *state)
{
    SDL_WaitCondition(state->condition, state->mutex);
}

static int thread_state_wait_condition_timeout(hid_device_thread_state *state, struct timespec *ts)
{
    Uint64 end_time;
    Sint64 timeout_ns;
    Sint32 timeout_ms;

    end_time = ts->tv_sec;
    end_time *= 1000000000L;
    end_time += ts->tv_nsec;
    timeout_ns = (Sint64)(end_time - SDL_GetTicksNS());
    if (timeout_ns <= 0) {
        timeout_ms = 0;
    } else {
        timeout_ms = (Sint32)SDL_NS_TO_MS(timeout_ns);
    }
    return SDL_WaitConditionTimeout(state->condition, state->mutex, timeout_ms);
}

static void thread_state_signal_condition(hid_device_thread_state *state)
{
    SDL_SignalCondition(state->condition);
}

static void thread_state_broadcast_condition(hid_device_thread_state *state)
{
    SDL_BroadcastCondition(state->condition);
}

static void thread_state_wait_barrier(hid_device_thread_state *state)
{
    SDL_WaitThreadBarrier(&state->barrier);
}

typedef struct
{
    void *(*func)(void*);
    void *func_arg;

} RunInputThreadParam;

static int RunInputThread(void *param)
{
    RunInputThreadParam *data = (RunInputThreadParam *)param;
    void *(*func)(void*) = data->func;
    void *func_arg = data->func_arg;
    SDL_free(data);
    func(func_arg);
    return 0;
}

static void thread_state_create_thread(hid_device_thread_state *state, void *(*func)(void*), void *func_arg)
{
    RunInputThreadParam *param = (RunInputThreadParam *)malloc(sizeof(*param));
    /* Note that the hidapi code didn't check for thread creation failure.
     * We'll crash if malloc() fails
     */
    param->func = func;
    param->func_arg = func_arg;
    state->thread = SDL_CreateThreadInternal(RunInputThread, "libusb", 0, param);
}

static void thread_state_join_thread(hid_device_thread_state *state)
{
    SDL_WaitThread(state->thread, NULL);
}

static void thread_state_get_current_time(struct timespec *ts)
{
    Uint64 ns = SDL_GetTicksNS();

    ts->tv_sec = ns / 1000000000L;
    ts->tv_nsec = ns % 1000000000L;
}

#undef HIDAPI_H__
#include "libusb/hid.c"
