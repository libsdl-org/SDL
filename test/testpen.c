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

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#define WIDTH  1600
#define HEIGHT 1200

#define VERBOSE 0

#define ALWAYS_SHOW_PRESSURE_BOX 1

static SDLTest_CommonState *state;
static int quitting = 0;

static float last_x, last_y;
static float last_xtilt, last_ytilt, last_pressure, last_distance, last_rotation;
static int last_button;
static int last_touching; /* tip touches surface */
static int last_was_eraser;

static SDL_Texture *offscreen_texture = NULL;

static void DrawScreen(SDL_Renderer *renderer)
{
    float xdelta, ydelta, endx, endy;
    /* off-screen texture to render into */
    SDL_Texture *window_texture;
    const float X = 128.0f, Y = 128.0f; /* mid-point in the off-screen texture */
    SDL_FRect dest_rect;
    float tilt_vec_x = SDL_sinf(last_xtilt * SDL_PI_F / 180.0f);
    float tilt_vec_y = SDL_sinf(last_ytilt * SDL_PI_F / 180.0f);
    int color = last_button + 1;

    if (!renderer) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xff);
    SDL_RenderClear(renderer);

    if (offscreen_texture == NULL) {
        offscreen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, (int)(X * 2.0f), (int)(Y * 2.0f));
    }

    /* Render into off-screen texture so we can do pixel-precise rendering later */
    window_texture = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, offscreen_texture);

    /* Rendering starts here */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xff);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 0xa0, 0xa0, 0xa0, 0xff);
    if (last_touching) {
	SDL_FRect rect;

        rect.x = 0;
        rect.y = 0;
        rect.w = 2.0f * X - 1.0f;
        rect.h = 2.0f * Y - 1.0f;

	SDL_RenderRect(renderer, &rect);
    } else {
	/* Show where the pen is rotating when it isn't touching the surface.
	   Otherwise we draw the rotation angle below together with pressure information. */
	float rot_vecx =  SDL_sinf(last_rotation / 180.0f * SDL_PI_F);
	float rot_vecy = -SDL_cosf(last_rotation / 180.0f * SDL_PI_F);
	float px = X + rot_vecx * 100.0f;
	float py = Y + rot_vecy * 100.0f;
	float px2 = X + rot_vecx * 80.0f;
	float py2 = Y + rot_vecy * 80.0f;

	SDL_RenderLine(renderer,
		       px, py,
		       px2 + rot_vecy * 20.0f,
		       py2 - rot_vecx * 20.0f);
	SDL_RenderLine(renderer,
		       px, py,
		       px2 - rot_vecy * 20.0f,
		       py2 + rot_vecx * 20.0f);
    }

    if (last_was_eraser) {
        SDL_FRect rect;

        rect.x = X - 10.0f;
        rect.y = Y - 10.0f;
        rect.w = 21.0f;
        rect.h = 21.0f;

        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
        SDL_RenderFillRect(renderer, &rect);
    } else {
        float distance = last_distance * 50.0f;

        SDL_SetRenderDrawColor(renderer, 0xff, 0, 0, 0xff);
        SDL_RenderLine(renderer,
                       X - 10.0f - distance, Y,
                       X - distance, Y);
        SDL_RenderLine(renderer,
                       X + 10.0f + distance, Y,
                       X + distance, Y);
        SDL_RenderLine(renderer,
                       X, Y - 10.0f - distance,
                       X, Y - distance);
        SDL_RenderLine(renderer,
                       X, Y + 10.0f + distance,
                       X, Y + distance);

    }

    /* Draw a cone based on the direction the pen is leaning as if it were shining a light. */
    /* Colour derived from pens, intensity based on pressure: */
    SDL_SetRenderDrawColor(renderer,
                           (color & 0x01) ? 0xff : 0,
                           (color & 0x02) ? 0xff : 0,
                           (color & 0x04) ? 0xff : 0,
                           (int)(0xff));

    xdelta = -tilt_vec_x * 100.0f;
    ydelta = -tilt_vec_y * 100.0f;
    endx = X + xdelta;
    endy = Y + ydelta;
    SDL_RenderLine(renderer, X, Y, endx, endy);

    SDL_SetRenderDrawColor(renderer,
                           (color & 0x01) ? 0xff : 0,
                           (color & 0x02) ? 0xff : 0,
                           (color & 0x04) ? 0xff : 0,
                           (Uint8)(0xff * last_pressure));
    /* Cone base width based on pressure: */
    SDL_RenderLine(renderer, X, Y, endx + (ydelta * last_pressure / 3.0f), endy - (xdelta * last_pressure / 3.0f));
    SDL_RenderLine(renderer, X, Y, endx - (ydelta * last_pressure / 3.0f), endy + (xdelta * last_pressure / 3.0f));

    /* If tilt is very small (or zero, for pens that don't have tilt), add some extra lines, rotated by the current rotation value */
    if (ALWAYS_SHOW_PRESSURE_BOX || (SDL_fabsf(tilt_vec_x) < 0.2f && SDL_fabsf(tilt_vec_y) < 0.2f)) {
        int rot;
        float pressure = last_pressure * 80.0f;

        /* Four times, rotated 90 degrees, so that we get a box */
        for (rot = 0; rot < 4; ++rot) {

            float vecx = SDL_cosf((last_rotation + (rot * 90.0f)) / 180.0f * SDL_PI_F);
            float vecy = SDL_sinf((last_rotation + (rot * 90.0f)) / 180.0f * SDL_PI_F);

            float px = X + vecx * pressure;
            float py = Y + vecy * pressure;

            SDL_RenderLine(renderer,
                                px + vecy * 10.0f, py - vecx * 10.0f,
                                px - vecy * 10.0f, py + vecx * 10.0f);

            if (rot == 3) {
                int r = 0;
                for (; r >= 0; r -= 2) {
                    float delta = 10.0f - ((float) r);

                    SDL_RenderLine(renderer,
                                   px + vecy * delta, py - vecx * delta,
                                   px + (vecx * pressure * 0.4f),
                                   py + (vecy * pressure * 0.4f));
                    SDL_RenderLine(renderer,
                                   px - vecy * delta, py + vecx * delta,
                                   px + (vecx * pressure * 0.4f),
                                   py + (vecy * pressure * 0.4f));
                }
            }
        }
    }

    SDL_SetRenderTarget(renderer, window_texture);
    /* Now render to pixel-precise position */
    dest_rect.x = last_x - X;
    dest_rect.y = last_y - Y;
    dest_rect.w = X * 2.0f;
    dest_rect.h = Y * 2.0f;
    SDL_RenderTexture(renderer, offscreen_texture, NULL, &dest_rect);
    SDL_RenderPresent(renderer);
}

static void dump_state(void)
{
    int i;
    int pens_nr;

    /* Make sure this also works with a NULL parameter */
    SDL_PenID* pens = SDL_GetPens(NULL);
    if (pens) {
        SDL_free(pens);
    }

    pens = SDL_GetPens(&pens_nr);
    if (!pens) {
        SDL_Log("Couldn't get pens: %s\n", SDL_GetError());
        return;
    }
    SDL_Log("Found %d pens (terminated by %u)\n", pens_nr, (unsigned) pens[pens_nr]);

    for (i = 0; i < pens_nr; ++i) {
        SDL_PenID penid = pens[i];
        SDL_GUID guid = SDL_GetPenGUID(penid);
        char guid_str[33];
        float axes[SDL_PEN_NUM_AXES];
        float x, y;
        int k;
        SDL_PenCapabilityInfo info;
        Uint32 status = SDL_GetPenStatus(penid, &x, &y, axes, SDL_PEN_NUM_AXES);
        const SDL_PenCapabilityFlags capabilities = SDL_GetPenCapabilities(penid, &info);
        char *type;
        char *buttons_str;

        SDL_GUIDToString(guid, guid_str, 33);

        switch (SDL_GetPenType(penid)) {
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

        SDL_Log("%s %lu: [%s] attached=%d, %s [cap= %08lx:%08lx =status] '%s'\n",
                type,
                (unsigned long) penid, guid_str,
                SDL_PenConnected(penid), /* should always be SDL_TRUE during iteration */
                buttons_str,
                (unsigned long) capabilities,
                (unsigned long) status,
                SDL_GetPenName(penid));
        SDL_free(buttons_str);
        SDL_Log("   pos=(%.2f, %.2f)", x, y);
        for (k = 0; k < SDL_PEN_NUM_AXES; ++k) {
            SDL_bool supported = ((capabilities & SDL_PEN_AXIS_CAPABILITY(k)) != 0);
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
    SDL_free(pens);
}

static void update_axes(float *axes)
{
    last_xtilt = axes[SDL_PEN_AXIS_XTILT];
    last_ytilt = axes[SDL_PEN_AXIS_YTILT];
    last_pressure = axes[SDL_PEN_AXIS_PRESSURE];
    last_distance = axes[SDL_PEN_AXIS_DISTANCE];
    last_rotation = axes[SDL_PEN_AXIS_ROTATION];
}

static void update_axes_from_touch(const float pressure)
{
    last_xtilt = 0;
    last_ytilt = 0;
    last_pressure = pressure;
    last_distance = 0;
    last_rotation = 0;
}

static void process_event(SDL_Event event)
{
    SDLTest_CommonEvent(state, &event, &quitting);

    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    {
        dump_state();
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
#if VERBOSE
    {
        float x, y;
        SDL_GetMouseState(&x, &y);
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            SDL_Log("[%lu] mouse motion: mouse ID %d is at (%.2f, %.2f)  (state: %.2f,%.2f) delta (%.2f, %.2f)\n",
                    event.motion.timestamp,
                    event.motion.which,
                    event.motion.x, event.motion.y,
                    event.motion.xrel, event.motion.yrel,
                    x, y);
        } else {
            SDL_Log("[%lu] mouse button: mouse ID %d is at (%.2f, %.2f)  (state: %.2f,%.2f)\n",
                    event.button.timestamp,
                    event.button.which,
                    event.button.x, event.button.y,
                    x, y);
        }
    }
#endif
    if (event.motion.which != SDL_PEN_MOUSEID && event.motion.which != SDL_TOUCH_MOUSEID) {
        SDL_ShowCursor();
    } break;

    case SDL_EVENT_PEN_MOTION:
    {
        SDL_PenMotionEvent *ev = &event.pmotion;

        SDL_HideCursor();
        last_x = ev->x;
        last_y = ev->y;
	update_axes(ev->axes);
        last_was_eraser = ev->pen_state & SDL_PEN_ERASER_MASK;
#if VERBOSE
        SDL_Log("[%lu] pen motion: %s %u at (%.4f, %.4f); pressure=%.3f, tilt=%.3f/%.3f, dist=%.3f [buttons=%02x]\n",
                (unsigned long) ev->timestamp,
                last_was_eraser ? "eraser" : "pen",
                (unsigned int)ev->which, ev->x, ev->y, last_pressure, last_xtilt, last_ytilt, last_distance,
                ev->pen_state);
#endif
    } break;

    case SDL_EVENT_PEN_UP:
    case SDL_EVENT_PEN_DOWN: {
        SDL_PenTipEvent *ev = &event.ptip;
        last_x = ev->x;
        last_y = ev->y;
	update_axes(ev->axes);
        last_was_eraser = ev->tip == SDL_PEN_TIP_ERASER;
        last_button = ev->pen_state & 0xf; /* button mask */
        last_touching = (event.type == SDL_EVENT_PEN_DOWN);
    } break;

    case SDL_EVENT_PEN_BUTTON_UP:
    case SDL_EVENT_PEN_BUTTON_DOWN:
    {
        SDL_PenButtonEvent *ev = &event.pbutton;

        SDL_HideCursor();
        last_x = ev->x;
        last_y = ev->y;
	update_axes(ev->axes);
        if (last_pressure > 0.0f && !last_touching) {
            SDL_LogWarn(SDL_LOG_CATEGORY_TEST,
                        "[%lu] : reported pressure %.5f even though pen is not touching surface",
                        (unsigned long) ev->timestamp, last_pressure);

        }
        last_was_eraser = ev->pen_state & SDL_PEN_ERASER_MASK;
        last_button = ev->pen_state & 0xf; /* button mask */
        if ((ev->pen_state & SDL_PEN_DOWN_MASK) &&  !last_touching) {
            SDL_LogWarn(SDL_LOG_CATEGORY_TEST,
                        "[%lu] : reported flags %x (SDL_PEN_FLAG_DOWN_MASK) despite not receiving SDL_EVENT_PEN_DOWN",
                        (unsigned long) ev->timestamp, ev->pen_state);

        }
        if (!(ev->pen_state & SDL_PEN_DOWN_MASK) &&  last_touching) {
            SDL_LogWarn(SDL_LOG_CATEGORY_TEST,
                        "[%lu] : reported flags %x (no SDL_PEN_FLAG_DOWN_MASK) despite receiving SDL_EVENT_PEN_DOWN without SDL_EVENT_PEN_UP afterwards",
                        (unsigned long) ev->timestamp, ev->pen_state);

        }
#if VERBOSE
        SDL_Log("[%lu] pen button: %s %u at (%.4f, %.4f); BUTTON %d reported %s with event %s [pressure=%.3f, tilt=%.3f/%.3f, dist=%.3f]\n",
                (unsigned long) ev->timestamp,
                last_was_eraser ? "eraser" : "pen",
                (unsigned int)ev->which, ev->x, ev->y,
                ev->button,
                (ev->state == SDL_PRESSED) ? "PRESSED"
                                           : ((ev->state == SDL_RELEASED) ? "RELEASED" : "--invalid--"),
                event.type == SDL_EVENT_PEN_BUTTON_UP ? "PENBUTTONUP" : "PENBUTTONDOWN",
                last_pressure, last_xtilt, last_ytilt, last_distance);
#endif
    } break;

    case SDL_EVENT_WINDOW_PEN_ENTER:
        SDL_Log("[%lu] Pen %lu entered window %lx",
                (unsigned long) event.window.timestamp,
                (unsigned long) event.window.data1,
                (unsigned long) event.window.windowID);
        break;

    case SDL_EVENT_WINDOW_PEN_LEAVE:
        SDL_Log("[%lu] Pen %lu left window %lx",
                (unsigned long) event.window.timestamp,
                (unsigned long) event.window.data1,
                (unsigned long) event.window.windowID);
        break;

#if VERBOSE
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        SDL_Log("[%lu] Mouse entered window %lx",
                (unsigned long) event.window.timestamp,
                (unsigned long) event.window.windowID);
        break;

    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        SDL_Log("[%lu] Mouse left window %lx",
                (unsigned long) event.window.timestamp,
                (unsigned long) event.window.windowID);
        break;
#endif

    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    {
        SDL_TouchFingerEvent *ev = &event.tfinger;
        int w, h;
        SDL_HideCursor();
        SDL_GetWindowSize(SDL_GetWindowFromID(ev->windowID), &w, &h);
        last_x = ev->x * w;
        last_y = ev->y * h;
        update_axes_from_touch(ev->pressure);
        last_was_eraser = SDL_FALSE;
        last_button = 0;
        last_touching = (ev->type != SDL_EVENT_FINGER_UP);
#if VERBOSE
        SDL_Log("[%lu] finger %s: %s (touchId: %" SDL_PRIu64 ", fingerId: %" SDL_PRIu64 ") at (%.4f, %.4f); pressure=%.3f\n",
                (unsigned long) ev->timestamp,
                ev->type == SDL_EVENT_FINGER_DOWN ? "down" : (ev->type == SDL_EVENT_FINGER_MOTION ? "motion" : "up"),
                SDL_GetTouchDeviceName(ev->touchId),
                ev->touchId,
                ev->fingerId,
                last_x, last_y, last_pressure);
#endif
    } break;

    default:
        break;
    }
}

static void loop(void)
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

int main(int argc, char *argv[])
{
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

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
