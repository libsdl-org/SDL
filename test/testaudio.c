/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

#define POOF_LIFETIME 250
#define VISUALIZER_WIDTH 100
#define VISUALIZER_HEIGHT 50

typedef struct Texture
{
    SDL_Texture *texture;
    float w;
    float h;
} Texture;

typedef enum ThingType
{
    THING_NULL,
    THING_PHYSDEV,
    THING_PHYSDEV_RECORDING,
    THING_LOGDEV,
    THING_LOGDEV_RECORDING,
    THING_TRASHCAN,
    THING_STREAM,
    THING_POOF,
    THING_WAV
} ThingType;


typedef struct Thing Thing;

struct Thing
{
    ThingType what;

    union {
        struct {
            SDL_AudioDeviceID devid;
            SDL_bool recording;
            SDL_AudioSpec spec;
            char *name;
        } physdev;
        struct {
            SDL_AudioDeviceID devid;
            SDL_bool recording;
            SDL_AudioSpec spec;
            Thing *physdev;
            SDL_bool visualizer_enabled;
            SDL_bool visualizer_updated;
            SDL_Texture *visualizer;
            SDL_Mutex *postmix_lock;
            float *postmix_buffer;
            int postmix_buflen;
            int postmix_allocated;
            SDL_AudioSpec postmix_spec;
            SDL_AtomicInt postmix_updated;
        } logdev;
        struct {
            SDL_AudioSpec spec;
            Uint8 *buf;
            Uint32 buflen;
        } wav;
        struct {
            float startw;
            float starth;
            float centerx;
            float centery;
        } poof;
        struct {
            SDL_AudioStream *stream;
            int total_bytes;
            Uint64 next_level_update;
            Uint8 levels[5];
        } stream;
    } data;

    Thing *line_connected_to;
    char *titlebar;
    SDL_FRect rect;
    float z;
    Uint8 r, g, b, a;
    float progress;
    float meter;
    float scale;
    Uint64 createticks;
    Texture *texture;
    const ThingType *can_be_dropped_onto;

    void (*ontick)(Thing *thing, Uint64 now);
    void (*ondrag)(Thing *thing, int button, float x, float y);
    void (*ondrop)(Thing *thing, int button, float x, float y);
    void (*ondraw)(Thing *thing, SDL_Renderer *renderer);
    void (*onmousewheel)(Thing *thing, float y);

    Thing *prev;
    Thing *next;
};


static Uint64 app_ready_ticks = 0;
static SDLTest_CommonState *state = NULL;

static Thing *things = NULL;
static char *current_titlebar = NULL;

static Thing *mouseover_thing = NULL;
static Thing *droppable_highlighted_thing = NULL;
static Thing *dragging_thing = NULL;
static int dragging_button = -1;
static int dragging_button_real = -1;
static SDL_bool ctrl_held = SDL_FALSE;
static SDL_bool alt_held = SDL_FALSE;

static Texture *physdev_texture = NULL;
static Texture *logdev_texture = NULL;
static Texture *audio_texture = NULL;
static Texture *trashcan_texture = NULL;
static Texture *soundboard_texture = NULL;
static Texture *soundboard_levels_texture = NULL;


static void SetTitleBar(const char *fmt, ...)
{
    char *newstr = NULL;
    va_list ap;
    va_start(ap, fmt);
    SDL_vasprintf(&newstr, fmt, ap);
    va_end(ap);

    if (newstr && (!current_titlebar || (SDL_strcmp(current_titlebar, newstr) != 0))) {
        SDL_SetWindowTitle(state->windows[0], newstr);
        SDL_free(current_titlebar);
        current_titlebar = newstr;
    } else {
        SDL_free(newstr);
    }
}

static void SetDefaultTitleBar(void)
{
    SetTitleBar("testaudio: %s", SDL_GetCurrentAudioDriver());
}

static Thing *FindThingAtPoint(const float x, const float y)
{
    const SDL_FPoint pt = { x, y };
    Thing *retval = NULL;
    Thing *i;
    for (i = things; i; i = i->next) {
        if ((i != dragging_thing) && SDL_PointInRectFloat(&pt, &i->rect)) {
            retval = i;  /* keep going, though, because things drawn on top are later in the list. */
        }
    }
    return retval;
}

static Thing *UpdateMouseOver(const float x, const float y)
{
    Thing *thing;

    if (dragging_thing) {
        thing = dragging_thing;
    } else {
        thing = FindThingAtPoint(x, y);
    }

    mouseover_thing = thing;

    if (!thing) {
        SetDefaultTitleBar();
    } else if (thing->titlebar) {
        SetTitleBar("%s", thing->titlebar);
    }

    return thing;
}

static Thing *CreateThing(ThingType what, float x, float y, float z, float w, float h, Texture *texture, const char *titlebar)
{
    Thing *last = NULL;
    Thing *i;
    Thing *thing;

    thing = (Thing *) SDL_calloc(1, sizeof (Thing));
    if (!thing) {
        SDL_Log("Out of memory!");
        return NULL;
    }

    if ((w < 0) || (h < 0)) {
        SDL_assert(texture != NULL);
        if (w < 0) {
            w = texture->w;
        }
        if (h < 0) {
            h = texture->h;
        }
    }

    thing->what = what;
    thing->rect.x = x;
    thing->rect.y = y;
    thing->rect.w = w;
    thing->rect.h = h;
    thing->z = z;
    thing->r = 255;
    thing->g = 255;
    thing->b = 255;
    thing->a = 255;
    thing->scale = 1.0f;
    thing->createticks = SDL_GetTicks();
    thing->texture = texture;
    thing->titlebar = titlebar ? SDL_strdup(titlebar) : NULL;  /* if allocation fails, oh well. */

    /* insert in list by Z order (furthest from the "camera" first, so they get drawn over; negative Z is not drawn at all). */
    if (!things) {
        things = thing;
        return thing;
    }

    for (i = things; i; i = i->next) {
        if (z > i->z) {  /* insert here. */
            thing->next = i;
            thing->prev = i->prev;

            SDL_assert(i->prev == last);

            if (i->prev) {
                i->prev->next = thing;
            } else {
                SDL_assert(i == things);
                things = thing;
            }
            i->prev = thing;
            return thing;
        }
        last = i;
    }

    if (last) {
        last->next = thing;
        thing->prev = last;
    }
    return thing;
}

static void DestroyThing(Thing *thing)
{
    if (!thing) {
        return;
    }

    if (mouseover_thing == thing) {
        mouseover_thing = NULL;
    }

    if (droppable_highlighted_thing == thing) {
        droppable_highlighted_thing = NULL;
    }

    if (dragging_thing == thing) {
        dragging_thing = NULL;
    }

    switch (thing->what) {
        case THING_POOF: break;
        case THING_NULL: break;
        case THING_TRASHCAN: break;

        case THING_LOGDEV:
        case THING_LOGDEV_RECORDING:
            SDL_CloseAudioDevice(thing->data.logdev.devid);
            if (state->renderers[0] != NULL) {
                SDL_DestroyTexture(thing->data.logdev.visualizer);
            }
            SDL_DestroyMutex(thing->data.logdev.postmix_lock);
            SDL_free(thing->data.logdev.postmix_buffer);
            break;

        case THING_PHYSDEV:
        case THING_PHYSDEV_RECORDING:
            SDL_free(thing->data.physdev.name);
            break;

        case THING_WAV:
            SDL_free(thing->data.wav.buf);
            break;

        case THING_STREAM:
            SDL_DestroyAudioStream(thing->data.stream.stream);
            break;
    }

    if (thing->prev) {
        thing->prev->next = thing->next;
    } else {
        SDL_assert(thing == things);
        things = thing->next;
    }

    if (thing->next) {
        thing->next->prev = thing->prev;
    }

    SDL_free(thing->titlebar);
    SDL_free(thing);
}

static void DrawOneThing(SDL_Renderer *renderer, Thing *thing)
{
    SDL_FRect dst;
    SDL_memcpy(&dst, &thing->rect, sizeof (SDL_FRect));
    if (thing->scale != 1.0f) {
        const float centerx = thing->rect.x + (thing->rect.w / 2);
        const float centery = thing->rect.y + (thing->rect.h / 2);
        const int w = thing->texture ? (int) thing->texture->w : 128;
        const int h = thing->texture ? (int) thing->texture->h : 128;
        dst.w = w * thing->scale;
        dst.h = h * thing->scale;
        dst.x = centerx - (dst.w / 2);
        dst.y = centery - (dst.h / 2);
    }

    if (thing->texture) {
        if (droppable_highlighted_thing == thing) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 255, 100);
            SDL_RenderFillRect(renderer, &dst);
        }
        SDL_SetRenderDrawColor(renderer, thing->r, thing->g, thing->b, thing->a);
        SDL_RenderTexture(renderer, thing->texture->texture, NULL, &dst);
    } else {
        SDL_SetRenderDrawColor(renderer, thing->r, thing->g, thing->b, thing->a);
        SDL_RenderFillRect(renderer, &dst);
    }

    if (thing->ondraw) {
        thing->ondraw(thing, renderer);
    }

    if (thing->progress > 0.0f) {
        SDL_FRect r = { thing->rect.x, thing->rect.y + (thing->rect.h + 2.0f), 0.0f, 10.0f };
        r.w = thing->rect.w * ((thing->progress > 1.0f) ? 1.0f : thing->progress);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 128);
        SDL_RenderFillRect(renderer, &r);
    }

    if (thing->meter > 0.0f) {
        SDL_FRect r;
        r.w = 10.0f;
        r.h = thing->rect.h * ((thing->meter > 1.0f) ? 1.0f : thing->meter);
        r.x = thing->rect.x + thing->rect.w + 2.0f;
        r.y = (thing->rect.y + thing->rect.h) - r.h;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 128);
        SDL_RenderFillRect(renderer, &r);
    }
}

static void DrawThings(SDL_Renderer *renderer)
{
    Thing *i;

    /* draw connecting lines first, so they're behind everything else. */
    for (i = things; i && (i->z >= 0.0f); i = i->next) {
        Thing *dst = i->line_connected_to;
        if (dst) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderLine(renderer, i->rect.x + (i->rect.w / 2), i->rect.y + (i->rect.h / 2), dst->rect.x + (dst->rect.w / 2), dst->rect.y + (dst->rect.h / 2));
        }
    }

    /* Draw the actual things. */
    for (i = things; i && (i->z >= 0.0f); i = i->next) {
        if (i != dragging_thing) {
            DrawOneThing(renderer, i);
        }
    }

    if (dragging_thing) {
        DrawOneThing(renderer, dragging_thing);  /* draw last so it's always on top. */
    }
}

static void Draw(void)
{
    SDL_Renderer *renderer = state->renderers[0];
    if (renderer) {  /* might be NULL if we're shutting down. */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 64, 0, 64, 255);
        SDL_RenderClear(renderer);
        DrawThings(renderer);
        SDL_RenderPresent(renderer);
    }
}

static void RepositionRowOfThings(const ThingType what, const float y)
{
    int total_things = 0;
    float texw = 0.0f;
    float texh = 0.0f;
    Thing *i;

    for (i = things; i; i = i->next) {
        if (i->what == what) {
            texw = i->rect.w;
            texh = i->rect.h;
            total_things++;
        }
    }

    if (total_things > 0) {
        int w, h;
        SDL_GetWindowSize(state->windows[0], &w, &h);
        const float spacing = w / ((float) total_things);
        float x = (spacing - texw) / 2.0f;
        for (i = things; i; i = i->next) {
            if (i->what == what) {
                i->rect.x = x;
                i->rect.y = (y >= 0.0f) ? y : ((h + y) - texh);
                x += spacing;
            }
        }
    }
}

static const char *AudioFmtToString(const SDL_AudioFormat fmt)
{
    switch (fmt) {
        #define FMTCASE(x) case SDL_AUDIO_##x: return #x
        FMTCASE(U8);
        FMTCASE(S8);
        FMTCASE(S16LE);
        FMTCASE(S16BE);
        FMTCASE(S32LE);
        FMTCASE(S32BE);
        FMTCASE(F32LE);
        FMTCASE(F32BE);
        #undef FMTCASE
    }
    return "?";
}

static const char *AudioChansToStr(const int channels)
{
    switch (channels) {
        case 1: return "mono";
        case 2: return "stereo";
        case 3: return "2.1";
        case 4: return "quad";
        case 5: return "4.1";
        case 6: return "5.1";
        case 7: return "6.1";
        case 8: return "7.1";
        default: break;
    }
    return "?";
}

static void PoofThing_ondrag(Thing *thing, int button, float x, float y)
{
    dragging_thing = NULL;   /* refuse to be dragged. */
}

static void PoofThing_ontick(Thing *thing, Uint64 now)
{
    const int lifetime = POOF_LIFETIME;
    const int elasped = (int) (now - thing->createticks);
    if (elasped > lifetime) {
        DestroyThing(thing);
    } else {
        const float pct = ((float) elasped) / ((float) lifetime);
        thing->a = (Uint8) (int) (255.0f - (pct * 255.0f));
        thing->scale = 1.0f - pct;  /* shrink to nothing! */
    }
}

static Thing *CreatePoofThing(Thing *poofing_thing)
{
    const float centerx = poofing_thing->rect.x + (poofing_thing->rect.w / 2);
    const float centery = poofing_thing->rect.y + (poofing_thing->rect.h / 2);
    const float z = poofing_thing->z;
    Thing *thing = CreateThing(THING_POOF, poofing_thing->rect.x, poofing_thing->rect.y, z, poofing_thing->rect.w, poofing_thing->rect.h, poofing_thing->texture, NULL);
    if (thing) {
        thing->data.poof.startw = poofing_thing->rect.w;
        thing->data.poof.starth = poofing_thing->rect.h;
        thing->data.poof.centerx = centerx;
        thing->data.poof.centery = centery;
        thing->ontick = PoofThing_ontick;
        thing->ondrag = PoofThing_ondrag;
    }
    return thing;
}

static void DestroyThingInPoof(Thing *thing)
{
    if (thing) {
        if (thing->what != THING_POOF) {
            CreatePoofThing(thing);
        }
        DestroyThing(thing);
    }
}

/* this poofs a thing and additionally poofs all things connected to the thing. */
static void TrashThing(Thing *thing)
{
    Thing *i, *next;
    for (i = things; i; i = next) {
        next = i->next;
        if (i->line_connected_to == thing) {
            TrashThing(i);
            next = things;  /* start over in case this blew up the list. */
        }
    }
    DestroyThingInPoof(thing);
}

static void StreamThing_ontick(Thing *thing, Uint64 now)
{
    if (!thing->line_connected_to) {
        return;
    }

    /* are we playing? See if we're done, or update state. */
    if (thing->line_connected_to->what == THING_LOGDEV) {
        const int available = SDL_GetAudioStreamAvailable(thing->data.stream.stream);
        if (!available) {
            DestroyThingInPoof(thing);
            return;
        } else {
            thing->progress = 1.0f - (thing->data.stream.total_bytes ? (((float) (available)) / ((float) thing->data.stream.total_bytes)) : 0.0f);
        }
    }

    if (thing->data.stream.next_level_update <= now) {
        Uint64 perf = SDL_GetPerformanceCounter();
        int i;
        for (i = 0; i < SDL_arraysize(thing->data.stream.levels); i++) {
            thing->data.stream.levels[i] = (Uint8) (perf % 6);
            perf >>= 3;
        }
        thing->data.stream.next_level_update += 150;
    }
}

static void StreamThing_ondrag(Thing *thing, int button, float x, float y)
{
    if (button == SDL_BUTTON_RIGHT) {  /* this is kinda hacky, but use this to disconnect from a playing source. */
        if (thing->line_connected_to) {
            SDL_UnbindAudioStream(thing->data.stream.stream); /* unbind from current device */
            if (thing->line_connected_to->what == THING_LOGDEV_RECORDING) {
                SDL_FlushAudioStream(thing->data.stream.stream);
            }
            thing->line_connected_to = NULL;
        }
    }
}

static void StreamThing_ondrop(Thing *thing, int button, float x, float y)
{
    if (droppable_highlighted_thing) {
        if (droppable_highlighted_thing->what == THING_TRASHCAN) {
            TrashThing(thing);
        } else if (((droppable_highlighted_thing->what == THING_LOGDEV) || (droppable_highlighted_thing->what == THING_LOGDEV_RECORDING)) && (droppable_highlighted_thing != thing->line_connected_to)) {
            /* connect to a logical device! */
            SDL_Log("Binding audio stream ('%s') to logical device %u", thing->titlebar, (unsigned int) droppable_highlighted_thing->data.logdev.devid);
            if (thing->line_connected_to) {
                SDL_UnbindAudioStream(thing->data.stream.stream); /* unbind from current device */
                if (thing->line_connected_to->what == THING_LOGDEV_RECORDING) {
                    SDL_FlushAudioStream(thing->data.stream.stream);
                }
            }

            SDL_BindAudioStream(droppable_highlighted_thing->data.logdev.devid, thing->data.stream.stream); /* bind to new device! */
            thing->data.stream.total_bytes = SDL_GetAudioStreamAvailable(thing->data.stream.stream);
            thing->progress = 0.0f;  /* ontick will adjust this if we're on a playback device.*/
            thing->data.stream.next_level_update = SDL_GetTicks() + 100;
            thing->line_connected_to = droppable_highlighted_thing;
        }
    }
}

static void StreamThing_onmousewheel(Thing *thing, float y)
{
    thing->meter += y * 0.01f;
    thing->meter = SDL_clamp(thing->meter, 0.0f, 1.0f);
    SDL_SetAudioStreamGain(thing->data.stream.stream, thing->meter);
}

static void StreamThing_ondraw(Thing *thing, SDL_Renderer *renderer)
{
    if (thing->line_connected_to) {  /* are we playing? Update progress bar, and bounce the levels a little. */
        static const float xlocs[5] = { 18, 39, 59, 79, 99 };
        static const float ylocs[5] = { 49, 39, 29, 19, 10 };
        const float blockw = soundboard_levels_texture->w;
        const float blockh = soundboard_levels_texture->h / 5.0f;
        int i, j;
        SDL_SetRenderDrawColor(renderer, thing->r, thing->g, thing->b, thing->a);
        for (i = 0; i < SDL_arraysize(thing->data.stream.levels); i++) {
            const int level = (int) thing->data.stream.levels[i];
            const float x = xlocs[i];
            for (j = 0; j < level; j++) {
                const SDL_FRect src = { 0, soundboard_levels_texture->h - ((j+1) * blockh), blockw, blockh };
                const SDL_FRect dst = { thing->rect.x + x, thing->rect.y + ylocs[j], blockw, blockh };
                SDL_RenderTexture(renderer, soundboard_levels_texture->texture, &src, &dst);
            }
        }
    }
}

static Thing *CreateStreamThing(const SDL_AudioSpec *spec, const Uint8 *buf, const Uint32 buflen, const char *fname, const float x, const float y)
{
    static const ThingType can_be_dropped_onto[] = { THING_TRASHCAN, THING_LOGDEV, THING_LOGDEV_RECORDING, THING_NULL };
    Thing *thing = CreateThing(THING_STREAM, x, y, 0, -1, -1, soundboard_texture, fname);
    if (thing) {
        SDL_Log("Adding audio stream for %s", fname ? fname : "(null)");
        thing->data.stream.stream = SDL_CreateAudioStream(spec, spec);
        if (buf && buflen) {
            SDL_PutAudioStreamData(thing->data.stream.stream, buf, (int) buflen);
            SDL_FlushAudioStream(thing->data.stream.stream);
            thing->data.stream.total_bytes = SDL_GetAudioStreamAvailable(thing->data.stream.stream);
        }
        thing->ontick = StreamThing_ontick;
        thing->ondrag = StreamThing_ondrag;
        thing->ondrop = StreamThing_ondrop;
        thing->ondraw = StreamThing_ondraw;
        thing->onmousewheel = StreamThing_onmousewheel;
        thing->can_be_dropped_onto = can_be_dropped_onto;
        thing->meter = 1.0f;  /* gain. */
    }
    return thing;
}

static void WavThing_ondrag(Thing *thing, int button, float x, float y)
{
    if (button == SDL_BUTTON_RIGHT) {  /* drag out a new audio stream. */
        dragging_thing = CreateStreamThing(&thing->data.wav.spec, thing->data.wav.buf, thing->data.wav.buflen, thing->titlebar, x - (thing->rect.w / 2), y - (thing->rect.h / 2));
    }
}

static void WavThing_ondrop(Thing *thing, int button, float x, float y)
{
    if (droppable_highlighted_thing) {
        if (droppable_highlighted_thing->what == THING_TRASHCAN) {
            TrashThing(thing);
        }
    }
}

static Thing *LoadWavThing(const char *fname, float x, float y)
{
    Thing *thing = NULL;
    char *path;
    SDL_AudioSpec spec;
    Uint8 *buf = NULL;
    Uint32 buflen = 0;

    path = GetNearbyFilename(fname);
    if (path) {
        fname = path;
    }

    if (SDL_LoadWAV(fname, &spec, &buf, &buflen) == 0) {
        static const ThingType can_be_dropped_onto[] = { THING_TRASHCAN, THING_NULL };
        char *titlebar = NULL;
        const char *nodirs = SDL_strrchr(fname, '/');
        #ifdef SDL_PLATFORM_WINDOWS
        const char *nodirs2 = SDL_strrchr(nodirs ? nodirs : fname, '\\');
        if (nodirs2) {
            nodirs = nodirs2;
        }
        #endif

        SDL_Log("Adding WAV file '%s'", fname);

        if (nodirs) {
            nodirs++;
        } else {
            nodirs = fname;
        }

        SDL_asprintf(&titlebar, "WAV file (\"%s\", %s, %s, %uHz)", nodirs, AudioFmtToString(spec.format), AudioChansToStr(spec.channels), (unsigned int) spec.freq);
        thing = CreateThing(THING_WAV, x - (audio_texture->w / 2), y - (audio_texture->h / 2), 5, -1, -1, audio_texture, titlebar);
        if (thing) {
            SDL_free(titlebar);
            SDL_memcpy(&thing->data.wav.spec, &spec, sizeof (SDL_AudioSpec));
            thing->data.wav.buf = buf;
            thing->data.wav.buflen = buflen;
            thing->can_be_dropped_onto = can_be_dropped_onto;
            thing->ondrag = WavThing_ondrag;
            thing->ondrop = WavThing_ondrop;
        }
    }

    SDL_free(path);

    return thing;
}

static Thing *LoadStockWavThing(const char *fname)
{
    char *path = GetNearbyFilename(fname);
    Thing *thing = LoadWavThing(path ? path : fname, 0.0f, 0.0f);  /* will reposition in a moment. */
    SDL_free(path);
    return thing;
}

static void LoadStockWavThings(void)
{
    LoadStockWavThing("sample.wav");
    RepositionRowOfThings(THING_WAV, -10.0f);
}

static void DestroyTexture(Texture *tex)
{
    if (tex) {
        if (state->renderers[0] != NULL) {  /* if the renderer went away, this pointer is already bogus. */
            SDL_DestroyTexture(tex->texture);
        }
        SDL_free(tex);
    }
}

static Texture *CreateTexture(const char *fname)
{
    Texture *tex = (Texture *) SDL_calloc(1, sizeof (Texture));
    if (!tex) {
        SDL_Log("Out of memory!");
    } else {
        int texw, texh;
        tex->texture = LoadTexture(state->renderers[0], fname, SDL_TRUE, &texw, &texh);
        if (!tex->texture) {
            SDL_Log("Failed to load '%s': %s", fname, SDL_GetError());
            SDL_free(tex);
            return NULL;
        }
        SDL_SetTextureBlendMode(tex->texture, SDL_BLENDMODE_BLEND);
        tex->w = (float) texw;
        tex->h = (float) texh;
    }
    return tex;
}

static Thing *CreateLogicalDeviceThing(Thing *parent, const SDL_AudioDeviceID which, const float x, const float y);

static void DeviceThing_ondrag(Thing *thing, int button, float x, float y)
{
    if ((button == SDL_BUTTON_MIDDLE) && (thing->what == THING_LOGDEV_RECORDING)) {  /* drag out a new stream. This is a UX mess. :/ */
        dragging_thing = CreateStreamThing(&thing->data.logdev.spec, NULL, 0, NULL, x, y);
        if (dragging_thing) {
            dragging_thing->data.stream.next_level_update = SDL_GetTicks() + 100;
            SDL_BindAudioStream(thing->data.logdev.devid, dragging_thing->data.stream.stream); /* bind to new device! */
            dragging_thing->line_connected_to = thing;
        }
    } else if (button == SDL_BUTTON_RIGHT) {  /* drag out a new logical device. */
        const SDL_AudioDeviceID which = ((thing->what == THING_LOGDEV) || (thing->what == THING_LOGDEV_RECORDING)) ? thing->data.logdev.devid : thing->data.physdev.devid;
        const SDL_AudioDeviceID devid = SDL_OpenAudioDevice(which, NULL);
        dragging_thing = devid ? CreateLogicalDeviceThing(thing, devid, x - (thing->rect.w / 2), y - (thing->rect.h / 2)) : NULL;
    }
}

static void SetLogicalDeviceTitlebar(Thing *thing)
{
    SDL_AudioSpec *spec = &thing->data.logdev.spec;
    int frames = 0;
    SDL_GetAudioDeviceFormat(thing->data.logdev.devid, spec, &frames);
    SDL_free(thing->titlebar);
    SDL_asprintf(&thing->titlebar, "Logical device #%u (%s, %s, %s, %uHz, %d frames)", (unsigned int) thing->data.logdev.devid, thing->data.logdev.recording ? "RECORDING" : "PLAYBACK", AudioFmtToString(spec->format), AudioChansToStr(spec->channels), (unsigned int) spec->freq, frames);
}

static void LogicalDeviceThing_ondrop(Thing *thing, int button, float x, float y)
{
    if (droppable_highlighted_thing) {
        if (droppable_highlighted_thing->what == THING_TRASHCAN) {
            TrashThing(thing);
        }
    }
}

static void SDLCALL PostmixCallback(void *userdata, const SDL_AudioSpec *spec, float *buffer, int buflen)
{
    Thing *thing = (Thing *) userdata;

    SDL_LockMutex(thing->data.logdev.postmix_lock);

    if (thing->data.logdev.postmix_allocated < buflen) {
        void *ptr = SDL_realloc(thing->data.logdev.postmix_buffer, buflen);
        if (!ptr) {
            SDL_UnlockMutex(thing->data.logdev.postmix_lock);
            return;  /* oh well. */
        }
        thing->data.logdev.postmix_buffer = (float *) ptr;
        thing->data.logdev.postmix_allocated = buflen;
    }

    SDL_copyp(&thing->data.logdev.postmix_spec, spec);
    SDL_memcpy(thing->data.logdev.postmix_buffer, buffer, buflen);
    thing->data.logdev.postmix_buflen = buflen;
    SDL_AtomicSet(&thing->data.logdev.postmix_updated, 1);

    SDL_UnlockMutex(thing->data.logdev.postmix_lock);
}

static void UpdateVisualizer(SDL_Renderer *renderer, SDL_Texture *visualizer, const int channels, const float *buffer, const int buflen)
{
    static const SDL_Color channel_colors[8] = {
        { 255, 255, 255, 255 },
        { 255, 0, 0, 255 },
        { 0, 255, 0, 255 },
        { 0, 0, 255, 255 },
        { 255, 255, 0, 255 },
        { 0, 255, 255, 255 },
        { 255, 0, 255, 255 },
        { 127, 127, 127, 255 }
    };

    SDL_SetRenderTarget(renderer, visualizer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    if (buffer && buflen) {
        const int frames = (buflen / sizeof (float)) / channels;
        const int skip = frames / (VISUALIZER_WIDTH * 2);
        int i, j;

        for (i = channels - 1; i >= 0; i--) {
            const SDL_Color *color = &channel_colors[i % SDL_arraysize(channel_colors)];
            SDL_FPoint points[VISUALIZER_WIDTH + 2];
            float prevx = 0.0f;
            int pointidx = 1;

            points[0].x = 0.0f;
            points[0].y = VISUALIZER_HEIGHT * 0.5f;

            for (j = 0; j < (SDL_arraysize(points)-1); j++) {
                const float val = buffer[((j * skip) * channels) + i];
                const float x = prevx + 2;
                const float y = (VISUALIZER_HEIGHT * 0.5f) - (VISUALIZER_HEIGHT * (val * 0.5f));
                SDL_assert(pointidx < SDL_arraysize(points));
                points[pointidx].x = x;
                points[pointidx].y = y;
                pointidx++;
                prevx = x;
            }

            SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, 255);
            SDL_RenderLines(renderer, points, pointidx);
        }
    }

    SDL_SetRenderTarget(renderer, NULL);
}

static void LogicalDeviceThing_ontick(Thing *thing, Uint64 now)
{
    const SDL_bool ismousedover = (thing == mouseover_thing);

    if (!thing->data.logdev.visualizer || !thing->data.logdev.postmix_lock) {  /* need these to work, skip if they failed. */
        return;
    }

    if (thing->data.logdev.visualizer_enabled != ismousedover) {
        thing->data.logdev.visualizer_enabled = ismousedover;
        if (!ismousedover) {
            SDL_SetAudioPostmixCallback(thing->data.logdev.devid, NULL, NULL);
        } else {
            if (thing->data.logdev.postmix_buffer) {
                SDL_memset(thing->data.logdev.postmix_buffer, '\0', thing->data.logdev.postmix_buflen);
            }
            SDL_AtomicSet(&thing->data.logdev.postmix_updated, 1);  /* so this will at least clear the texture later. */
            SDL_SetAudioPostmixCallback(thing->data.logdev.devid, PostmixCallback, thing);
        }
    }
}

static void LogicalDeviceThing_ondraw(Thing *thing, SDL_Renderer *renderer)
{
    if (thing->data.logdev.visualizer_enabled) {
        SDL_FRect dst;
        dst.w = thing->rect.w;
        dst.h = thing->rect.h;
        dst.x = thing->rect.x + ((thing->rect.w - dst.w) / 2);
        dst.y = thing->rect.y + ((thing->rect.h - dst.h) / 2);

        if (SDL_AtomicGet(&thing->data.logdev.postmix_updated)) {
            float *buffer;
            int channels;
            int buflen;

            SDL_LockMutex(thing->data.logdev.postmix_lock);
            channels = thing->data.logdev.postmix_spec.channels;
            buflen = thing->data.logdev.postmix_buflen;
            buffer = (float *) SDL_malloc(thing->data.logdev.postmix_buflen);
            if (buffer) {
                SDL_memcpy(buffer, thing->data.logdev.postmix_buffer, thing->data.logdev.postmix_buflen);
                SDL_AtomicSet(&thing->data.logdev.postmix_updated, 0);
            }
            SDL_UnlockMutex(thing->data.logdev.postmix_lock);

            UpdateVisualizer(renderer, thing->data.logdev.visualizer, channels, buffer, buflen);
            SDL_free(buffer);
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 30);
        SDL_RenderTexture(renderer, thing->data.logdev.visualizer, NULL, &dst);
    }
}

static void LogicalDeviceThing_onmousewheel(Thing *thing, float y)
{
    thing->meter += y * 0.01f;
    thing->meter = SDL_clamp(thing->meter, 0.0f, 1.0f);
    SDL_SetAudioDeviceGain(thing->data.logdev.devid, thing->meter);
}

static Thing *CreateLogicalDeviceThing(Thing *parent, const SDL_AudioDeviceID which, const float x, const float y)
{
    static const ThingType can_be_dropped_onto[] = { THING_TRASHCAN, THING_NULL };
    Thing *physthing = ((parent->what == THING_LOGDEV) || (parent->what == THING_LOGDEV_RECORDING)) ? parent->data.logdev.physdev : parent;
    const SDL_bool recording = physthing->data.physdev.recording;
    Thing *thing;

    SDL_Log("Adding logical audio device %u", (unsigned int) which);
    thing = CreateThing(recording ? THING_LOGDEV_RECORDING : THING_LOGDEV, x, y, 5, -1, -1, logdev_texture, NULL);
    if (thing) {
        thing->data.logdev.devid = which;
        thing->data.logdev.recording = recording;
        thing->data.logdev.physdev = physthing;
        thing->data.logdev.visualizer = SDL_CreateTexture(state->renderers[0], SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, VISUALIZER_WIDTH, VISUALIZER_HEIGHT);
        thing->data.logdev.postmix_lock = SDL_CreateMutex();
        if (thing->data.logdev.visualizer) {
            SDL_SetTextureBlendMode(thing->data.logdev.visualizer, SDL_BLENDMODE_BLEND);
        }
        thing->line_connected_to = physthing;
        thing->meter = 1.0f;
        thing->ontick = LogicalDeviceThing_ontick;
        thing->ondrag = DeviceThing_ondrag;
        thing->ondrop = LogicalDeviceThing_ondrop;
        thing->ondraw = LogicalDeviceThing_ondraw;
        thing->onmousewheel = LogicalDeviceThing_onmousewheel;
        thing->can_be_dropped_onto = can_be_dropped_onto;

        SetLogicalDeviceTitlebar(thing);
    }
    return thing;
}

static void SetPhysicalDeviceTitlebar(Thing *thing)
{
    int frames = 0;
    SDL_AudioSpec *spec = &thing->data.physdev.spec;
    SDL_GetAudioDeviceFormat(thing->data.physdev.devid, spec, &frames);
    SDL_free(thing->titlebar);
    if (thing->data.physdev.devid == SDL_AUDIO_DEVICE_DEFAULT_RECORDING) {
        SDL_asprintf(&thing->titlebar, "Default system device (RECORDING, %s, %s, %uHz, %d frames)", AudioFmtToString(spec->format), AudioChansToStr(spec->channels), (unsigned int) spec->freq, frames);
    } else if (thing->data.physdev.devid == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
        SDL_asprintf(&thing->titlebar, "Default system device (PLAYBACK, %s, %s, %uHz, %d frames)", AudioFmtToString(spec->format), AudioChansToStr(spec->channels), (unsigned int) spec->freq, frames);
    } else {
        SDL_asprintf(&thing->titlebar, "Physical device #%u (%s, \"%s\", %s, %s, %uHz, %d frames)", (unsigned int) thing->data.physdev.devid, thing->data.physdev.recording ? "RECORDING" : "PLAYBACK", thing->data.physdev.name, AudioFmtToString(spec->format), AudioChansToStr(spec->channels), (unsigned int) spec->freq, frames);
    }
}

static void PhysicalDeviceThing_ondrop(Thing *thing, int button, float x, float y)
{
    if (droppable_highlighted_thing) {
        if (droppable_highlighted_thing->what == THING_TRASHCAN) {
            TrashThing(thing);
        }
    }
}

static void PhysicalDeviceThing_ontick(Thing *thing, Uint64 now)
{
    const int lifetime = POOF_LIFETIME;
    const int elasped = (int) (now - thing->createticks);
    if (elasped > lifetime) {
        thing->scale = 1.0f;
        thing->a = 255;
        thing->ontick = NULL;  /* no more ticking. */
    } else {
        const float pct = ((float) elasped) / ((float) lifetime);
        thing->a = (Uint8) (int) (pct * 255.0f);
        thing->scale = pct;  /* grow to normal size */
    }
}


static Thing *CreatePhysicalDeviceThing(const SDL_AudioDeviceID which, const SDL_bool recording)
{
    static const ThingType can_be_dropped_onto[] = { THING_TRASHCAN, THING_NULL };
    static float next_physdev_x = 0;
    Thing *thing;
    int winw, winh;

    SDL_GetWindowSize(state->windows[0], &winw, &winh);
    if (next_physdev_x > (winw-physdev_texture->w)) {
        next_physdev_x = 0;
    }

    SDL_Log("Adding physical audio device %u", (unsigned int) which);
    thing = CreateThing(recording ? THING_PHYSDEV_RECORDING : THING_PHYSDEV, next_physdev_x, 170, 5, -1, -1, physdev_texture, NULL);
    if (thing) {
        const char *name = SDL_GetAudioDeviceName(which);
        thing->data.physdev.devid = which;
        thing->data.physdev.recording = recording;
        thing->data.physdev.name = name ? SDL_strdup(name) : NULL;
        thing->ondrag = DeviceThing_ondrag;
        thing->ondrop = PhysicalDeviceThing_ondrop;
        thing->ontick = PhysicalDeviceThing_ontick;
        thing->can_be_dropped_onto = can_be_dropped_onto;

        SetPhysicalDeviceTitlebar(thing);
        if (SDL_GetTicks() <= (app_ready_ticks + 2000)) {  /* assume this is the initial batch if it happens in the first two seconds. */
            RepositionRowOfThings(THING_PHYSDEV, 10.0f);  /* don't rearrange them after the initial add. */
            RepositionRowOfThings(THING_PHYSDEV_RECORDING, 170.0f);  /* don't rearrange them after the initial add. */
            next_physdev_x = 0.0f;
        } else {
            next_physdev_x += physdev_texture->w * 1.5f;
        }
    }

    return thing;
}

static Thing *CreateTrashcanThing(void)
{
    int winw, winh;
    SDL_GetWindowSize(state->windows[0], &winw, &winh);
    return CreateThing(THING_TRASHCAN, winw - trashcan_texture->w, winh - trashcan_texture->h, 10, -1, -1, trashcan_texture, "Drag things here to remove them.");
}

static Thing *CreateDefaultPhysicalDevice(const SDL_bool recording)
{
    return CreatePhysicalDeviceThing(recording ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, recording);
}

static void TickThings(void)
{
    Thing *i;
    Thing *next;
    const Uint64 now = SDL_GetTicks();
    for (i = things; i; i = next) {
        next = i->next;  /* in case this deletes itself. */
        if (i->ontick) {
            i->ontick(i, now);
        }
    }
}

static void WindowResized(const int newwinw, const int newwinh)
{
    Thing *i;
    const float neww = (float) newwinw;
    const float newh = (float) newwinh;
    const float oldw = (float) state->window_w;
    const float oldh = (float) state->window_h;
    for (i = things; i; i = i->next) {
        const float halfw = i->rect.w / 2.0f;
        const float halfh = i->rect.h / 2.0f;
        const float x = (i->rect.x + halfw) / oldw;
        const float y = (i->rect.y + halfh) / oldh;
        i->rect.x = (x * neww) - halfw;
        i->rect.y = (y * newh) - halfh;
    }
    state->window_w = newwinw;
    state->window_h = newwinh;
}

int SDL_AppInit(void **appstate, int argc, char *argv[])
{
    int i;

    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    state->window_flags |= SDL_WINDOW_RESIZABLE;

    for (i = 1; i < argc;) {
        int consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
            /* add our own command lines here. */
        }
        if (consumed < 0) {
            static const char *options[] = {
                /* add our own command lines here. */
                /*"[--blend none|blend|add|mod|mul|sub]",*/
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return SDL_APP_FAILURE;
        }
        i += consumed;
    }

    if (!SDLTest_CommonInit(state)) {
        return SDL_APP_FAILURE;
    }

    if (state->audio_id) {
        SDL_CloseAudioDevice(state->audio_id);
        state->audio_id = 0;
    }

    SetDefaultTitleBar();

    if ((physdev_texture = CreateTexture("physaudiodev.bmp")) == NULL) { return -1; }
    if ((logdev_texture = CreateTexture("logaudiodev.bmp")) == NULL) { return -1; }
    if ((audio_texture = CreateTexture("audiofile.bmp")) == NULL) { return -1; }
    if ((trashcan_texture = CreateTexture("trashcan.bmp")) == NULL) { return -1; }
    if ((soundboard_texture = CreateTexture("soundboard.bmp")) == NULL) { return -1; }
    if ((soundboard_levels_texture = CreateTexture("soundboard_levels.bmp")) == NULL) { return -1; }

    LoadStockWavThings();
    CreateTrashcanThing();
    CreateDefaultPhysicalDevice(SDL_FALSE);
    CreateDefaultPhysicalDevice(SDL_TRUE);

    return SDL_APP_CONTINUE;
}


static SDL_bool saw_event = SDL_FALSE;

int SDL_AppEvent(void *appstate, const SDL_Event *event)
{
    Thing *thing = NULL;

    saw_event = SDL_TRUE;

    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            thing = UpdateMouseOver(event->motion.x, event->motion.y);
            if ((dragging_button == -1) && event->motion.state) {
                if (event->motion.state & SDL_BUTTON_LMASK) {
                    /* for people that don't have all three buttons... */
                    if (ctrl_held) {
                        dragging_button = SDL_BUTTON_RIGHT;
                    } else if (alt_held) {
                        dragging_button = SDL_BUTTON_MIDDLE;
                    } else {
                        dragging_button = SDL_BUTTON_LEFT;
                    }
                    dragging_button_real = SDL_BUTTON_LEFT;
                } else if (event->motion.state & SDL_BUTTON_RMASK) {
                    dragging_button = SDL_BUTTON_RIGHT;
                    dragging_button_real = SDL_BUTTON_RIGHT;
                } else if (event->motion.state & SDL_BUTTON_MMASK) {
                    dragging_button = SDL_BUTTON_MIDDLE;
                    dragging_button_real = SDL_BUTTON_MIDDLE;
                }

                if (dragging_button != -1) {
                    dragging_thing = thing;
                    if (thing && thing->ondrag) {
                        thing->ondrag(thing, dragging_button, event->motion.x, event->motion.y);
                    }
                }
            }

            droppable_highlighted_thing = NULL;
            if (dragging_thing) {
                dragging_thing->rect.x = event->motion.x - (dragging_thing->rect.w / 2);
                dragging_thing->rect.y = event->motion.y - (dragging_thing->rect.h / 2);
                if (dragging_thing->can_be_dropped_onto) {
                    thing = FindThingAtPoint(event->motion.x, event->motion.y);
                    if (thing) {
                        int i;
                        for (i = 0; dragging_thing->can_be_dropped_onto[i]; i++) {
                            if (dragging_thing->can_be_dropped_onto[i] == thing->what) {
                                droppable_highlighted_thing = thing;
                                break;
                            }
                        }
                    }
                }
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            thing = UpdateMouseOver(event->button.x, event->button.y);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (dragging_button_real == event->button.button) {
                Thing *dropped_thing = dragging_thing;
                dragging_thing = NULL;
                dragging_button = -1;
                dragging_button_real = -1;
                if (dropped_thing && dropped_thing->ondrop) {
                    dropped_thing->ondrop(dropped_thing, event->button.button, event->button.x, event->button.y);
                }
                droppable_highlighted_thing = NULL;
            }
            thing = UpdateMouseOver(event->button.x, event->button.y);
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            thing = UpdateMouseOver(event->wheel.mouse_x, event->wheel.mouse_y);
            if (thing && thing->onmousewheel) {
                thing->onmousewheel(thing, event->wheel.y * ((event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -1.0f : 1.0f));
            }
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            ctrl_held = ((event->key.mod & SDL_KMOD_CTRL) != 0);
            alt_held = ((event->key.mod & SDL_KMOD_ALT) != 0);
            break;

        case SDL_EVENT_DROP_FILE:
            SDL_Log("Drop file! '%s'", event->drop.data);
            LoadWavThing(event->drop.data, event->drop.x, event->drop.y);
            /* SDL frees event->drop.data for you when you use SDL_AppEvent(). */
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            WindowResized(event->window.data1, event->window.data2);
            break;

        case SDL_EVENT_AUDIO_DEVICE_ADDED:
            CreatePhysicalDeviceThing(event->adevice.which, event->adevice.recording);
            break;

        case SDL_EVENT_AUDIO_DEVICE_REMOVED: {
            const SDL_AudioDeviceID which = event->adevice.which;
            Thing *i, *next;
            SDL_Log("Removing audio device %u", (unsigned int) which);
            for (i = things; i; i = next) {
                next = i->next;
                if (((i->what == THING_PHYSDEV) || (i->what == THING_PHYSDEV_RECORDING)) && (i->data.physdev.devid == which)) {
                    TrashThing(i);
                    next = things;  /* in case we mangled the list. */
                } else if (((i->what == THING_LOGDEV) || (i->what == THING_LOGDEV_RECORDING)) && (i->data.logdev.devid == which)) {
                    TrashThing(i);
                    next = things;  /* in case we mangled the list. */
                }
            }
            break;
        }

        default: break;
    }

    return SDLTest_CommonEventMainCallbacks(state, event);
}

int SDL_AppIterate(void *appstate)
{
    if (app_ready_ticks == 0) {
        app_ready_ticks = SDL_GetTicks();
    }

    TickThings();
    Draw();

    if (saw_event) {
        saw_event = SDL_FALSE;  /* reset this so we know when SDL_AppEvent() runs again */
    } else {
        SDL_Delay(10);
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate)
{
    while (things) {
        DestroyThing(things);  /* make sure all the audio devices are closed, etc. */
    }

    DestroyTexture(physdev_texture);
    DestroyTexture(logdev_texture);
    DestroyTexture(audio_texture);
    DestroyTexture(trashcan_texture);
    DestroyTexture(soundboard_texture);
    DestroyTexture(soundboard_levels_texture);
    SDLTest_CommonQuit(state);
}

