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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "SDL_waylandcolor.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylandwindow.h"
#include "xx-color-management-v4-client-protocol.h"

typedef struct Wayland_ColorInfoState
{
    struct xx_image_description_v4 *xx_image_description;
    struct xx_image_description_info_v4 *xx_image_description_info;
    Wayland_ColorInfo *info;

    bool result;
} Wayland_ColorInfoState;

static void image_description_info_handle_done(void *data,
                                               struct xx_image_description_info_v4 *xx_image_description_info_v4)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;

    if (state->xx_image_description_info) {
        xx_image_description_info_v4_destroy(state->xx_image_description_info);
        state->xx_image_description_info = NULL;
    }
    if (state->xx_image_description) {
        xx_image_description_v4_destroy(state->xx_image_description);
        state->xx_image_description = NULL;
    }

    state->result = true;
}

static void image_description_info_handle_icc_file(void *data,
                                                   struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                   int32_t icc, uint32_t icc_size)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;

    state->info->icc_fd = icc;
    state->info->icc_size = icc_size;
}

static void image_description_info_handle_primaries(void *data,
                                                    struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                    int32_t r_x, int32_t r_y,
                                                    int32_t g_x, int32_t g_y,
                                                    int32_t b_x, int32_t b_y,
                                                    int32_t w_x, int32_t w_y)
{
    // NOP
}

static void image_description_info_handle_primaries_named(void *data,
                                                          struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                          uint32_t primaries)
{
    // NOP
}

static void image_description_info_handle_tf_power(void *data,
                                                   struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                   uint32_t eexp)
{
    // NOP
}

static void image_description_info_handle_tf_named(void *data,
                                                   struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                   uint32_t tf)
{
    // NOP
}

static void image_description_info_handle_luminances(void *data,
                                                     struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                     uint32_t min_lum,
                                                     uint32_t max_lum,
                                                     uint32_t reference_lum)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;
    state->info->HDR.HDR_headroom = (float)max_lum / (float)reference_lum;
}

static void image_description_info_handle_target_primaries(void *data,
                                                           struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                           int32_t r_x, int32_t r_y,
                                                           int32_t g_x, int32_t g_y,
                                                           int32_t b_x, int32_t b_y,
                                                           int32_t w_x, int32_t w_y)
{
    // NOP
}

static void image_description_info_handle_target_luminance(void *data,
                                                           struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                           uint32_t min_lum,
                                                           uint32_t max_lum)
{
    // NOP
}

static void image_description_info_handle_target_max_cll(void *data,
                                                         struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                         uint32_t max_cll)
{
    // NOP
}

static void image_description_info_handle_target_max_fall(void *data,
                                                          struct xx_image_description_info_v4 *xx_image_description_info_v4,
                                                          uint32_t max_fall)
{
    // NOP
}

static const struct xx_image_description_info_v4_listener image_description_info_listener = {
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
    struct wl_event_queue *queue = WAYLAND_wl_display_create_queue(vid->display);
    WAYLAND_wl_proxy_set_queue((struct wl_proxy *)state->xx_image_description, queue);

    while (state->xx_image_description) {
        WAYLAND_wl_display_dispatch_queue(vid->display, queue);
    }

    WAYLAND_wl_event_queue_destroy(queue);
}

static void image_description_handle_failed(void *data,
                                            struct xx_image_description_v4 *xx_image_description_v4,
                                            uint32_t cause,
                                            const char *msg)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;

    xx_image_description_v4_destroy(state->xx_image_description);
    state->xx_image_description = NULL;
}

static void image_description_handle_ready(void *data,
                                           struct xx_image_description_v4 *xx_image_description_v4,
                                           uint32_t identity)
{
    Wayland_ColorInfoState *state = (Wayland_ColorInfoState *)data;

    // This will inherit the queue of the factory image description object.
    state->xx_image_description_info = xx_image_description_v4_get_information(state->xx_image_description);
    xx_image_description_info_v4_add_listener(state->xx_image_description_info, &image_description_info_listener, data);
}

static const struct xx_image_description_v4_listener image_description_listener = {
    image_description_handle_failed,
    image_description_handle_ready
};

bool Wayland_GetColorInfoForWindow(SDL_WindowData *window_data, Wayland_ColorInfo *info)
{
    Wayland_ColorInfoState state;
    SDL_zero(state);
    state.info = info;

    state.xx_image_description = xx_color_management_feedback_surface_v4_get_preferred(window_data->xx_color_management_feedback_surface);
    xx_image_description_v4_add_listener(state.xx_image_description, &image_description_listener, &state);

    PumpColorspaceEvents(&state);

    return state.result;
}

bool Wayland_GetColorInfoForOutput(SDL_DisplayData *display_data, Wayland_ColorInfo *info)
{
    Wayland_ColorInfoState state;
    SDL_zero(state);
    state.info = info;

    state.xx_image_description = xx_color_management_output_v4_get_image_description(display_data->xx_color_management_output);
    xx_image_description_v4_add_listener(state.xx_image_description, &image_description_listener, &state);

    PumpColorspaceEvents(&state);

    return state.result;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
