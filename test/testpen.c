/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "SDL.h"
#include <math.h>

#include "SDL_test.h"
#include "SDL_test_common.h"

#define WIDTH 1600
#define HEIGHT 1200

#define VERBOSE 0

#define ALWAYS_SHOW_PRESSURE_BOX 1

static SDLTest_CommonState *state;
static int quitting = 0;

static float last_x, last_y;
static float last_xtilt, last_ytilt, last_pressure, last_distance, last_rotation;
static int last_button;
static int last_was_eraser;

static SDL_Texture *offscreen_texture = NULL;

static void
DrawScreen(SDL_Renderer *renderer)
{
    float xdelta, ydelta, endx, endy;
    /* off-screen texture to render into */
    SDL_Texture *window_texture;
    const int X = 128, Y = 128; /* mid-point in the off-screen texture */
    SDL_FRect dest_rect;

    if (!renderer) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xff);
    SDL_RenderClear(renderer);

    if (offscreen_texture == NULL) {
        offscreen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, X * 2, Y * 2);
    }

    /* Render into off-screen texture so we can do pixel-precise rendering later */
    window_texture = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, offscreen_texture);

    /* Rendering starts here */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xff);
    SDL_RenderClear(renderer);

    if (last_was_eraser) {
        SDL_Rect rect;

        rect.x = X - 10;
        rect.y = Y - 10;
        rect.w = 21;
        rect.h = 21;

        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
        SDL_RenderFillRect(renderer, &rect);
    } else {
        int distance = last_distance * 20;

        SDL_SetRenderDrawColor(renderer, 0xff, 0, 0, 0xff);
        SDL_RenderDrawLineF(renderer,
                            X - 10 - distance, Y,
                            X - distance, Y);
        SDL_RenderDrawLineF(renderer,
                            X + 10 + distance, Y,
                            X + distance, Y);
        SDL_RenderDrawLineF(renderer,
                            X, Y - 10 - distance,
                            X, Y - distance);
        SDL_RenderDrawLineF(renderer,
                            X, Y + 10 + distance,
                            X, Y + distance);
    }

    /* Draw a cone based on the direction the pen is leaning as if it were shining a light. */
    /* Colour derived from pens, intensity based on pressure: */
    SDL_SetRenderDrawColor(renderer,
                           (last_button & 0x01)? 0xff : 0,
                           (last_button & 0x02)? 0xff : 0,
                           (last_button & 0x04)? 0xff : 0,
                           (int) (0xff));

    xdelta = -last_xtilt * 100;
    ydelta = -last_ytilt * 100;
    endx = X + xdelta;
    endy = Y + ydelta;
    SDL_RenderDrawLineF(renderer, X, Y, endx, endy);

    SDL_SetRenderDrawColor(renderer,
                           (last_button & 0x01)? 0xff : 0,
                           (last_button & 0x02)? 0xff : 0,
                           (last_button & 0x04)? 0xff : 0,
                           (int) (0xff * last_pressure));
    /* Cone base width based on pressure: */
    SDL_RenderDrawLineF(renderer, X, Y, endx + (ydelta * last_pressure / 3.0), endy - (xdelta * last_pressure / 3.0));
    SDL_RenderDrawLineF(renderer, X, Y, endx - (ydelta * last_pressure / 3.0), endy + (xdelta * last_pressure / 3.0));

    /* If tilt is very small (or zero, for pens that don't have tilt), add some extra lines, rotated by the current rotation value */
    if (ALWAYS_SHOW_PRESSURE_BOX || (fabs(last_xtilt) < 0.1f && fabs(last_ytilt) < 0.1f)) {
        int rot;
        float pressure = last_pressure * 40.0;

        /* Four times, rotated 90 degrees, so that we get a box */
        for (rot = 0; rot < 4; ++rot) {

            float vecx = SDL_cos((last_rotation + (rot/2.0)) * M_PI);
            float vecy = SDL_sin((last_rotation + (rot/2.0)) * M_PI);

            float px = X + vecx * pressure;
            float py = Y + vecy * pressure;

            SDL_RenderDrawLineF(renderer,
                                px + vecy * 10.0f, py - vecx * 10.0f,
                                px - vecy * 10.0f, py + vecx * 10.0f);

            if (rot == 3) {
                SDL_RenderDrawLineF(renderer,
                                    px + vecy * 10.0f, py - vecx * 10.0f,
                                    px + (vecx * pressure * 0.4),
                                    py + (vecy * pressure * 0.4));
                SDL_RenderDrawLineF(renderer,
                                    px - vecy * 10.0f, py + vecx * 10.0f,
                                    px + (vecx * pressure * 0.4),
                                    py + (vecy * pressure * 0.4));
            }
        }
    }

    SDL_SetRenderTarget(renderer, window_texture);
    /* Now render to pixel-precise position */
    dest_rect.x = last_x - X;
    dest_rect.y = last_y - Y;
    dest_rect.w = X * 2;
    dest_rect.h = Y * 2;
    SDL_RenderCopyF(renderer, offscreen_texture, NULL, &dest_rect);
    SDL_RenderPresent(renderer);
}

static void
dump_state(void)
{
    int i;
    SDL_Log("Found %d pens\n", SDL_NumPens());
    for (i = 0; i < SDL_NumPens(); ++i) {
        SDL_PenID penid = SDL_PenIDForIndex(i);
        SDL_GUID guid = SDL_PenGUIDForPenID(penid);
        char guid_str[33];
        float axes[SDL_PEN_NUM_AXES];
        float x, y;
        int k;
        SDL_PenCapabilityInfo info;
        Uint32 status = SDL_PenStatus(penid, &x, &y, axes, SDL_PEN_NUM_AXES);
        Uint32 capabilities = SDL_PenCapabilities(penid, &info);
        char *type;
        char *buttons_str;

        SDL_GUIDToString(guid, guid_str, 33);

        switch (SDL_PenType(penid)) {
        case SDL_PEN_TYPE_ERASER:
                type = "Eraser";
                break;
        case SDL_PEN_TYPE_PEN:
                type = "Pen";
                break;
        case SDL_PEN_TYPE_PENCIL:
                type = "Pencil";
                break;
        case SDL_PEN_TYPE_BRUSH:
                type = "Brush";
                break;
        case SDL_PEN_TYPE_AIRBRUSH:
                type = "Airbrush";
                break;
        default:
                type = "Unknown (bug?)";
        }

        switch (info.num_buttons) {
        case SDL_PEN_INFO_UNKNOWN:
            SDL_asprintf(&buttons_str, "? buttons");
            break;
        case 1:
            SDL_asprintf(&buttons_str, "1 button");
            break;
        default:
            SDL_asprintf(&buttons_str, "%d button", info.num_buttons);
            break;
        }

        SDL_Log("%s %d: [%s] attached=%d, %s [cap= %08x:%08x =status] '%s'\n",
                type,
                penid, guid_str,
                SDL_PenAttached(penid), /* should always be SDL_TRUE during iteration */
                buttons_str,
                capabilities,
                status,
                SDL_PenName(penid)
            );
        SDL_free(buttons_str);
        SDL_Log("   pos=(%.2f, %.2f)", x, y);
        for (k = 0; k < SDL_PEN_NUM_AXES; ++k) {
            SDL_bool supported = capabilities & SDL_PEN_AXIS_CAPABILITY(k);
            if (supported) {
                if (k == SDL_PEN_AXIS_XTILT || k == SDL_PEN_AXIS_YTILT) {
                    if (info.max_tilt == SDL_PEN_INFO_UNKNOWN) {
                        SDL_Log("   axis %d:  %.3f (max tilt unknown)", k, axes[k]);
                    } else {
                        SDL_Log("   axis %d:  %.3f (tilt -%.1f..%.1f)", k, axes[k],
                                info.max_tilt, info.max_tilt);
                    }
                } else {
                    SDL_Log("   axis %d:  %.3f", k, axes[k]);
                }
            } else {
                SDL_Log("   axis %d:  unsupported (%.3f)", k, axes[k]);
            }
        }
    }
}

static void
process_event(SDL_Event event)
{
    SDLTest_CommonEvent(state, &event, &quitting);

    switch (event.type) {
    case SDL_KEYDOWN: {
        dump_state();
        break;
    }
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
#if VERBOSE
    {   int x, y;
        SDL_GetMouseState(&x, &y);
        if (event.type == SDL_MOUSEMOTION) {
            SDL_Log("mouse motion: mouse ID %d is at %d,%d  (state: %d,%d) delta (%d, %d)\n",
                    event.motion.which,
                    event.motion.x, event.motion.y,
                    event.motion.xrel, event.motion.yrel,
                    x, y);
        } else {
            SDL_Log("mouse button: mouse ID %d is at %d,%d  (state: %d,%d)\n",
                    event.button.which,
                    event.button.x, event.button.y,
                    x, y);
        }
    }
#endif
        if (event.motion.which != SDL_PEN_MOUSEID) {
            SDL_ShowCursor(SDL_ENABLE);
        }
        break;

    case SDL_PENMOTION: {
        SDL_PenMotionEvent *ev = &event.pmotion;

        SDL_ShowCursor(SDL_DISABLE);
        last_x = ev->x;
        last_y = ev->y;
        last_xtilt = ev->axes[SDL_PEN_AXIS_XTILT];
        last_ytilt = ev->axes[SDL_PEN_AXIS_YTILT];
        last_pressure = ev->axes[SDL_PEN_AXIS_PRESSURE];
        last_distance = ev->axes[SDL_PEN_AXIS_DISTANCE];
        last_rotation = ev->axes[SDL_PEN_AXIS_ROTATION];
        last_was_eraser = ev->pen_state & SDL_PEN_ERASER_MASK;
#if VERBOSE
        SDL_Log("pen motion @ %d: %s %u at %f,%f; pressure=%.3f, tilt=%.3f/%.3f, dist=%.3f [buttons=%02x]\n",
                ev->timestamp,
                last_was_eraser ? "eraser" : "pen",
                (unsigned int) ev->which, ev->x, ev->y, last_pressure, last_xtilt, last_ytilt, last_distance,
                ev->pen_state);
#endif
        break;
    }
    case SDL_PENBUTTONUP:
    case SDL_PENBUTTONDOWN: {
        SDL_PenButtonEvent *ev = &event.pbutton;

        SDL_ShowCursor(SDL_DISABLE);
        last_x = ev->x;
        last_y = ev->y;
        last_xtilt = ev->axes[SDL_PEN_AXIS_XTILT];
        last_ytilt = ev->axes[SDL_PEN_AXIS_YTILT];
        last_pressure = ev->axes[SDL_PEN_AXIS_PRESSURE];
        last_distance = ev->axes[SDL_PEN_AXIS_DISTANCE];
        last_rotation = ev->axes[SDL_PEN_AXIS_ROTATION];
        last_was_eraser = ev->pen_state & SDL_PEN_ERASER_MASK;
        last_button = ev->pen_state & 0xf; /* button mask */
#if VERBOSE
        SDL_Log("pen button: %s %u at %f,%f; BUTTON %d reported %s with event %s [pressure=%.3f, tilt=%.3f/%.3f, dist=%.3f]\n",
                last_was_eraser ? "eraser" : "pen",
                (unsigned int) ev->which, ev->x, ev->y,
                ev->button,
                (ev->state == SDL_PRESSED) ? "PRESSED"
                : ((ev->state == SDL_RELEASED) ? "RELEASED" : "--invalid--"),
                event.type == SDL_PENBUTTONUP ? "PENBUTTONUP" : "PENBUTTONDOWN",
                last_pressure, last_xtilt, last_ytilt, last_distance);
#endif
        break;
    }

    case SDL_WINDOWEVENT:
        switch (event.window.event) {
        case SDL_WINDOWEVENT_PEN_ENTER:
            SDL_Log("Pen %d entered window %d", event.window.data1, event.window.windowID);
            break;

        case SDL_WINDOWEVENT_PEN_LEAVE:
            SDL_Log("Pen %d left window %d", event.window.data1, event.window.windowID);
            break;

#if VERBOSE
        case SDL_WINDOWEVENT_ENTER:
            SDL_Log("Mouse entered window %d", event.window.windowID);
            break;

        case SDL_WINDOWEVENT_LEAVE:
            SDL_Log("Mouse left window %d", event.window.windowID);
            break;
#endif
        }
        break;

    default:
        break;
    }
}

static void
loop(void)
{
    SDL_Event event;
    int i;

    for (i = 0; i < state->num_windows; ++i) {
        if (state->renderers[i]) {
            DrawScreen(state->renderers[i]);
        }
    }

    if (SDL_WaitEventTimeout(&event, 10)) {
        process_event(event);
    }
    while (SDL_PollEvent(&event)) {
        process_event(event);
    }
}

int main(int argc, char* argv[])
{
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    state->window_title = "Pressure-Sensitive Pen Test";
    state->window_w = WIDTH;
    state->window_h = HEIGHT;
    state->skip_renderer = SDL_FALSE;

    if (!SDLTest_CommonDefaultArgs(state, argc, argv) || !SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return 1;
    }

    while (!quitting) {
        loop();
    }

    SDLTest_CommonQuit(state);
    return 0;
}
