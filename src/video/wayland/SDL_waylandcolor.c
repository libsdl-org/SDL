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

#include "SDL_waylandcolor.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylandwindow.h"
#include "color-management-v1-client-protocol.h"
#include "../../events/SDL_windowevents_c.h"

typedef struct Wayland_ColorInfoState
{
    struct wp_image_description_v1 *wp_image_description;
    struct wp_image_description_info_v1 *wp_image_description_info;
    struct wl_event_queue *queue;

    union
    {
        SDL_WindowData *window_data;
        SDL_DisplayData *display_data;
    };

    enum
    {
        WAYLAND_COLOR_OBJECT_TYPE_WINDOW,
        WAYLAND_COLOR_OBJECT_TYPE_DISPLAY
    } object_type;

    SDL_HDROutputProperties HDR;

    // The ICC fd is only valid if the size is non-zero.
    int icc_fd;
    Uint32 icc_size;

    bool deferred_event_processing;
} Wayland_ColorInfoState;

static void Wayland_CancelColorInfoRequest(Wayland_ColorInfoState *state)
{
    if (state) {
        if (state->wp_image_description_info) {
            wp_image_description_info_v1_destroy(state->wp_image_description_info);
            state->wp_image_description_info = NULL;
        }
        if (state->wp_image_description) {
            wp_image_description_v1_destroy(state->wp_image_description);
            state->wp_image_description = NULL;
        }
    }
}

void Wayland_FreeColorInfoState(Wayland_ColorInfoState *state)
{
    if (state) {
        Wayland_CancelColorInfoRequest(state);
        if (state->queue) {
            WAYLAND_wl_event_queue_destroy(state->queue);
        }

        switch (state->object_type) {
        case WAYLAND_COLOR_OBJECT_TYPE_WINDOW:
            state->window_data->color_info_state = NULL;
            break;
        case WAYLAND_COLOR_OBJECT_TYPE_DISPLAY:
            state->display_data->color_info_state = NULL;
            break;
        }

        SDL_free(state);
    }
}

static void image_description_info_handle_done(void *data,
                                               struct wp_image_description_info_v1 *wp_image_description_info_v1)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;
    Wayland_CancelColorInfoRequest(state);

    switch (state->object_type) {
        case WAYLAND_COLOR_OBJECT_TYPE_WINDOW:
        {
            SDL_SetWindowHDRProperties(state->window_data->sdlwindow, &state->HDR, true);
            if (state->icc_size) {
                state->window_data->icc_fd = state->icc_fd;
                state->window_data->icc_size = state->icc_size;
                SDL_SendWindowEvent(state->window_data->sdlwindow, SDL_EVENT_WINDOW_ICCPROF_CHANGED, 0, 0);
            }
        } break;
        case WAYLAND_COLOR_OBJECT_TYPE_DISPLAY:
        {
            SDL_copyp(&state->display_data->HDR, &state->HDR);

            if (state->display_data->display) {
                SDL_VideoDisplay *disp = SDL_GetVideoDisplay(state->display_data->display);
                if (disp) {
                    SDL_SetDisplayHDRProperties(disp, &state->HDR);
                }
            } else {
                SDL_copyp(&state->display_data->placeholder.HDR, &state->HDR);
            }
        } break;
    }
}

static void image_description_info_handle_icc_file(void *data,
                                                   struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                   int32_t icc, uint32_t icc_size)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;

    state->icc_fd = icc;
    state->icc_size = icc_size;
}

static void image_description_info_handle_primaries(void *data,
                                                    struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                    int32_t r_x, int32_t r_y,
                                                    int32_t g_x, int32_t g_y,
                                                    int32_t b_x, int32_t b_y,
                                                    int32_t w_x, int32_t w_y)
{
    // NOP
}

static void image_description_info_handle_primaries_named(void *data,
                                                          struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                          uint32_t primaries)
{
    // NOP
}

static void image_description_info_handle_tf_power(void *data,
                                                   struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                   uint32_t eexp)
{
    // NOP
}

static void image_description_info_handle_tf_named(void *data,
                                                   struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                   uint32_t tf)
{
    // NOP
}

static void image_description_info_handle_luminances(void *data,
                                                     struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                     uint32_t min_lum,
                                                     uint32_t max_lum,
                                                     uint32_t reference_lum)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;
    state->HDR.HDR_headroom = (float)max_lum / (float)reference_lum;
}

static void image_description_info_handle_target_primaries(void *data,
                                                           struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                           int32_t r_x, int32_t r_y,
                                                           int32_t g_x, int32_t g_y,
                                                           int32_t b_x, int32_t b_y,
                                                           int32_t w_x, int32_t w_y)
{
    // NOP
}

static void image_description_info_handle_target_luminance(void *data,
                                                           struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                           uint32_t min_lum,
                                                           uint32_t max_lum)
{
    // NOP
}

static void image_description_info_handle_target_max_cll(void *data,
                                                         struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                         uint32_t max_cll)
{
    // NOP
}

static void image_description_info_handle_target_max_fall(void *data,
                                                          struct wp_image_description_info_v1 *wp_image_description_info_v1,
                                                          uint32_t max_fall)
{
    // NOP
}

static const struct wp_image_description_info_v1_listener image_description_info_listener = {
    image_description_info_handle_done,
    image_description_info_handle_icc_file,
    image_description_info_handle_primaries,
    image_description_info_handle_primaries_named,
    image_description_info_handle_tf_power,
    image_description_info_handle_tf_named,
    image_description_info_handle_luminances,
    image_description_info_handle_target_primaries,
    image_description_info_handle_target_luminance,
    image_description_info_handle_target_max_cll,
    image_description_info_handle_target_max_fall
};

static void PumpColorspaceEvents(Wayland_ColorInfoState *state)
{
    SDL_VideoData *vid = SDL_GetVideoDevice()->internal;

    // Run the image description sequence to completion in its own queue.
    while (state->wp_image_description) {
        WAYLAND_wl_display_dispatch_queue(vid->display, state->queue);
    }

    Wayland_FreeColorInfoState(state);
}

static void image_description_handle_failed(void *data,
                                            struct wp_image_description_v1 *wp_image_description_v1,
                                            uint32_t cause,
                                            const char *msg)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;
    Wayland_CancelColorInfoRequest(state);

    if (state->deferred_event_processing) {
        Wayland_FreeColorInfoState(state);
    }
}

static void image_description_handle_ready2(void *data,
                                            struct wp_image_description_v1 *wp_image_description_v1,
                                            uint32_t identity_hi, uint32_t identity_lo)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;

    /* If event processing was deferred, then the image description is on the default queue.
     * Otherwise, it will inherit the queue from the image description object.
     */
    if (state->deferred_event_processing) {
        SDL_VideoData *vid = SDL_GetVideoDevice()->internal;
        state->queue = Wayland_DisplayCreateQueue(vid->display, "SDL Color Management Queue");

        struct wl_proxy *image_desc_wrapper = WAYLAND_wl_proxy_create_wrapper(state->wp_image_description);
        WAYLAND_wl_proxy_set_queue(image_desc_wrapper, state->queue);
        state->wp_image_description_info = wp_image_description_v1_get_information((struct wp_image_description_v1 *)image_desc_wrapper);
        WAYLAND_wl_proxy_wrapper_destroy(image_desc_wrapper);
    } else {
        state->wp_image_description_info = wp_image_description_v1_get_information(state->wp_image_description);
    }
    wp_image_description_info_v1_add_listener(state->wp_image_description_info, &image_description_info_listener, data);

    if (state->deferred_event_processing) {
        PumpColorspaceEvents(state);
    }
}

static void image_description_handle_ready(void *data,
                                           struct wp_image_description_v1 *wp_image_description_v1,
                                           uint32_t identity)
{
    image_description_handle_ready2(data, wp_image_description_v1, 0, identity);
}

static const struct wp_image_description_v1_listener image_description_listener = {
    image_description_handle_failed,
    image_description_handle_ready,
    image_description_handle_ready2
};

void Wayland_GetColorInfoForWindow(SDL_WindowData *window_data, bool defer_event_processing)
{
    // Cancel any pending request, as it is out-of-date.
    Wayland_FreeColorInfoState(window_data->color_info_state);
    Wayland_ColorInfoState *state = SDL_calloc(1, sizeof(Wayland_ColorInfoState));

    if (state) {
        window_data->color_info_state = state;
        state->window_data = window_data;
        state->object_type = WAYLAND_COLOR_OBJECT_TYPE_WINDOW;
        state->deferred_event_processing = defer_event_processing;

        if (!defer_event_processing) {
            state->queue = Wayland_DisplayCreateQueue(window_data->waylandData->display, "SDL Color Management Queue");

            struct wl_proxy *surface_feedback_wrapper = WAYLAND_wl_proxy_create_wrapper(window_data->wp_color_management_surface_feedback);
            WAYLAND_wl_proxy_set_queue(surface_feedback_wrapper, state->queue);
            state->wp_image_description = wp_color_management_surface_feedback_v1_get_preferred((struct wp_color_management_surface_feedback_v1 *)surface_feedback_wrapper);
            WAYLAND_wl_proxy_wrapper_destroy(surface_feedback_wrapper);
        } else {
            state->wp_image_description = wp_color_management_surface_feedback_v1_get_preferred(window_data->wp_color_management_surface_feedback);
        }
        wp_image_description_v1_add_listener(state->wp_image_description, &image_description_listener, state);

        if (!defer_event_processing) {
            PumpColorspaceEvents(state);
        }
    }
}

void Wayland_GetColorInfoForOutput(SDL_DisplayData *display_data, bool defer_event_processing)
{
    // Cancel any pending request, as it is out-of-date.
    Wayland_FreeColorInfoState(display_data->color_info_state);
    Wayland_ColorInfoState *state = SDL_calloc(1, sizeof(Wayland_ColorInfoState));

    if (state) {
        display_data->color_info_state = state;
        state->display_data = display_data;
        state->object_type = WAYLAND_COLOR_OBJECT_TYPE_DISPLAY;
        state->deferred_event_processing = defer_event_processing;

        if (!defer_event_processing) {
            state->queue = Wayland_DisplayCreateQueue(display_data->videodata->display, "SDL Color Management Queue");

            struct wl_proxy *output_feedback_wrapper = WAYLAND_wl_proxy_create_wrapper(display_data->wp_color_management_output);
            WAYLAND_wl_proxy_set_queue(output_feedback_wrapper, state->queue);
            state->wp_image_description = wp_color_management_output_v1_get_image_description((struct wp_color_management_output_v1 *)output_feedback_wrapper);
            WAYLAND_wl_proxy_wrapper_destroy(output_feedback_wrapper);
        } else {
            state->wp_image_description = wp_color_management_output_v1_get_image_description(display_data->wp_color_management_output);
        }
        wp_image_description_v1_add_listener(state->wp_image_description, &image_description_listener, state);

        if (!defer_event_processing) {
            PumpColorspaceEvents(state);
        }
    }
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
