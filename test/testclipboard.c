/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "icon.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

static const char *mime_types[] = {
    "text/plain",
    "image/bmp",
};

static const void *ClipboardDataCallback(void *userdata, const char *mime_type, size_t *size)
{
    if (SDL_strcmp(mime_type, "text/plain") == 0) {
        const char *text = "Hello world!";
        *size = SDL_strlen(text);
        return text;
    } else if (SDL_strcmp(mime_type, "image/bmp") == 0) {
        *size = icon_bmp_len;
        return icon_bmp;
    } else {
        return NULL;
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("testclipboard", 640, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            return SDL_APP_SUCCESS;
        }
        if (event->key.key == SDLK_C && event->key.mod & SDL_KMOD_CTRL) {
            SDL_SetClipboardData(ClipboardDataCallback, NULL, NULL, mime_types, SDL_arraysize(mime_types));
            break;
        } else if (event->key.key == SDLK_P && event->key.mod & SDL_KMOD_CTRL) {
            SDL_SetPrimarySelectionText("SDL Primary Selection Text!");
        }
        break;

    case SDL_EVENT_CLIPBOARD_UPDATE:
        if (event->clipboard.num_mime_types > 0) {
            int i;
            SDL_Log("Clipboard updated:");
            for (i = 0; event->clipboard.mime_types[i]; ++i) {
                SDL_Log("    %s", event->clipboard.mime_types[i]);
            }
        } else {
            SDL_Log("Clipboard cleared");
        }
        break;

    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;

    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

static float PrintClipboardText(float x, float y, const char *mime_type)
{
    void *data = SDL_GetClipboardData(mime_type, NULL);
    if (data) {
        SDL_RenderDebugText(renderer, x, y, (const char *)data);
        SDL_free(data);
        return SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2.0f;
    }
    return 0.0f;
}

static float PrintPrimarySelectionText(float x, float y)
{
    if (SDL_HasPrimarySelectionText()) {
        SDL_RenderDebugText(renderer, x, y, SDL_GetPrimarySelectionText());
        return SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2.0f;
    }
    return 0.0f;
}

static float PrintClipboardImage(float x, float y, const char *mime_type)
{
    /* We don't actually need to read this data each frame, but this is a simple example */
    if (SDL_strcmp(mime_type, "image/bmp") == 0) {
        size_t size;
        void *data = SDL_GetClipboardData(mime_type, &size);
        if (data) {
            float w = 0.0f, h = 0.0f;
            bool rendered = false;
            SDL_IOStream *stream = SDL_IOFromConstMem(data, size);
            if (stream) {
                SDL_Surface *surface = SDL_LoadBMP_IO(stream, false);
                if (surface) {
                    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
                    if (texture) {
                        SDL_GetTextureSize(texture, &w, &h);

                        SDL_FRect dst = { x, y, w, h };
                        rendered = SDL_RenderTexture(renderer, texture, NULL, &dst);
                        SDL_DestroyTexture(texture);
                    }
                    SDL_DestroySurface(surface);
                }
                SDL_CloseIO(stream);
            }
            if (!rendered) {
                SDL_RenderDebugText(renderer, x, y, SDL_GetError());
            }
            SDL_free(data);
            return h + 2.0f;
        }
    }
    return 0.0f;
}

static float PrintClipboardContents(float x, float y)
{
    char **clipboard_mime_types = SDL_GetClipboardMimeTypes(NULL);
    if (clipboard_mime_types) {
        int i;

        for (i = 0; clipboard_mime_types[i]; ++i) {
            const char *mime_type = clipboard_mime_types[i];
            SDL_RenderDebugText(renderer, x, y, mime_type);
            y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2;
            if (SDL_strncmp(mime_type, "text/", 5) == 0) {
                y += PrintClipboardText(x + SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2, y, mime_type);
            } else if (SDL_strncmp(mime_type, "image/", 6) == 0) {
                y += PrintClipboardImage(x + SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2, y, mime_type);
            }
        }
        SDL_free(clipboard_mime_types);
    }

    return y;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    float x = 4.0f;
    float y = 4.0f;
    SDL_RenderDebugText(renderer, x, y, "Press Ctrl+C to copy content to the clipboard");
    y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2;
    SDL_RenderDebugText(renderer, x, y, "Press Ctrl+P to set the primary selection text");
    y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2;
    SDL_RenderDebugText(renderer, x, y, "Clipboard contents:");
    x += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2;
    y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2;
    y = PrintClipboardContents(x, y);
    if (SDL_HasPrimarySelectionText()) {
        x = 4.0f;
        SDL_RenderDebugText(renderer, x, y, "Primary selection text contents:");
        y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2;
        PrintPrimarySelectionText(x + SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2, y);
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}

