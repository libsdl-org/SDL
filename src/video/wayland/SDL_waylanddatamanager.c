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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>

#include "../../core/unix/SDL_poll.h"
#include "../../events/SDL_events_c.h"
#include "../SDL_clipboard_c.h"

#include "SDL_waylandclipboard.h"
#include "SDL_waylanddatamanager.h"
#include "primary-selection-unstable-v1-client-protocol.h"

/* This value is arbitrary, but needs to be long enough to allow the remote end to
 * wake up and do any necessary processing of the data before sending it. In some
 * cases, this can be multiple seconds.
 */
#define DEFAULT_PIPE_TIMEOUT_NS SDL_MS_TO_NS(5000)

typedef struct SDL_WaylandUserdata
{
    void *data;
    Uint32 sequence;
} Wayland_Userdata;

typedef struct Wayland_DataSource
{
    struct wl_data_source *source;
    Wayland_DataDevice *data_device;
    SDL_ClipboardDataCallback data_callback;
    SDL_ClipboardCleanupCallback cleanup_callback;
    Wayland_Userdata userdata;
} Wayland_DataSource;

typedef struct Wayland_PrimarySelectionSource
{
    struct zwp_primary_selection_source_v1 *source;
    Wayland_PrimarySelectionDevice *primary_selection_device;
    SDL_ClipboardDataCallback data_callback;
    SDL_ClipboardCleanupCallback cleanup_callback;
    Wayland_Userdata userdata;
} Wayland_PrimarySelectionSource;

typedef struct Wayland_DataOffer
{
    struct wl_data_offer *offer;
    struct wl_array mimes;
    Wayland_DataDevice *data_device;

    // Callback data for queued receive.
    struct wl_callback *callback;
    int read_fd;
} Wayland_DataOffer;

typedef struct Wayland_DropData
{
    Wayland_DataOffer *offer;
    SDL_Window *dnd_window;
    const char *text_mime_type;
    struct wl_callback *cleanup_callback;
    Wayland_JobID id;

    struct wl_list link;
} Wayland_DropData;

typedef struct Wayland_PrimarySelectionOffer
{
    struct zwp_primary_selection_offer_v1 *offer;
    struct wl_array mimes;
    Wayland_PrimarySelectionDevice *primary_selection_device;
} Wayland_PrimarySelectionOffer;

struct Wayland_DataDevice
{
    struct wl_data_device *data_device;
    SDL_WaylandSeat *seat;
    char *id_str;

    // Drag and Drop
    uint32_t drag_serial;
    Wayland_DataOffer *drag_offer;
    const char *text_mime_type;
    bool has_mime_file;
    SDL_Window *dnd_window;
    struct wl_surface *dnd_surface;
    struct wl_list pending_drops;

    // Clipboard
    Wayland_DataOffer *selection_offer;
    Wayland_DataSource *selection_source;
};

struct Wayland_PrimarySelectionDevice
{
    struct zwp_primary_selection_device_v1 *primary_selection_device;
    SDL_WaylandSeat *seat;

    Wayland_PrimarySelectionSource *source;
    Wayland_PrimarySelectionOffer *offer;
};

/* sigtimedwait() is an optional part of POSIX.1-2001, and OpenBSD doesn't implement it.
 * Based on https://comp.unix.programmer.narkive.com/rEDH0sPT/sigtimedwait-implementation
 */
#ifndef HAVE_SIGTIMEDWAIT
#include <errno.h>
#include <time.h>
static int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout)
{
    struct timespec elapsed = { 0 }, rem = { 0 };
    sigset_t pending;

    do {
        // Check the pending signals, and call sigwait if there is at least one of interest in the set.
        sigpending(&pending);
        for (int signo = 1; signo < NSIG; ++signo) {
            if (sigismember(set, signo) && sigismember(&pending, signo)) {
                if (!sigwait(set, &signo)) {
                    if (info) {
                        SDL_zerop(info);
                        info->si_signo = signo;
                    }
                    return signo;
                } else {
                    return -1;
                }
            }
        }

        if (timeout->tv_sec || timeout->tv_nsec) {
            long ns = 20000000L; // 2/100ths of a second
            nanosleep(&(struct timespec){ 0, ns }, &rem);
            ns -= rem.tv_nsec;
            elapsed.tv_sec += (elapsed.tv_nsec + ns) / 1000000000L;
            elapsed.tv_nsec = (elapsed.tv_nsec + ns) % 1000000000L;
        }
    } while (elapsed.tv_sec < timeout->tv_sec || (elapsed.tv_sec == timeout->tv_sec && elapsed.tv_nsec < timeout->tv_nsec));

    errno = EAGAIN;
    return -1;
}
#endif

static bool MIMEDataListHasType(struct wl_array *list, const char *mime_type)
{
    const char **item;
    wl_array_for_each (item, list) {
        if (SDL_strcmp(*item, mime_type) == 0) {
            return true;
        }
    }

    return false;
}

static bool MIMEDataListAdd(struct wl_array *list, const char *mime_type)
{
    if (!MIMEDataListHasType(list, mime_type)) {
        char **item = WAYLAND_wl_array_add(list, sizeof(char *));
        *item = SDL_strdup(mime_type);
    }

    return true;
}

static void MIMEDataListFree(struct wl_array *list)
{
    char **item;
    wl_array_for_each (item, list) {
        SDL_free(*item);
    }

    WAYLAND_wl_array_release(list);
}

static ssize_t WritePipe(int fd, const void *buffer, size_t total_length, size_t *pos)
{
    ssize_t bytes_written = 0;
    const ssize_t length = total_length - *pos;

    sigset_t sig_set;
    sigset_t old_sig_set;
    struct timespec zerotime = { 0 };

    const int ready = SDL_IOReady(fd, SDL_IOR_WRITE, DEFAULT_PIPE_TIMEOUT_NS);

    sigemptyset(&sig_set);
    sigaddset(&sig_set, SIGPIPE);

#ifdef SDL_THREADS_DISABLED
    sigprocmask(SIG_BLOCK, &sig_set, &old_sig_set);
#else
    pthread_sigmask(SIG_BLOCK, &sig_set, &old_sig_set);
#endif

    if (ready > 0) {
        if (length > 0) {
            bytes_written = write(fd, (Uint8 *)buffer + *pos, SDL_min(length, PIPE_BUF));
        }

        if (bytes_written > 0) {
            *pos += bytes_written;
        }
    } else if (ready == 0) {
        SDL_SetError("Pipe timeout");
    } else {
        SDL_SetError("Pipe poll error");
    }

    sigtimedwait(&sig_set, NULL, &zerotime);

#ifdef SDL_THREADS_DISABLED
    sigprocmask(SIG_SETMASK, &old_sig_set, NULL);
#else
    pthread_sigmask(SIG_SETMASK, &old_sig_set, NULL);
#endif

    return bytes_written;
}

static ssize_t ReadPipe(int fd, void **buffer, size_t *total_length, Sint64 timeout_ns)
{
    void *output_buffer = NULL;
    char temp[PIPE_BUF];
    ssize_t bytes_read = 0;

    const int ready = SDL_IOReady(fd, SDL_IOR_READ, timeout_ns);

    if (ready > 0) {
        bytes_read = read(fd, temp, sizeof(temp));
    } else if (ready == 0) {
        SDL_SetError("Pipe timeout");
    } else {
        SDL_SetError("Pipe poll error");
    }

    if (bytes_read > 0) {
        const size_t pos = *total_length;
        *total_length += bytes_read;

        const size_t new_buffer_length = *total_length + sizeof(Uint32);

        if (!*buffer) {
            output_buffer = SDL_malloc(new_buffer_length);
        } else {
            output_buffer = SDL_realloc(*buffer, new_buffer_length);
        }

        if (!output_buffer) {
            bytes_read = -1;
        } else {
            SDL_memcpy((Uint8 *)output_buffer + pos, temp, bytes_read);
            SDL_memset((Uint8 *)output_buffer + (new_buffer_length - sizeof(Uint32)), 0, sizeof(Uint32));

            *buffer = output_buffer;
        }
    }

    return bytes_read;
}

static size_t SendData(const void *data, size_t length, int fd)
{
    size_t result = 0;

    if (length > 0 && data) {
        while (WritePipe(fd, data, length, &result) > 0) {
            // Just keep spinning
        }
    }
    close(fd);

    return result;
}

static ssize_t DataSourceSend(Wayland_DataSource *source, const char *mime_type, int fd)
{
    const void *data = NULL;
    size_t length = 0;

    if (SDL_strcmp(mime_type, SDL_DATA_ORIGIN_MIME) == 0) {
        data = source->data_device->id_str;
        length = SDL_strlen(source->data_device->id_str);
    } else if (source->data_callback) {
        data = source->data_callback(source->userdata.data, mime_type, &length);
    }

    return SendData(data, length, fd);
}

static ssize_t PrimarySelectionSourceSend(Wayland_PrimarySelectionSource *source, const char *mime_type, int fd)
{
    const void *data = NULL;
    size_t length = 0;

    if (source->data_callback) {
        data = source->data_callback(source->userdata.data, mime_type, &length);
    }

    return SendData(data, length, fd);
}

static void DestroyDataSource(Wayland_DataSource *source)
{
    if (source) {
        Wayland_DataDevice *data_device = source->data_device;
        if (data_device && (data_device->selection_source == source)) {
            data_device->selection_source = NULL;
        }
        wl_data_source_destroy(source->source);
        if (source->userdata.sequence) {
            SDL_CancelClipboardData(source->userdata.sequence);
        } else if (source->cleanup_callback) {
            source->cleanup_callback(source->userdata.data);
        }
        SDL_free(source);
    }
}

static void DestroyPrimarySelectionSource(Wayland_PrimarySelectionSource *source)
{
    if (source) {
        Wayland_PrimarySelectionDevice *primary_selection_device = source->primary_selection_device;
        if (primary_selection_device && (primary_selection_device->source == source)) {
            primary_selection_device->source = NULL;
        }
        zwp_primary_selection_source_v1_destroy(source->source);
        if (source->cleanup_callback) {
            source->cleanup_callback(source->userdata.data);
        }
        SDL_free(source);
    }
}

static void DestroyDataOffer(Wayland_DataOffer *offer)
{
    if (offer) {
        if (offer->callback) {
            wl_callback_destroy(offer->callback);
        }
        if (offer->read_fd >= 0) {
            close(offer->read_fd);
        }
        wl_data_offer_destroy(offer->offer);
        MIMEDataListFree(&offer->mimes);
        SDL_free(offer);
    }
}

static void DestroyPrimarySelectionOffer(Wayland_PrimarySelectionOffer *offer)
{
    if (offer) {
        zwp_primary_selection_offer_v1_destroy(offer->offer);
        MIMEDataListFree(&offer->mimes);
        SDL_free(offer);
    }
}

static void DestroyDropData(Wayland_DropData *drop_data)
{
    if (drop_data->cleanup_callback) {
        wl_callback_destroy(drop_data->cleanup_callback);
    }
    DestroyDataOffer(drop_data->offer);
    WAYLAND_wl_list_remove(&drop_data->link);
    SDL_free(drop_data);
}

static void data_source_handle_target(void *data, struct wl_data_source *wl_data_source, const char *mime_type)
{
}

static void data_source_handle_send(void *data, struct wl_data_source *wl_data_source, const char *mime_type, int32_t fd)
{
    DataSourceSend((Wayland_DataSource *)data, mime_type, fd);
}

static void data_source_handle_cancelled(void *data, struct wl_data_source *wl_data_source)
{
    Wayland_DataSource *source = data;
    if (source) {
        DestroyDataSource(source);
    }
}

static void data_source_handle_dnd_drop_performed(void *data, struct wl_data_source *wl_data_source)
{
}

static void data_source_handle_dnd_finished(void *data, struct wl_data_source *wl_data_source)
{
    Wayland_DataSource *source = data;
    if (source) {
        DestroyDataSource(source);
    }
}

static void data_source_handle_action(void *data, struct wl_data_source *wl_data_source, uint32_t dnd_action)
{
}

static const struct wl_data_source_listener data_source_listener = {
    data_source_handle_target,
    data_source_handle_send,
    data_source_handle_cancelled,
    data_source_handle_dnd_drop_performed, // Version 3
    data_source_handle_dnd_finished,       // Version 3
    data_source_handle_action,             // Version 3
};

static Wayland_DataSource *CreateDataSource(Wayland_DataDevice *data_device)
{
    SDL_VideoData *videodata = data_device->seat->display;

    Wayland_DataSource *data_source = SDL_calloc(1, sizeof(*data_source));
    if (!data_source) {
        return NULL;
    }

    struct wl_data_source *id = wl_data_device_manager_create_data_source(videodata->data_device_manager);
    data_source->source = id;
    data_source->data_device = data_device;
    wl_data_source_set_user_data(id, data_source);
    wl_data_source_add_listener(id, &data_source_listener, data_source);

    return data_source;
}

static void primary_selection_source_send(void *data, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1, const char *mime_type, int32_t fd)
{
    PrimarySelectionSourceSend((Wayland_PrimarySelectionSource *)data, mime_type, fd);
}

static void primary_selection_source_cancelled(void *data, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1)
{
    DestroyPrimarySelectionSource(data);
}

static const struct zwp_primary_selection_source_v1_listener primary_selection_source_listener = {
    primary_selection_source_send,
    primary_selection_source_cancelled,
};

static Wayland_PrimarySelectionSource *CreatePrimarySelectionSource(Wayland_PrimarySelectionDevice *primary_selection_device)
{
    SDL_VideoData *videodata = primary_selection_device->seat->display;

    Wayland_PrimarySelectionSource *primary_selection_source = SDL_calloc(1, sizeof(*primary_selection_source));
    if (!primary_selection_source) {
        return NULL;
    }

    struct zwp_primary_selection_source_v1 *id = zwp_primary_selection_device_manager_v1_create_source(videodata->primary_selection_device_manager);
    primary_selection_source->source = id;
    primary_selection_source->primary_selection_device = primary_selection_device;
    zwp_primary_selection_source_v1_add_listener(id, &primary_selection_source_listener, primary_selection_source);

    return primary_selection_source;
}

static void *GetOfferDataFromSelf(Wayland_DataOffer *offer, size_t *length)
{
    if (!offer) {
        SDL_SetError("Invalid data offer");
        return NULL;
    }

    if (offer->read_fd < 0) {
        SDL_SetError("Pipe closed");
        return NULL;
    }

    // The timeout is 0, as the data will already be in the pipe if it is from this process.
    void *buffer = NULL;
    while (ReadPipe(offer->read_fd, &buffer, length, 0) > 0) {
    }

    return buffer;
}

static bool RequestOfferDataFromSelf(Wayland_DataOffer *offer, const char *mime_type, const struct wl_callback_listener *completion_listener, void *userdata)
{
    int pipefd[2];

    if (!offer) {
        SDL_SetError("Invalid data offer");
        return false;
    }

    Wayland_DataDevice *data_device = offer->data_device;
    if (offer->read_fd >= 0) {
        close(offer->read_fd);
        offer->read_fd = -1;
    }
    if (offer->callback) {
        wl_callback_destroy(offer->callback);
        offer->callback = NULL;
    }

    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
        offer->read_fd = pipefd[0];
        wl_data_offer_receive(offer->offer, mime_type, pipefd[1]);
        close(pipefd[1]);

        offer->callback = wl_display_sync(data_device->seat->display->display);
        wl_callback_add_listener(offer->callback, completion_listener, userdata);

        WAYLAND_wl_display_flush(data_device->seat->display->display);
    } else {
        return SDL_SetError("Could not create pipe");
    }

    return true;
}

static void SelectionOfferNotifyFromMIMEs(Wayland_DataDevice *data_device, bool check_origin);

static void offer_source_done_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    if (!callback) {
        return;
    }

    Wayland_DataOffer *offer = (Wayland_DataOffer *)data;
    char *id = NULL;
    size_t length = 0;

    wl_callback_destroy(offer->callback);
    offer->callback = NULL;

    id = GetOfferDataFromSelf(offer, &length);
    if (id) {
        const bool source_is_external = SDL_strncmp(offer->data_device->id_str, id, length) != 0;
        SDL_free(id);
        if (source_is_external) {
            SelectionOfferNotifyFromMIMEs(offer->data_device, false);
        } else {
            // Recursive data offer; just destroy it.
            Wayland_DataDevice *data_device = offer->data_device;
            DestroyDataOffer(offer);
            data_device->selection_offer = NULL;
        }
    }
}

static const struct wl_callback_listener offer_source_listener = {
    offer_source_done_handler
};

static void UpdateSeatOffers(Wayland_DataDevice *data_device)
{
    SDL_WaylandSeat *offer_seat = data_device->seat;
    SDL_VideoData *video_data = offer_seat->display;

    // Clear any existing references to the existing clipboard data before replacing the current offer.
    SDL_WaylandSeat *s;
    wl_list_for_each (s, &video_data->seat_list, link) {
        if (s->data_device) {
            DestroyDataSource(s->data_device->selection_source);
            s->data_device->selection_source = NULL;

            // Don't clear the offer that is about to be set.
            if (s != offer_seat) {
                DestroyDataOffer(s->data_device->selection_offer);
                s->data_device->selection_offer = NULL;
            }
        }
    }

    video_data->current_data_offer_seat = offer_seat;
}

static void SelectionOfferNotifyFromMIMEs(Wayland_DataDevice *data_device, bool check_origin)
{
    Wayland_DataOffer *offer = data_device->selection_offer;
    char **new_mime_types = NULL;
    size_t num_formats = 0;

    if (offer) {
        size_t alloc_size = 0;

        // Do a first pass to compute allocation size.
        const char **item;
        wl_array_for_each (item, &offer->mimes) {
            // If origin metadata is found, queue a check and wait for confirmation that this offer isn't recursive.
            if (check_origin && SDL_strcmp(*item, SDL_DATA_ORIGIN_MIME) == 0) {
                RequestOfferDataFromSelf(offer, *item, &offer_source_listener, offer);
                return;
            }

            ++num_formats;
            alloc_size += SDL_strlen(*item) + 1;
        }

        alloc_size += (num_formats + 1) * sizeof(char *);

        new_mime_types = SDL_AllocateTemporaryMemory(alloc_size);
        if (!new_mime_types) {
            SDL_LogError(SDL_LOG_CATEGORY_INPUT, "unable to allocate new_mime_types");
            return;
        }

        // Second pass to fill.
        char *strPtr = (char *)(new_mime_types + num_formats + 1);
        alloc_size -= (uintptr_t)strPtr - (uintptr_t)new_mime_types;

        item = NULL;
        int i = 0;
        wl_array_for_each (item, &offer->mimes) {
            if (!item) {
                continue;
            }

            new_mime_types[i++] = strPtr;
            const size_t len = SDL_strlcpy(strPtr, *item, alloc_size) + 1;
            strPtr += len;
            alloc_size -= len;
        }
        new_mime_types[num_formats] = NULL;
    }

    UpdateSeatOffers(data_device);
    SDL_SendClipboardUpdate(false, new_mime_types, num_formats);
}

static void DataDeviceSetOffer(Wayland_DataDevice *data_device, Wayland_DataOffer *offer)
{
    // Don't notify when clearing the old selection offer if doing so will inadvertently clear the selection source.
    const bool notify = offer || (!offer && data_device->selection_offer && (!data_device->selection_offer->callback || !data_device->selection_source));
    DestroyDataOffer(data_device->selection_offer);
    data_device->selection_offer = offer;
    if (notify) {
        SelectionOfferNotifyFromMIMEs(data_device, true);
    }
}

static void *ReceiveDataOffer(Wayland_DataOffer *offer, const char *mime_type, size_t *length)
{
    int pipefd[2];
    void *buffer = NULL;
    *length = 0;

    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
        wl_data_offer_receive(offer->offer, mime_type, pipefd[1]);
        close(pipefd[1]);

        WAYLAND_wl_display_flush(offer->data_device->seat->display->display);

        while (ReadPipe(pipefd[0], &buffer, length, DEFAULT_PIPE_TIMEOUT_NS) > 0) {
        }
        close(pipefd[0]);
    } else {
        SDL_SetError("Could not create pipe");
    }

    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In Wayland_data_offer_receive for '%s', buffer (%zu) at %p",
                 mime_type, *length, buffer);

    return buffer;
}

void *Wayland_DataDeviceReceiveSelectionOffer(Wayland_DataDevice *data_device, const char *mime_type, size_t *length)
{
    if (!data_device) {
        SDL_SetError("Invalid data device");
        return NULL;
    }
    if (!data_device->selection_offer) {
        SDL_SetError("Invalid selection offer");
        return NULL;
    }

    return ReceiveDataOffer(data_device->selection_offer, mime_type, length);
}

static void *PrimarySelectionDeviceReceiveOffer(Wayland_PrimarySelectionDevice *primary_selection_device, const char *mime_type, size_t *length)
{
    int pipefd[2];
    void *buffer = NULL;
    *length = 0;

    if (!primary_selection_device) {
        SDL_SetError("Invalid primary selection device");
        return NULL;
    }
    if (!primary_selection_device->offer) {
        SDL_SetError("Invalid primary selection offer");
        return NULL;
    }

    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
        zwp_primary_selection_offer_v1_receive(primary_selection_device->offer->offer, mime_type, pipefd[1]);
        close(pipefd[1]);

        WAYLAND_wl_display_flush(primary_selection_device->seat->display->display);

        while (ReadPipe(pipefd[0], &buffer, length, DEFAULT_PIPE_TIMEOUT_NS) > 0) {
        }
        close(pipefd[0]);

    } else {
        SDL_SetError("Could not create pipe");
    }

    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In Wayland_primary_selection_offer_receive for '%s', buffer (%zu) at %p",
                 mime_type, *length, buffer);

    return buffer;
}

static void data_offer_handle_offer(void *data, struct wl_data_offer *wl_data_offer, const char *mime_type)
{
    Wayland_DataOffer *offer = data;
    MIMEDataListAdd(&offer->mimes, mime_type);
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In wl_data_offer_listener . data_offer_handle_offer on data_offer 0x%08x for MIME '%s'",
                 (wl_data_offer ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)wl_data_offer) : -1),
                 mime_type);
}

static void data_offer_handle_source_actions(void *data, struct wl_data_offer *wl_data_offer, uint32_t source_actions)
{
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In wl_data_offer_listener . data_offer_handle_source_actions on data_offer 0x%08x for Source Actions '%d'",
                 (wl_data_offer ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)wl_data_offer) : -1),
                 source_actions);
}

static void data_offer_handle_actions(void *data, struct wl_data_offer *wl_data_offer, uint32_t dnd_action)
{
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In wl_data_offer_listener . data_offer_handle_actions on data_offer 0x%08x for DND Actions '%d'",
                 (wl_data_offer ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)wl_data_offer) : -1),
                 dnd_action);
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_handle_offer,
    data_offer_handle_source_actions, // Version 3
    data_offer_handle_actions,        // Version 3
};

static void primary_selection_offer_handle_offer(void *data, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1, const char *mime_type)
{
    Wayland_PrimarySelectionOffer *offer = data;
    MIMEDataListAdd(&offer->mimes, mime_type);
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In zwp_primary_selection_offer_v1_listener . primary_selection_offer_handle_offer on primary_selection_offer 0x%08x for MIME '%s'",
                 (zwp_primary_selection_offer_v1 ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)zwp_primary_selection_offer_v1) : -1),
                 mime_type);
}

static const struct zwp_primary_selection_offer_v1_listener primary_selection_offer_listener = {
    primary_selection_offer_handle_offer,
};

static void data_device_handle_data_offer(void *data, struct wl_data_device *wl_data_device, struct wl_data_offer *id)
{
    Wayland_DataOffer *data_offer = SDL_calloc(1, sizeof(*data_offer));
    if (data_offer) {
        data_offer->offer = id;
        data_offer->data_device = (Wayland_DataDevice *)data;
        data_offer->read_fd = -1;
        WAYLAND_wl_array_init(&data_offer->mimes);
        wl_data_offer_set_user_data(id, data_offer);
        wl_data_offer_add_listener(id, &data_offer_listener, data_offer);
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_data_offer on data_offer 0x%08x",
                     (id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1));
    }
}

static void data_device_handle_enter(void *data, struct wl_data_device *wl_data_device,
                                     uint32_t serial, struct wl_surface *surface,
                                     wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id)
{
    Wayland_DataDevice *data_device = data;
    SDL_WindowData *window = surface ? Wayland_GetWindowDataForOwnedSurface(surface) : NULL;
    data_device->has_mime_file = false;
    data_device->text_mime_type = NULL;

    data_device->drag_serial = serial;

    // Save the drag offer so it can be freed later.
    if (id) {
        data_device->drag_offer = wl_data_offer_get_user_data(id);
    }

    if (data_device->drag_offer && window && window->accepts_drag_and_drop) {
#ifdef SDL_USE_LIBDBUS
        if (MIMEDataListHasType(&data_device->drag_offer->mimes, FILE_PORTAL_MIME)) {
            data_device->has_mime_file = true;
            wl_data_offer_accept(id, serial, FILE_PORTAL_MIME);
        }
#endif
        if (MIMEDataListHasType(&data_device->drag_offer->mimes, FILE_MIME)) {
            data_device->has_mime_file = true;
            wl_data_offer_accept(id, serial, FILE_MIME);
        }

        if (!data_device->has_mime_file) {
            size_t mime_count = 0;
            const char *const *text_mime_types = Wayland_GetTextMimeTypes(SDL_GetVideoDevice(), &mime_count);
            for (size_t i = 0; i < mime_count; ++i) {
                if (MIMEDataListHasType(&data_device->drag_offer->mimes, text_mime_types[i])) {
                    data_device->text_mime_type = text_mime_types[i];
                    wl_data_offer_accept(id, serial, text_mime_types[i]);
                    break;
                }
            }
        }

        if (data_device->has_mime_file || data_device->text_mime_type) {
            // SDL only supports "copy" style drag and drop
            if (wl_data_offer_get_version(data_device->drag_offer->offer) >= WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
                wl_data_offer_set_actions(data_device->drag_offer->offer, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
            }

            // Set the destination window and send the initial position.
            data_device->dnd_window = window->sdlwindow;
            data_device->dnd_surface = surface;
            double dx = wl_fixed_to_double(x);
            double dy = wl_fixed_to_double(y);

            // If over the mask, adjust the offset.
            if (surface == window->mask.surface) {
                dx += (double)window->mask.offset_x;
                dy += (double)window->mask.offset_y;
            }

            dx *= window->pointer_scale.x;
            dy *= window->pointer_scale.y;

            SDL_SendDropPosition(data_device->dnd_window, (float)dx, (float)dy);
            SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                         ". In wl_data_device_listener . data_device_handle_enter on data_offer 0x%08x at %d x %d into window %d for serial %d",
                         WAYLAND_wl_proxy_get_id((struct wl_proxy *)id),
                         wl_fixed_to_int(x), wl_fixed_to_int(y), SDL_GetWindowID(data_device->dnd_window), serial);
        } else {
            // Decline the offer.
            wl_data_offer_accept(id, serial, NULL);
            if (wl_data_offer_get_version(data_device->drag_offer->offer) >= WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
                wl_data_offer_set_actions(data_device->drag_offer->offer, WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE, WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE);
            }

            SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                         ". In wl_data_device_listener . data_device_handle_enter on data_offer 0x%08x at %d x %d for serial %d",
                         WAYLAND_wl_proxy_get_id((struct wl_proxy *)id), wl_fixed_to_int(x), wl_fixed_to_int(y), serial);
        }
    } else {
        data_device->dnd_window = NULL;
        data_device->dnd_surface = NULL;

        // Decline the offer.
        if (id) {
            wl_data_offer_accept(id, serial, NULL);
            if (wl_data_offer_get_version(data_device->drag_offer->offer) >= WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
                wl_data_offer_set_actions(data_device->drag_offer->offer, WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE, WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE);
            }
        }
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_enter on data_offer 0x%08x at %d x %d for serial %d",
                     id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1,
                     wl_fixed_to_int(x), wl_fixed_to_int(y), serial);
    }
}

static void data_device_handle_leave(void *data, struct wl_data_device *wl_data_device)
{
    Wayland_DataDevice *data_device = data;

    if (data_device->drag_offer) {
        if (data_device->dnd_window) {
            SDL_SendDropComplete(data_device->dnd_window);
            SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                         ". In wl_data_device_listener . data_device_handle_leave on data_offer 0x%08x from window %d for serial %d",
                         WAYLAND_wl_proxy_get_id((struct wl_proxy *)data_device->drag_offer->offer),
                         SDL_GetWindowID(data_device->dnd_window), data_device->drag_serial);
        } else {
            SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                         ". In wl_data_device_listener . data_device_handle_leave on data_offer 0x%08x for serial %d",
                         WAYLAND_wl_proxy_get_id((struct wl_proxy *)data_device->drag_offer->offer),
                         data_device->drag_serial);
        }
        DestroyDataOffer(data_device->drag_offer);
        data_device->drag_offer = NULL;
    } else {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_leave on data_offer 0x%08x for serial %d",
                     -1, -1);
    }
    data_device->has_mime_file = false;
    data_device->text_mime_type = NULL;
}

static void data_device_handle_motion(void *data, struct wl_data_device *wl_data_device, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    Wayland_DataDevice *data_device = data;

    if (data_device->drag_offer && data_device->dnd_window && (data_device->has_mime_file || data_device->text_mime_type)) {
        SDL_WindowData *window_data = data_device->dnd_window->internal;
        double dx = wl_fixed_to_double(x);
        double dy = wl_fixed_to_double(y);

        // If over the mask, adjust the offset.
        if (data_device->dnd_surface == window_data->mask.surface) {
            dx += (double)window_data->mask.offset_x;
            dy += (double)window_data->mask.offset_y;
        }

        dx *= window_data->pointer_scale.x;
        dy *= window_data->pointer_scale.y;

        /* XXX: Send the filename here if the event system ever starts passing it though.
         *      Any future implementation should cache the filenames, as otherwise this could
         *      hammer the DBus interface hundreds or even thousands of times per second.
         */
        SDL_SendDropPosition(data_device->dnd_window, (float)dx, (float)dy);
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_motion on data_offer 0x%08x at %d x %d in window %d serial %d",
                     WAYLAND_wl_proxy_get_id((struct wl_proxy *)data_device->drag_offer->offer),
                     wl_fixed_to_int(x), wl_fixed_to_int(y),
                     SDL_GetWindowID(data_device->dnd_window), data_device->drag_serial);
    } else {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_motion on data_offer 0x%08x at %d x %d serial %d",
                     -1, wl_fixed_to_int(x), wl_fixed_to_int(y), -1);
    }
}

static void drop_cleanup_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    DestroyDropData(data);
}

static const struct wl_callback_listener drop_cleanup_listener = {
    drop_cleanup_handler
};

static void drop_dispatch_handler(void *data)
{
    Wayland_DropData *drop = (Wayland_DropData *)data;
    Wayland_DataOffer *offer = drop->offer;
    Wayland_DataDevice *data_device = offer->data_device;

    if (drop->dnd_window) {
        size_t length;
        bool drop_handled = false;

#ifdef SDL_USE_LIBDBUS
        if (MIMEDataListHasType(&offer->mimes, FILE_PORTAL_MIME)) {
            void *buffer = ReceiveDataOffer(offer, FILE_PORTAL_MIME, &length);
            if (buffer) {
                SDL_DBusContext *dbus = SDL_DBus_GetContext();
                if (dbus) {
                    int path_count = 0;
                    char **paths = SDL_DBus_DocumentsPortalRetrieveFiles(buffer, &path_count);
                    // If dropped files contain a directory the list is empty
                    if (paths && path_count > 0) {
                        for (int i = 0; i < path_count; i++) {
                            SDL_SendDropFile(drop->dnd_window, NULL, paths[i]);
                        }
                        dbus->free_string_array(paths);
                        drop_handled = true;
                    }
                }
                SDL_free(buffer);
            }
        }
#endif
        /* If the XDG document portal fails, fall back to text/uri-list, if available.
         * When running inside of a flatpak sandbox, this will most likely be a list of
         * non-paths that are not visible to the application.
         */
        if (!drop_handled) {
            if (MIMEDataListHasType(&offer->mimes, FILE_MIME)) {
                void *buffer = ReceiveDataOffer(offer, FILE_MIME, &length);
                if (buffer) {
                    char *saveptr = NULL;
                    char *token = SDL_strtok_r((char *)buffer, "\r\n", &saveptr);
                    while (token) {
                        if (SDL_URIToLocal(token, token) >= 0) {
                            SDL_SendDropFile(drop->dnd_window, NULL, token);
                        }
                        token = SDL_strtok_r(NULL, "\r\n", &saveptr);
                    }
                    SDL_free(buffer);
                    drop_handled = true;
                }
            } else if (MIMEDataListHasType(&offer->mimes, drop->text_mime_type)) {
                void *buffer = ReceiveDataOffer(offer, drop->text_mime_type, &length);
                if (buffer) {
                    char *saveptr = NULL;
                    char *token = SDL_strtok_r((char *)buffer, "\r\n", &saveptr);
                    while (token) {
                        SDL_SendDropText(drop->dnd_window, token);
                        token = SDL_strtok_r(NULL, "\r\n", &saveptr);
                    }
                    SDL_free(buffer);
                    drop_handled = true;
                }
            }
        }

        if (drop_handled && wl_data_offer_get_version(offer->offer) >= WL_DATA_OFFER_FINISH_SINCE_VERSION) {
            wl_data_offer_finish(offer->offer);
        }

        SDL_SendDropComplete(drop->dnd_window);
    }

    DestroyDataOffer(drop->offer);
    drop->offer = NULL;

    // Queue a cleanup event to delete the drop object on the event queue thread.
    drop->cleanup_callback = wl_display_sync(data_device->seat->display->display);
    wl_callback_add_listener(drop->cleanup_callback, &drop_cleanup_listener, drop);
}

static void data_device_handle_drop(void *data, struct wl_data_device *wl_data_device)
{
    Wayland_DataDevice *data_device = data;

    if (data_device->drag_offer && data_device->dnd_window &&
        (data_device->has_mime_file || data_device->text_mime_type)) {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_drop on data_offer 0x%08x in window %d serial %d",
                     WAYLAND_wl_proxy_get_id((struct wl_proxy *)data_device->drag_offer->offer),
                     SDL_GetWindowID(data_device->dnd_window), data_device->drag_serial);

        Wayland_JobThreadContext *ctx = data_device->seat->display->job_thread_context;

        /* Dispatch the read on the event thread, as the remote end may be slow to respond,
         * and we don't want to block the main event loop for an excessive time.
         */
        Wayland_DropData *drop = SDL_calloc(1, sizeof(Wayland_DropData));
        if (drop) {
            drop->offer = data_device->drag_offer;
            drop->dnd_window = data_device->dnd_window;
            drop->text_mime_type = data_device->text_mime_type;

            WAYLAND_wl_list_insert(&data_device->pending_drops, &drop->link);
            Wayland_JobThreadAddJob(ctx, drop_dispatch_handler, drop, &drop->id);
        } else {
            DestroyDataOffer(data_device->drag_offer);
        }
    } else {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_drop on data_offer 0x%08x serial %d",
                     -1, -1);

        DestroyDataOffer(data_device->drag_offer);
    }

    data_device->drag_offer = NULL;
}

static void data_device_handle_selection(void *data, struct wl_data_device *wl_data_device, struct wl_data_offer *id)
{
    Wayland_DataDevice *data_device = data;
    Wayland_DataOffer *offer = NULL;

    if (id) {
        offer = wl_data_offer_get_user_data(id);
    }

    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In data_device_listener . data_device_handle_selection on data_offer 0x%08x",
                 (id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1));

    DataDeviceSetOffer(data_device, offer);
}

static const struct wl_data_device_listener data_device_listener = {
    data_device_handle_data_offer,
    data_device_handle_enter,
    data_device_handle_leave,
    data_device_handle_motion,
    data_device_handle_drop,
    data_device_handle_selection
};

static void primary_selection_device_handle_offer(void *data, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1, struct zwp_primary_selection_offer_v1 *id)
{
    Wayland_PrimarySelectionOffer *primary_selection_offer = SDL_calloc(1, sizeof(*primary_selection_offer));
    if (primary_selection_offer) {
        Wayland_PrimarySelectionDevice *primary_selection_device = (Wayland_PrimarySelectionDevice *)data;
        primary_selection_device->seat->display->current_primary_selection_seat = primary_selection_device->seat;
        primary_selection_offer->offer = id;
        primary_selection_offer->primary_selection_device = primary_selection_device;
        WAYLAND_wl_array_init(&primary_selection_offer->mimes);
        zwp_primary_selection_offer_v1_set_user_data(id, primary_selection_offer);
        zwp_primary_selection_offer_v1_add_listener(id, &primary_selection_offer_listener, primary_selection_offer);
    }
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In zwp_primary_selection_device_v1_listener . primary_selection_device_handle_offer on primary_selection_offer 0x%08x",
                 (id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1));
}

static void primary_selection_device_handle_selection(void *data, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1,
                                                      struct zwp_primary_selection_offer_v1 *id)
{
    Wayland_PrimarySelectionDevice *primary_selection_device = data;
    Wayland_PrimarySelectionOffer *offer = NULL;

    if (id) {
        offer = zwp_primary_selection_offer_v1_get_user_data(id);
    }

    if (primary_selection_device->offer != offer) {
        DestroyPrimarySelectionOffer(primary_selection_device->offer);
        primary_selection_device->offer = offer;
    }
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In zwp_primary_selection_device_v1_listener . primary_selection_device_handle_selection on primary_selection_offer 0x%08x",
                 (id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1));
}

static const struct zwp_primary_selection_device_v1_listener primary_selection_device_listener = {
    primary_selection_device_handle_offer,
    primary_selection_device_handle_selection
};

static void DataDeviceSetID(Wayland_DataDevice *data_device)
{
#ifdef SDL_USE_LIBDBUS
    if (!data_device->id_str)
    {
        SDL_DBusContext *dbus = SDL_DBus_GetContext();
        if (dbus) {
            const char *id = dbus->bus_get_unique_name(dbus->session_conn);
            if (id) {
                data_device->id_str = SDL_strdup(id);
            }
        }
    }
#endif
    if (!data_device->id_str)
    {
        char id[24];
        Uint64 pid = (Uint64)getpid();
        SDL_snprintf(id, sizeof(id), "%" SDL_PRIu64, pid);
        data_device->id_str = SDL_strdup(id);
    }
}

bool Wayland_CreateDataDevice(SDL_WaylandSeat *seat)
{
    if (!seat || !seat->display->data_device_manager || seat->data_device) {
        return false;
    }

    Wayland_DataDevice *data_device = SDL_calloc(1, sizeof(*data_device));
    if (!data_device) {
        return false;
    }

    data_device->data_device = wl_data_device_manager_get_data_device(seat->display->data_device_manager, seat->wl_seat);
    data_device->seat = seat;

    if (!data_device->data_device) {
        SDL_free(data_device);
        return false;
    }

    DataDeviceSetID(data_device);
    WAYLAND_wl_list_init(&data_device->pending_drops);
    wl_data_device_set_user_data(data_device->data_device, data_device);
    wl_data_device_add_listener(data_device->data_device, &data_device_listener, data_device);
    seat->data_device = data_device;

    return true;
}

void Wayland_DestroyDataDevice(Wayland_DataDevice *data_device)
{
    if (!data_device) {
        return;
    }

    if (data_device->selection_offer) {
        DestroyDataOffer(data_device->selection_offer);
    }
    if (data_device->selection_source) {
        DestroyDataSource(data_device->selection_source);
    }
    if (data_device->drag_offer) {
        DestroyDataOffer(data_device->drag_offer);
    }

    // Free any remaining drop events.
    Wayland_DropData *drop, *drop_temp;
    wl_list_for_each_safe (drop, drop_temp, &data_device->pending_drops, link) {
        Wayland_JobThreadCancelJob(data_device->seat->display->job_thread_context, drop->id);
        DestroyDropData(drop);
    }

    if (data_device->data_device) {
        if (wl_data_device_get_version(data_device->data_device) >= WL_DATA_DEVICE_RELEASE_SINCE_VERSION) {
            wl_data_device_release(data_device->data_device);
        } else {
            wl_data_device_destroy(data_device->data_device);
        }
    }
    SDL_free(data_device->id_str);
    SDL_free(data_device);
}

bool Wayland_DataDeviceSetSelectionSource(Wayland_DataDevice *data_device, const char *const *mime_types, size_t mime_count,
                                          SDL_ClipboardDataCallback data_callback, SDL_ClipboardCleanupCallback cleanup_callback, void *userdata, Uint32 sequence)
{
    if (!data_device) {
        return SDL_SetError("Invalid Data Device");
    }

    if (mime_types && mime_count && data_callback) {
        Wayland_DataSource *source = CreateDataSource(data_device);
        if (!source) {
            return SDL_SetError("Data source allocation failed");
        }

        source->data_callback = data_callback;
        source->cleanup_callback = cleanup_callback;
        source->userdata.sequence = sequence;
        source->userdata.data = userdata;

        for (size_t index = 0; index < mime_count; ++index) {
            const char *mime_type = mime_types[index];
            wl_data_source_offer(source->source, mime_type);
        }

        // Advertise the data origin MIME.
        wl_data_source_offer(source->source, SDL_DATA_ORIGIN_MIME);

        // Destroy the old source after setting the new source, as some clipboard managers prefer this behavior.
        wl_data_device_set_selection(data_device->data_device, source->source, data_device->seat->last_implicit_grab_serial);
        DestroyDataSource(data_device->selection_source);

        data_device->selection_source = source;
    } else {
        DestroyDataSource(data_device->selection_source);
        data_device->selection_source = NULL;
    }

    return true;
}

void Wayland_DataDeviceDestroySelectionOffer(Wayland_DataDevice *data_device, bool clear)
{
    if (data_device) {
        if (clear) {
            DataDeviceSetOffer(data_device, NULL);
        } else {
            DestroyDataOffer(data_device->selection_offer);
            data_device->selection_offer = NULL;
        }
    }
}

bool Wayland_DataDeviceHasSelectionSource(Wayland_DataDevice *data_device)
{
    return data_device && data_device->selection_source;
}

bool Wayland_DataDeviceSelectionOfferHasMIME(Wayland_DataDevice *data_device, const char *mime_type)
{
    if (data_device && data_device->selection_offer && mime_type) {
        return MIMEDataListHasType(&data_device->selection_offer->mimes, mime_type);
    }
    return false;
}

void Wayland_DataDeviceRemoveWindowReferences(Wayland_DataDevice *data_device, SDL_Window *window)
{
    if (!data_device) {
        return;
    }

    SDL_WaylandSeat *seat = data_device->seat;
    SDL_VideoData *viddata = seat->display;

    // Cancel any pending drop events.
    Wayland_DropData *drop, *drop_temp;
    wl_list_for_each_safe (drop, drop_temp, &seat->data_device->pending_drops, link) {
        if (drop->dnd_window == window) {
            Wayland_JobThreadCancelJob(viddata->job_thread_context, drop->id);
            DestroyDropData(drop);
        }
    }
}

bool Wayland_CreatePrimarySelectionDevice(SDL_WaylandSeat *seat)
{
    if (!seat || !seat->display->primary_selection_device_manager || seat->primary_selection_device) {
        return false;
    }

    Wayland_PrimarySelectionDevice *primary_selection_device = SDL_calloc(1, sizeof(Wayland_PrimarySelectionDevice));
    if (!primary_selection_device) {
        return false;
    }

    primary_selection_device->primary_selection_device = zwp_primary_selection_device_manager_v1_get_device(
        seat->display->primary_selection_device_manager, seat->wl_seat);
    primary_selection_device->seat = seat;

    if (!primary_selection_device->primary_selection_device) {
        SDL_free(primary_selection_device);
    } else {
        zwp_primary_selection_device_v1_set_user_data(primary_selection_device->primary_selection_device, primary_selection_device);
        zwp_primary_selection_device_v1_add_listener(primary_selection_device->primary_selection_device, &primary_selection_device_listener, primary_selection_device);
        seat->primary_selection_device = primary_selection_device;
    }

    return true;
}

void Wayland_DestroyPrimarySelectionDevice(Wayland_PrimarySelectionDevice *primary_selection_device)
{
    if (!primary_selection_device) {
        return;
    }

    if (primary_selection_device->offer) {
        DestroyPrimarySelectionOffer(primary_selection_device->offer);
    }
    if (primary_selection_device->source) {
        DestroyPrimarySelectionSource(primary_selection_device->source);
    }
    if (primary_selection_device->primary_selection_device) {
        zwp_primary_selection_device_v1_destroy(primary_selection_device->primary_selection_device);
    }
    SDL_free(primary_selection_device);
}

bool Wayland_PrimarySelectionDeviceSetText(Wayland_PrimarySelectionDevice *primary_selection_device, const char *text)
{
    if (!primary_selection_device) {
        return SDL_SetError("Invalid Primary Selection Device");
    }

    if (text && *text != '\0') {
        char *text_copy = SDL_strdup(text);
        if (!text_copy) {
            return false;
        }
        Wayland_PrimarySelectionSource *source = CreatePrimarySelectionSource(primary_selection_device);
        if (!source) {
            SDL_free(text_copy);
            SDL_SetError("Failed to create primary selection source");
        }

        source->data_callback = SDL_ClipboardTextCallback;
        source->cleanup_callback = SDL_free;
        source->userdata.sequence = 0;
        source->userdata.data = text_copy;

        size_t mime_type_count = 0;
        const char *const *mime_types = Wayland_GetTextMimeTypes(NULL, &mime_type_count);
        for (size_t index = 0; index < mime_type_count; ++index) {
            const char *mime_type = mime_types[index];
            zwp_primary_selection_source_v1_offer(source->source, mime_type);
        }

        // Destroy the old source after setting the new source, as some clipboard managers prefer this behavior.
        zwp_primary_selection_device_v1_set_selection(primary_selection_device->primary_selection_device, source->source, primary_selection_device->seat->last_implicit_grab_serial);
        DestroyPrimarySelectionSource(primary_selection_device->source);

        primary_selection_device->source = source;
    } else {
        DestroyPrimarySelectionSource(primary_selection_device->source);
        primary_selection_device->source = NULL;
    }

    return true;
}

char *Wayland_PrimarySelectionDeviceGetText(Wayland_PrimarySelectionDevice *primary_selection_device, size_t *length)
{
    if (!primary_selection_device) {
        SDL_SetError("Invalid primary selection device");
        return NULL;
    }

    if (primary_selection_device->source) {
        char *buf = SDL_strdup(primary_selection_device->source->userdata.data);
        if (buf && length) {
            *length = strlen(buf);
        }
        return buf;
    } else if (primary_selection_device->offer) {
        size_t mime_type_count = 0;
        const char *const *mime_types = Wayland_GetTextMimeTypes(NULL, &mime_type_count);
        for (size_t i = 0; i < mime_type_count; ++i) {
            if (MIMEDataListHasType(&primary_selection_device->offer->mimes, mime_types[i])) {
                return PrimarySelectionDeviceReceiveOffer(primary_selection_device, mime_types[i], length);
            }
        }
    }

    return NULL;
}

bool Wayland_PrimarySelectionDeviceHasText(Wayland_PrimarySelectionDevice *primary_selection_device)
{
    if (!primary_selection_device) {
        return false;
    }

    if (primary_selection_device->source) {
        return true;
    } else if (primary_selection_device->offer) {
        size_t mime_type_count = 0;
        const char *const *mime_types = Wayland_GetTextMimeTypes(NULL, &mime_type_count);
        for (size_t i = 0; i < mime_type_count; ++i) {
            if (MIMEDataListHasType(&primary_selection_device->offer->mimes, mime_types[i])) {
                return true;
            }
        }
    }

    return false;
}

void Wayland_PrimarySelectionDeviceDestroyOffer(Wayland_PrimarySelectionDevice *primary_selection_device)
{
    if (!primary_selection_device) {
        return;
    }

    DestroyPrimarySelectionOffer(primary_selection_device->offer);
    primary_selection_device->offer = NULL;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
