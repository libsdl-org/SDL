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
#include <unistd.h>
#include <limits.h>
#include <signal.h>

#include "../../core/unix/SDL_poll.h"
#include "../../events/SDL_events_c.h"
#include "../SDL_clipboard_c.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylanddatamanager.h"
#include "primary-selection-unstable-v1-client-protocol.h"

/* This is arbitrary, but reading while polling should block for less than a frame, to
 * prevent hanging while pumping events.
 *
 * When querying the clipboard data directly, a larger value is needed to avoid timing
 * out if the source needs to process or transfer a large amount of data.
 */
#define DEFAULT_PIPE_TIMEOUT_NS SDL_MS_TO_NS(14)
#define EXTENDED_PIPE_TIMEOUT_NS SDL_MS_TO_NS(5000)

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
    } else  {
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

static SDL_MimeDataList *MIMEDataListFind(struct wl_list *list, const char *mime_type)
{
    SDL_MimeDataList *found = NULL;

    SDL_MimeDataList *item = NULL;
    wl_list_for_each (item, list, link) {
        if (!item->mime_type) {
            continue;
        }

        if (SDL_strcmp(item->mime_type, mime_type) == 0) {
            found = item;
            break;
        }
    }
    return found;
}

static bool MIMEDataListAdd(struct wl_list *list, const char *mime_type, const void *buffer, size_t length)
{
    bool result = true;
    void *internal_buffer = NULL;

    if (buffer) {
        internal_buffer = SDL_malloc(length);
        if (!internal_buffer) {
            return false;
        }
        SDL_memcpy(internal_buffer, buffer, length);
    }

    SDL_MimeDataList *mime_data = MIMEDataListFind(list, mime_type);

    if (!mime_data) {
        mime_data = SDL_calloc(1, sizeof(*mime_data));
        if (!mime_data) {
            result = false;
        } else {
            WAYLAND_wl_list_insert(list, &(mime_data->link));

            const size_t mime_type_length = SDL_strlen(mime_type) + 1;
            mime_data->mime_type = SDL_malloc(mime_type_length);
            if (!mime_data->mime_type) {
                result = false;
            } else {
                SDL_memcpy(mime_data->mime_type, mime_type, mime_type_length);
            }
        }
    }

    if (mime_data && buffer && length > 0) {
        SDL_free(mime_data->data);
        mime_data->data = internal_buffer;
        mime_data->length = length;
    } else {
        SDL_free(internal_buffer);
    }

    return result;
}

static void MIMEDataListFree(struct wl_list *list)
{
    SDL_MimeDataList *mime_data = NULL;
    SDL_MimeDataList *next = NULL;

    wl_list_for_each_safe (mime_data, next, list, link) {
        SDL_free(mime_data->data);
        SDL_free(mime_data->mime_type);
        SDL_free(mime_data);
    }
}

static void data_source_handle_target(void *data, struct wl_data_source *wl_data_source, const char *mime_type)
{
}

static void data_source_handle_send(void *data, struct wl_data_source *wl_data_source, const char *mime_type, int32_t fd)
{
    Wayland_DataSourceSend((SDL_WaylandDataSource *)data, mime_type, fd);
}

static void data_source_handle_cancelled(void *data, struct wl_data_source *wl_data_source)
{
    SDL_WaylandDataSource *source = data;
    if (source) {
        Wayland_DataSourceDestroy(source);
    }
}

static void data_source_handle_dnd_drop_performed(void *data, struct wl_data_source *wl_data_source)
{
}

static void data_source_handle_dnd_finished(void *data, struct wl_data_source *wl_data_source)
{
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

SDL_WaylandDataSource *Wayland_DataSourceCreate(SDL_VideoData *video_data)
{
    SDL_WaylandDataSource *data_source = SDL_calloc(1, sizeof(*data_source));
    if (!data_source) {
        return NULL;
    }

    struct wl_data_source *id = wl_data_device_manager_create_data_source(video_data->data_device_manager);
    data_source->source = id;
    wl_data_source_set_user_data(id, data_source);
    wl_data_source_add_listener(id, &data_source_listener, data_source);

    return data_source;
}

static void primary_selection_source_send(void *data, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1, const char *mime_type, int32_t fd)
{
    Wayland_PrimarySelectionSourceSend((SDL_WaylandPrimarySelectionSource *)data, mime_type, fd);
}

static void primary_selection_source_cancelled(void *data, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1)
{
    Wayland_PrimarySelectionSourceDestroy(data);
}

static const struct zwp_primary_selection_source_v1_listener primary_selection_source_listener = {
    primary_selection_source_send,
    primary_selection_source_cancelled,
};

SDL_WaylandPrimarySelectionSource *Wayland_PrimarySelectionSourceCreate(SDL_VideoData *video_data)
{
    SDL_WaylandPrimarySelectionSource *primary_selection_source = SDL_calloc(1, sizeof(*primary_selection_source));
    if (!primary_selection_source) {
        return NULL;
    }

    struct zwp_primary_selection_source_v1 *id = zwp_primary_selection_device_manager_v1_create_source(video_data->primary_selection_device_manager);
    primary_selection_source->source = id;
    zwp_primary_selection_source_v1_add_listener(id, &primary_selection_source_listener, primary_selection_source);

    return primary_selection_source;
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

ssize_t Wayland_DataSourceSend(SDL_WaylandDataSource *source, const char *mime_type, int fd)
{
    const void *data = NULL;
    size_t length = 0;

    if (SDL_strcmp(mime_type, SDL_DATA_ORIGIN_MIME) == 0) {
        data = source->data_device->id_str;
        length = SDL_strlen(source->data_device->id_str);
    } else if (source->callback) {
        data = source->callback(source->userdata.data, mime_type, &length);
    }

    return SendData(data, length, fd);
}

ssize_t Wayland_PrimarySelectionSourceSend(SDL_WaylandPrimarySelectionSource *source, const char *mime_type, int fd)
{
    const void *data = NULL;
    size_t length = 0;

    if (source->callback) {
        data = source->callback(source->userdata.data, mime_type, &length);
    }

    return SendData(data, length, fd);
}

void Wayland_DataSourceSetCallback(SDL_WaylandDataSource *source, SDL_ClipboardDataCallback callback, void *userdata, Uint32 sequence)
{
    if (source) {
        source->callback = callback;
        source->userdata.sequence = sequence;
        source->userdata.data = userdata;
    }
}

void Wayland_PrimarySelectionSourceSetCallback(SDL_WaylandPrimarySelectionSource *source, SDL_ClipboardDataCallback callback, void *userdata)
{
    if (source) {
        source->callback = callback;
        source->userdata.sequence = 0;
        source->userdata.data = userdata;
    }
}

static void *CloneDataBuffer(const void *buffer, const size_t *len)
{
    void *clone = NULL;
    if (*len > 0 && buffer) {
        clone = SDL_malloc((*len)+sizeof(Uint32));
        if (clone) {
            SDL_memcpy(clone, buffer, *len);
            SDL_memset((Uint8 *)clone + *len, 0, sizeof(Uint32));
        }
    }
    return clone;
}

void *Wayland_DataSourceGetData(SDL_WaylandDataSource *source, const char *mime_type, size_t *length)
{
    void *buffer = NULL;
    *length = 0;

    if (!source) {
        SDL_SetError("Invalid data source");
    } else if (source->callback) {
        const void *internal_buffer = source->callback(source->userdata.data, mime_type, length);
        buffer = CloneDataBuffer(internal_buffer, length);
    }

    return buffer;
}

void *Wayland_PrimarySelectionSourceGetData(SDL_WaylandPrimarySelectionSource *source, const char *mime_type, size_t *length)
{
    void *buffer = NULL;
    *length = 0;

    if (!source) {
        SDL_SetError("Invalid primary selection source");
    } else if (source->callback) {
        const void *internal_buffer = source->callback(source->userdata.data, mime_type, length);
        buffer = CloneDataBuffer(internal_buffer, length);
    }

    return buffer;
}

void Wayland_DataSourceDestroy(SDL_WaylandDataSource *source)
{
    if (source) {
        SDL_WaylandDataDevice *data_device = source->data_device;
        if (data_device && (data_device->selection_source == source)) {
            data_device->selection_source = NULL;
        }
        wl_data_source_destroy(source->source);
        if (source->userdata.sequence) {
            SDL_CancelClipboardData(source->userdata.sequence);
        } else {
            SDL_free(source->userdata.data);
        }
        SDL_free(source);
    }
}

void Wayland_PrimarySelectionSourceDestroy(SDL_WaylandPrimarySelectionSource *source)
{
    if (source) {
        SDL_WaylandPrimarySelectionDevice *primary_selection_device = source->primary_selection_device;
        if (primary_selection_device && (primary_selection_device->selection_source == source)) {
            primary_selection_device->selection_source = NULL;
        }
        zwp_primary_selection_source_v1_destroy(source->source);
        if (source->userdata.sequence == 0) {
            SDL_free(source->userdata.data);
        }
        SDL_free(source);
    }
}

static void offer_source_done_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    if (!callback) {
        return;
    }

    SDL_WaylandDataOffer *offer = (SDL_WaylandDataOffer *)data;
    char *id = NULL;
    size_t length = 0;

    wl_callback_destroy(offer->callback);
    offer->callback = NULL;

    while (ReadPipe(offer->read_fd, (void **)&id, &length, DEFAULT_PIPE_TIMEOUT_NS) > 0) {
    }
    close(offer->read_fd);
    offer->read_fd = -1;

    if (id) {
        const bool source_is_external = SDL_strncmp(offer->data_device->id_str, id, length) != 0;
        SDL_free(id);
        if (source_is_external) {
            Wayland_DataOfferNotifyFromMIMEs(offer, false);
        } else {
            // Recursive data offer; just destroy it.
            SDL_WaylandDataDevice *data_device = offer->data_device;
            Wayland_DataOfferDestroy(offer);
            data_device->selection_offer = NULL;
        }
    }
}

static struct wl_callback_listener offer_source_listener = {
    offer_source_done_handler
};

static void DataOfferCheckSource(SDL_WaylandDataOffer *offer, const char *mime_type)
{
    int pipefd[2];

    if (!offer) {
        return;
    }
    SDL_WaylandDataDevice *data_device = offer->data_device;

    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
        if (offer->callback) {
            wl_callback_destroy(offer->callback);
        }
        if (offer->read_fd >= 0) {
            close(offer->read_fd);
        }

        offer->read_fd = pipefd[0];

        wl_data_offer_receive(offer->offer, mime_type, pipefd[1]);
        close(pipefd[1]);

        offer->callback = wl_display_sync(offer->data_device->seat->display->display);
        wl_callback_add_listener(offer->callback, &offer_source_listener, offer);

        WAYLAND_wl_display_flush(data_device->seat->display->display);
    }
}

static void SetCurrentClipboardOffer(SDL_WaylandDataOffer *offer)
{
    SDL_WaylandSeat *offer_seat = offer->data_device->seat;
    SDL_VideoData *video_data = offer_seat->display;

    // Clear any existing references to the existing clipboard data before replacing the current offer.
    SDL_WaylandSeat *s;
    wl_list_for_each (s, &video_data->seat_list, link) {
        if (s->data_device) {
            Wayland_DataSourceDestroy(s->data_device->selection_source);
            s->data_device->selection_source = NULL;

            // Don't clear the offer that is about to be set.
            if (s != offer_seat) {
                Wayland_DataOfferDestroy(s->data_device->selection_offer);
                s->data_device->selection_offer = NULL;
            }
        }
    }

    video_data->current_data_offer_seat = offer_seat;
}

void Wayland_DataOfferNotifyFromMIMEs(SDL_WaylandDataOffer *offer, bool check_origin)
{
    int nformats = 0;
    char **new_mime_types = NULL;
    if (offer) {
        size_t alloc_size = 0;

        // Do a first pass to compute allocation size.
        SDL_MimeDataList *item = NULL;
        wl_list_for_each(item, &offer->mimes, link) {
            if (!item->mime_type) {
                continue;
            }

            // If origin metadata is found, queue a check and wait for confirmation that this offer isn't recursive.
            if (check_origin && SDL_strcmp(item->mime_type, SDL_DATA_ORIGIN_MIME) == 0) {
                DataOfferCheckSource(offer, item->mime_type);
                return;
            }

            ++nformats;
            alloc_size += SDL_strlen(item->mime_type) + 1;
        }

        alloc_size += (nformats + 1) * sizeof(char *);

        new_mime_types = SDL_AllocateTemporaryMemory(alloc_size);
        if (!new_mime_types) {
            SDL_LogError(SDL_LOG_CATEGORY_INPUT, "unable to allocate new_mime_types");
            return;
        }

        // Second pass to fill.
        char *strPtr = (char *)(new_mime_types + nformats + 1);
        item = NULL;
        int i = 0;
        wl_list_for_each(item, &offer->mimes, link) {
            if (!item->mime_type) {
                continue;
            }

            new_mime_types[i] = strPtr;
            strPtr = stpcpy(strPtr, item->mime_type) + 1;
            i++;
        }
        new_mime_types[nformats] = NULL;
    }

    SetCurrentClipboardOffer(offer);
    SDL_SendClipboardUpdate(false, new_mime_types, nformats);
}

void *Wayland_DataOfferReceive(SDL_WaylandDataOffer *offer, const char *mime_type, size_t *length, bool extended_timeout)
{
    const Sint64 timeout = extended_timeout ? EXTENDED_PIPE_TIMEOUT_NS : DEFAULT_PIPE_TIMEOUT_NS;

    int pipefd[2];
    void *buffer = NULL;
    *length = 0;

    if (!offer) {
        SDL_SetError("Invalid data offer");
        return NULL;
    }

    SDL_WaylandDataDevice *data_device = offer->data_device;

    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
        wl_data_offer_receive(offer->offer, mime_type, pipefd[1]);
        close(pipefd[1]);

        WAYLAND_wl_display_flush(data_device->seat->display->display);

        while (ReadPipe(pipefd[0], &buffer, length, timeout) > 0) {
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

void *Wayland_PrimarySelectionOfferReceive(SDL_WaylandPrimarySelectionOffer *offer, const char *mime_type, size_t *length)
{
    int pipefd[2];
    void *buffer = NULL;
    *length = 0;

    if (!offer) {
        SDL_SetError("Invalid data offer");
        return NULL;
    }

    SDL_WaylandPrimarySelectionDevice *primary_selection_device = offer->primary_selection_device;

    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
        zwp_primary_selection_offer_v1_receive(offer->offer, mime_type, pipefd[1]);
        close(pipefd[1]);

        WAYLAND_wl_display_flush(primary_selection_device->seat->display->display);

        while (ReadPipe(pipefd[0], &buffer, length, EXTENDED_PIPE_TIMEOUT_NS) > 0) {
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

bool Wayland_DataOfferAddMIME(SDL_WaylandDataOffer *offer, const char *mime_type)
{
    return MIMEDataListAdd(&offer->mimes, mime_type, NULL, 0);
}

bool Wayland_PrimarySelectionOfferAddMIME(SDL_WaylandPrimarySelectionOffer *offer, const char *mime_type)
{
    return MIMEDataListAdd(&offer->mimes, mime_type, NULL, 0);
}

bool Wayland_DataOfferHasMIME(SDL_WaylandDataOffer *offer, const char *mime_type)
{
    bool found = false;

    if (offer) {
        found = MIMEDataListFind(&offer->mimes, mime_type) != NULL;
    }
    return found;
}

bool Wayland_PrimarySelectionOfferHasMIME(SDL_WaylandPrimarySelectionOffer *offer, const char *mime_type)
{
    bool found = false;

    if (offer) {
        found = MIMEDataListFind(&offer->mimes, mime_type) != NULL;
    }
    return found;
}

void Wayland_DataOfferDestroy(SDL_WaylandDataOffer *offer)
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

void Wayland_PrimarySelectionOfferDestroy(SDL_WaylandPrimarySelectionOffer *offer)
{
    if (offer) {
        zwp_primary_selection_offer_v1_destroy(offer->offer);
        MIMEDataListFree(&offer->mimes);
        SDL_free(offer);
    }
}

bool Wayland_DataDeviceSetSelection(SDL_WaylandDataDevice *data_device, SDL_WaylandDataSource *source, const char **mime_types, size_t mime_count)
{
    if (!data_device) {
        return SDL_SetError("Invalid Data Device");
    }
    if (!source) {
        return SDL_SetError("Invalid source");
    }

    if (mime_count) {
        for (size_t index = 0; index < mime_count; ++index) {
            const char *mime_type = mime_types[index];
            wl_data_source_offer(source->source, mime_type);
        }

        // Advertise the data origin MIME
        wl_data_source_offer(source->source, SDL_DATA_ORIGIN_MIME);

        // Only set if there is a valid serial if not set it later
        if (data_device->selection_serial != 0) {
            wl_data_device_set_selection(data_device->data_device, source->source, data_device->selection_serial);
        }
        if (data_device->selection_source) {
            Wayland_DataSourceDestroy(data_device->selection_source);
        }
        data_device->selection_source = source;
        source->data_device = data_device;

    } else {
        Wayland_DataSourceDestroy(data_device->selection_source);
        data_device->selection_source = NULL;
        return SDL_SetError("No mime data");
    }

    return true;
}

bool Wayland_PrimarySelectionDeviceSetSelection(SDL_WaylandPrimarySelectionDevice *primary_selection_device,
                                                SDL_WaylandPrimarySelectionSource *source,
                                                const char *const *mime_types, size_t mime_count)
{
    if (!primary_selection_device) {
        return SDL_SetError("Invalid Primary Selection Device");
    }
    if (!source) {
        return SDL_SetError("Invalid source");
    }

    if (mime_count) {
        for (size_t index = 0; index < mime_count; ++index) {
            const char *mime_type = mime_types[index];
            zwp_primary_selection_source_v1_offer(source->source, mime_type);
        }

        // Only set if there is a valid serial if not set it later
        if (primary_selection_device->selection_serial != 0) {
            zwp_primary_selection_device_v1_set_selection(primary_selection_device->primary_selection_device,
                                                          source->source,
                                                          primary_selection_device->selection_serial);
        }
        if (primary_selection_device->selection_source) {
            Wayland_PrimarySelectionSourceDestroy(primary_selection_device->selection_source);
        }
        primary_selection_device->selection_source = source;
        source->primary_selection_device = primary_selection_device;
    } else {
        Wayland_PrimarySelectionSourceDestroy(primary_selection_device->selection_source);
        primary_selection_device->selection_source = NULL;
        return SDL_SetError("No mime data");
    }

    return true;
}

void Wayland_DataDeviceSetSerial(SDL_WaylandDataDevice *data_device, uint32_t serial)
{
    if (data_device) {
        // If there was no serial and there is a pending selection, set it now.
        if (data_device->selection_serial == 0 && data_device->selection_source) {
            wl_data_device_set_selection(data_device->data_device, data_device->selection_source->source, serial);
        }

        data_device->selection_serial = serial;
    }
}

void Wayland_PrimarySelectionDeviceSetSerial(SDL_WaylandPrimarySelectionDevice *primary_selection_device, uint32_t serial)
{
    if (primary_selection_device) {
        // If there was no serial and there is a pending selection, set it now.
        if (primary_selection_device->selection_serial == 0 && primary_selection_device->selection_source) {
            zwp_primary_selection_device_v1_set_selection(primary_selection_device->primary_selection_device,
                                                          primary_selection_device->selection_source->source,
                                                          serial);
        }

        primary_selection_device->selection_serial = serial;
    }
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
