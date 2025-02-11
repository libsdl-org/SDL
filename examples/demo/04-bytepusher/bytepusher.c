/*
 * An implementation of the BytePusher VM.
 *
 * For example programs and more information about BytePusher, see
 * https://esolangs.org/wiki/BytePusher
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdarg.h>

#define SCREEN_W 256
#define SCREEN_H 256
#define RAM_SIZE 0x1000000
#define FRAMES_PER_SECOND 60
#define SAMPLES_PER_FRAME 256
#define NS_PER_SECOND (Uint64)SDL_NS_PER_SECOND
#define MAX_AUDIO_LATENCY_FRAMES 5

#define IO_KEYBOARD 0
#define IO_PC 2
#define IO_SCREEN_PAGE 5
#define IO_AUDIO_BANK 6

typedef struct {
    Uint8 ram[RAM_SIZE + 8];
    Uint8 screenbuf[SCREEN_W * SCREEN_H];
    Uint64 last_tick;
    Uint64 tick_acc;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Surface* screen; 
    SDL_Texture* screentex;
    SDL_Texture* rendertarget; /* we need this render target for text to look good */
    SDL_AudioStream* audiostream;
    char status[SCREEN_W / 8];
    int status_ticks;
    Uint16 keystate;
    bool display_help;
    bool positional_input;
} BytePusher;

static const struct {
    const char *key;
    const char *value;
} extended_metadata[] = {
    { SDL_PROP_APP_METADATA_URL_STRING, "https://examples.libsdl.org/SDL3/demo/04-bytepusher/" },
    { SDL_PROP_APP_METADATA_CREATOR_STRING, "SDL team" },
    { SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "Placed in the public domain" },
    { SDL_PROP_APP_METADATA_TYPE_STRING, "game" }
};

static inline Uint16 read_u16(const BytePusher* vm, Uint32 addr) {
    const Uint8* ptr = &vm->ram[addr];
    return ((Uint16)ptr[0] << 8) | ((Uint16)ptr[1]);
}

static inline Uint32 read_u24(const BytePusher* vm, Uint32 addr) {
    const Uint8* ptr = &vm->ram[addr];
    return ((Uint32)ptr[0] << 16) | ((Uint32)ptr[1] << 8) | ((Uint32)ptr[2]);
}

static void set_status(BytePusher* vm, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(vm->status, sizeof(vm->status), fmt, args);
    va_end(args);
    vm->status[sizeof(vm->status) - 1] = 0;
    vm->status_ticks = FRAMES_PER_SECOND * 3;
}

static bool load(BytePusher* vm, SDL_IOStream* stream, bool closeio) {
    size_t bytes_read = 0;
    bool ok = true;

    SDL_memset(vm->ram, 0, RAM_SIZE);

    if (!stream) {
        return false;
    }

    while (bytes_read < RAM_SIZE) {
        size_t read = SDL_ReadIO(stream, &vm->ram[bytes_read], RAM_SIZE - bytes_read);
        bytes_read += read;
        if (read == 0) {
            ok = SDL_GetIOStatus(stream) == SDL_IO_STATUS_EOF;
            break;
        }
    }
    if (closeio) {
        SDL_CloseIO(stream);
    }

    SDL_ClearAudioStream(vm->audiostream);

    vm->display_help = !ok;
    return ok;
}

static const char* filename(const char* path) {
    size_t i = SDL_strlen(path) + 1;
    while (i > 0) {
        i -= 1;
        if (path[i] == '/' || path[i] == '\\') {
            return path + i + 1;
        }
    }
    return path;
}

static bool load_file(BytePusher* vm, const char* path) {
    if (load(vm, SDL_IOFromFile(path, "rb"), true)) {
        set_status(vm, "loaded %s", filename(path));
        return true;
    } else {
        set_status(vm, "load failed: %s", filename(path));
        return false;
    }
}

static void print(BytePusher* vm, int x, int y, const char* str) {
    SDL_SetRenderDrawColor(vm->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(vm->renderer, (float)(x + 1), (float)(y + 1), str);
    SDL_SetRenderDrawColor(vm->renderer, 0xff, 0xff, 0xff, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(vm->renderer, (float)x, (float)y, str);
    SDL_SetRenderDrawColor(vm->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    BytePusher* vm;
    SDL_Palette* palette;
    SDL_Rect usable_bounds;
    SDL_AudioSpec audiospec = { SDL_AUDIO_S8, 1, SAMPLES_PER_FRAME * FRAMES_PER_SECOND };
    SDL_DisplayID primary_display;
    SDL_PropertiesID texprops;
    int zoom = 2;
    int i;
    Uint8 r, g, b;
    (void)argc;
    (void)argv;

    if (!SDL_SetAppMetadata("SDL 3 BytePusher", "1.0", "com.example.SDL3BytePusher")) {
        return SDL_APP_FAILURE;
    }

    for (i = 0; i < (int)SDL_arraysize(extended_metadata); i++) {
        if (!SDL_SetAppMetadataProperty(extended_metadata[i].key, extended_metadata[i].value)) {
            return SDL_APP_FAILURE;
        }
    }

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        return SDL_APP_FAILURE;
    }

    if (!(vm = (BytePusher *)SDL_calloc(1, sizeof(*vm)))) {
        return SDL_APP_FAILURE;
    }
    *(BytePusher**)appstate = vm;

    vm->display_help = true;

    primary_display = SDL_GetPrimaryDisplay();
    if (SDL_GetDisplayUsableBounds(primary_display, &usable_bounds)) {
        int zoom_w = (usable_bounds.w - usable_bounds.x) * 2 / 3 / SCREEN_W;
        int zoom_h = (usable_bounds.h - usable_bounds.y) * 2 / 3 / SCREEN_H;
        zoom = zoom_w < zoom_h ? zoom_w : zoom_h;
        if (zoom < 1) {
            zoom = 1;
        }
    }

    if (!SDL_CreateWindowAndRenderer("SDL 3 BytePusher",
        SCREEN_W * zoom, SCREEN_H * zoom, SDL_WINDOW_RESIZABLE,
        &vm->window, &vm->renderer
    )) {
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderLogicalPresentation(
        vm->renderer, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
    )) {
        return SDL_APP_FAILURE;
    }

    if (!(vm->screen = SDL_CreateSurfaceFrom(
        SCREEN_W, SCREEN_H, SDL_PIXELFORMAT_INDEX8, vm->screenbuf, SCREEN_W
    ))) {
        return SDL_APP_FAILURE;
    }

    if (!(palette = SDL_CreateSurfacePalette(vm->screen))) {
        return SDL_APP_FAILURE;
    }
    i = 0;
    for (r = 0; r < 6; ++r) {
        for (g = 0; g < 6; ++g) {
            for (b = 0; b < 6; ++b, ++i) {
                SDL_Color color = { (Uint8)(r * 0x33), (Uint8)(g * 0x33), (Uint8)(b * 0x33), SDL_ALPHA_OPAQUE };
                palette->colors[i] = color;
            }
        }
    }
    for (; i < 256; ++i) {
        SDL_Color color = { 0, 0, 0, SDL_ALPHA_OPAQUE };
        palette->colors[i] = color;
    }

    texprops = SDL_CreateProperties();
    SDL_SetNumberProperty(texprops, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STREAMING);
    SDL_SetNumberProperty(texprops, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, SCREEN_W);
    SDL_SetNumberProperty(texprops, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, SCREEN_H);
    vm->screentex = SDL_CreateTextureWithProperties(vm->renderer, texprops);
    SDL_SetNumberProperty(texprops, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_TARGET);
    vm->rendertarget = SDL_CreateTextureWithProperties(vm->renderer, texprops);
    SDL_DestroyProperties(texprops);
    if (!vm->screentex || !vm->rendertarget) {
        return SDL_APP_FAILURE;
    }
    SDL_SetTextureScaleMode(vm->screentex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureScaleMode(vm->rendertarget, SDL_SCALEMODE_NEAREST);

    if (!(vm->audiostream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audiospec, NULL, NULL
    ))) {
        return SDL_APP_FAILURE;
    }
    SDL_SetAudioStreamGain(vm->audiostream, 0.1f); /* examples are loud! */
    SDL_ResumeAudioStreamDevice(vm->audiostream);

    set_status(vm, "renderer: %s", SDL_GetRendererName(vm->renderer));

    vm->last_tick = SDL_GetTicksNS();
    vm->tick_acc = NS_PER_SECOND;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    BytePusher* vm = (BytePusher*)appstate;

    Uint64 tick = SDL_GetTicksNS();
    Uint64 delta = tick - vm->last_tick;
    bool updated, skip_audio;

    vm->last_tick = tick;

    vm->tick_acc += delta * FRAMES_PER_SECOND;
    updated = vm->tick_acc >= NS_PER_SECOND;
    skip_audio = vm->tick_acc >= MAX_AUDIO_LATENCY_FRAMES * NS_PER_SECOND;

    if (skip_audio) {
        // don't let audio fall too far behind
        SDL_ClearAudioStream(vm->audiostream);
    }

    while (vm->tick_acc >= NS_PER_SECOND) {
        Uint32 pc;
        int i;

        vm->tick_acc -= NS_PER_SECOND;

        vm->ram[IO_KEYBOARD] = (Uint8)(vm->keystate >> 8);
        vm->ram[IO_KEYBOARD + 1] = (Uint8)(vm->keystate);

        pc = read_u24(vm, IO_PC);
        for (i = 0; i < SCREEN_W * SCREEN_H; ++i) {
            Uint32 src = read_u24(vm, pc);
            Uint32 dst = read_u24(vm, pc + 3);
            vm->ram[dst] = vm->ram[src];
            pc = read_u24(vm, pc + 6);
        }

        if (!skip_audio || vm->tick_acc < NS_PER_SECOND) {
            SDL_PutAudioStreamData(
                vm->audiostream,
                &vm->ram[(Uint32)read_u16(vm, IO_AUDIO_BANK) << 8],
                SAMPLES_PER_FRAME
            );
        }
    }

    if (updated) {
        SDL_Surface *tex;

        SDL_SetRenderTarget(vm->renderer, vm->rendertarget);

        if (!SDL_LockTextureToSurface(vm->screentex, NULL, &tex)) {
            return SDL_APP_FAILURE;
        }
        vm->screen->pixels = &vm->ram[(Uint32)vm->ram[IO_SCREEN_PAGE] << 16];
        SDL_BlitSurface(vm->screen, NULL, tex, NULL);
        SDL_UnlockTexture(vm->screentex);

        SDL_RenderTexture(vm->renderer, vm->screentex, NULL, NULL);
    }

    if (vm->display_help) {
        print(vm, 4, 4, "Drop a BytePusher file in this");
        print(vm, 8, 12, "window to load and run it!");
        print(vm, 4, 28, "Press ENTER to switch between");
        print(vm, 8, 36, "positional and symbolic input.");
    }

    if (vm->status_ticks > 0) {
        vm->status_ticks -= 1;
        print(vm, 4, SCREEN_H - 12, vm->status);
    }

    SDL_SetRenderTarget(vm->renderer, NULL);
    SDL_RenderClear(vm->renderer);
    SDL_RenderTexture(vm->renderer, vm->rendertarget, NULL, NULL);
    SDL_RenderPresent(vm->renderer);

    return SDL_APP_CONTINUE;
}

static Uint16 keycode_mask(SDL_Keycode key) {
    int index;
    if (key >= SDLK_0 && key <= SDLK_9) {
        index = key - SDLK_0;
    } else if (key >= SDLK_A && key <= SDLK_F) {
        index = key - SDLK_A + 10;
    } else {
        return 0;
    }
    return (Uint16)1 << index;
}

static Uint16 scancode_mask(SDL_Scancode scancode) {
    int index;
    switch (scancode) {
        case SDL_SCANCODE_1: index = 0x1; break;
        case SDL_SCANCODE_2: index = 0x2; break;
        case SDL_SCANCODE_3: index = 0x3; break;
        case SDL_SCANCODE_4: index = 0xc; break;
        case SDL_SCANCODE_Q: index = 0x4; break;
        case SDL_SCANCODE_W: index = 0x5; break;
        case SDL_SCANCODE_E: index = 0x6; break;
        case SDL_SCANCODE_R: index = 0xd; break;
        case SDL_SCANCODE_A: index = 0x7; break;
        case SDL_SCANCODE_S: index = 0x8; break;
        case SDL_SCANCODE_D: index = 0x9; break;
        case SDL_SCANCODE_F: index = 0xe; break;
        case SDL_SCANCODE_Z: index = 0xa; break;
        case SDL_SCANCODE_X: index = 0x0; break;
        case SDL_SCANCODE_C: index = 0xb; break;
        case SDL_SCANCODE_V: index = 0xf; break;
        default: return 0;
    }
    return (Uint16)1 << index;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    BytePusher* vm = (BytePusher*)appstate;

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_DROP_FILE:
            load_file(vm, event->drop.data);
            break;
        
        case SDL_EVENT_KEY_DOWN:
#ifndef __EMSCRIPTEN__
            if (event->key.key == SDLK_ESCAPE) {
                return SDL_APP_SUCCESS;
            }
#endif
            if (event->key.key == SDLK_RETURN) {
                vm->positional_input = !vm->positional_input;
                vm->keystate = 0;
                if (vm->positional_input) {
                    set_status(vm, "switched to positional input");
                } else {
                    set_status(vm, "switched to symbolic input");
                }
            }
            if (vm->positional_input) {
                vm->keystate |= scancode_mask(event->key.scancode);
            } else {
                vm->keystate |= keycode_mask(event->key.key);
            }
            break;
        
        case SDL_EVENT_KEY_UP: 
            if (vm->positional_input) {
                vm->keystate &= ~scancode_mask(event->key.scancode);
            } else {
                vm->keystate &= ~keycode_mask(event->key.key);
            }
            break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    if (result == SDL_APP_FAILURE) {
        SDL_Log("Error: %s", SDL_GetError());
    }
    if (appstate) {
        BytePusher* vm = (BytePusher*)appstate;
        SDL_DestroyAudioStream(vm->audiostream);
        SDL_DestroyTexture(vm->rendertarget);
        SDL_DestroyTexture(vm->screentex);
        SDL_DestroySurface(vm->screen);
        SDL_DestroyRenderer(vm->renderer);
        SDL_DestroyWindow(vm->window);
        SDL_free(vm);
    }
}
