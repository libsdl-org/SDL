/*
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* A simple program to test the Input Method support in the SDL library (2.0+) */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "SDL.h"
#ifdef HAVE_SDL_TTF
#include "SDL_ttf.h"
#endif

#include "SDL_test_common.h"

#define DEFAULT_PTSIZE  30
#define DEFAULT_FONT    "/System/Library/Fonts/华文细黑.ttf"
#define MAX_TEXT_LENGTH 256

static SDLTest_CommonState *state;
static SDL_Rect textRect, markedRect;
static SDL_Color lineColor = {0,0,0,0};
static SDL_Color backColor = {255,255,255,0};
static SDL_Color textColor = {0,0,0,0};
static char text[MAX_TEXT_LENGTH], markedText[SDL_TEXTEDITINGEVENT_TEXT_SIZE];
static int cursor = 0;
#ifdef HAVE_SDL_TTF
static TTF_Font *font;
#endif

size_t utf8_length(unsigned char c)
{
    c = (unsigned char)(0xff & c);
    if (c < 0x80)
        return 1;
    else if ((c >> 5) ==0x6)
        return 2;
    else if ((c >> 4) == 0xe)
        return 3;
    else if ((c >> 3) == 0x1e)
        return 4;
    else
        return 0;
}

char *utf8_next(char *p)
{
    size_t len = utf8_length(*p);
    size_t i = 0;
    if (!len)
        return 0;

    for (; i < len; ++i)
    {
        ++p;
        if (!*p)
            return 0;
    }
    return p;
}

char *utf8_advance(char *p, size_t distance)
{
    size_t i = 0;
    for (; i < distance && p; ++i)
    {
        p = utf8_next(p);
    }
    return p;
}

void usage()
{
    SDL_Log("usage: testime [--font fontfile]\n");
    exit(0);
}

void InitInput()
{

    /* Prepare a rect for text input */
    textRect.x = textRect.y = 100;
    textRect.w = DEFAULT_WINDOW_WIDTH - 2 * textRect.x;
    textRect.h = 50;

    text[0] = 0;
    markedRect = textRect;
    markedText[0] = 0;

    SDL_StartTextInput();
}

void CleanupVideo()
{
    SDL_StopTextInput();
#ifdef HAVE_SDL_TTF
    TTF_CloseFont(font);
    TTF_Quit();
#endif
}


void _Redraw(SDL_Renderer * renderer) {
    int w = 0, h = textRect.h;
    SDL_Rect cursorRect, underlineRect;

    SDL_SetRenderDrawColor(renderer, 255,255,255,255);
    SDL_RenderFillRect(renderer,&textRect);

#ifdef HAVE_SDL_TTF
    if (*text)
    {
        SDL_Surface *textSur = TTF_RenderUTF8_Blended(font, text, textColor);
        SDL_Rect dest = {textRect.x, textRect.y, textSur->w, textSur->h };

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer,textSur);
        SDL_FreeSurface(textSur);

        SDL_RenderCopy(renderer,texture,NULL,&dest);
        SDL_DestroyTexture(texture);
        TTF_SizeUTF8(font, text, &w, &h);
    }
#endif

    markedRect.x = textRect.x + w;
    markedRect.w = textRect.w - w;
    if (markedRect.w < 0)
    {
        /* Stop text input because we cannot hold any more characters */
        SDL_StopTextInput();
        return;
    }
    else
    {
        SDL_StartTextInput();
    }

    cursorRect = markedRect;
    cursorRect.w = 2;
    cursorRect.h = h;

    SDL_SetRenderDrawColor(renderer, 255,255,255,255);
    SDL_RenderFillRect(renderer,&markedRect);

    if (markedText[0])
    {
#ifdef HAVE_SDL_TTF
        if (cursor)
        {
            char *p = utf8_advance(markedText, cursor);
            char c = 0;
            if (!p)
                p = &markedText[strlen(markedText)];

            c = *p;
            *p = 0;
            TTF_SizeUTF8(font, markedText, &w, 0);
            cursorRect.x += w;
            *p = c;
        }
        SDL_Surface *textSur = TTF_RenderUTF8_Blended(font, markedText, textColor);
        SDL_Rect dest = {markedRect.x, markedRect.y, textSur->w, textSur->h };
        TTF_SizeUTF8(font, markedText, &w, &h);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer,textSur);
        SDL_FreeSurface(textSur);

        SDL_RenderCopy(renderer,texture,NULL,&dest);
        SDL_DestroyTexture(texture);
#endif

        underlineRect = markedRect;
        underlineRect.y += (h - 2);
        underlineRect.h = 2;
        underlineRect.w = w;

        SDL_SetRenderDrawColor(renderer, 0,0,0,0);
        SDL_RenderFillRect(renderer,&markedRect);
    }

    SDL_SetRenderDrawColor(renderer, 0,0,0,0);
    SDL_RenderFillRect(renderer,&cursorRect);

    SDL_SetTextInputRect(&markedRect);
}

void Redraw() {
    int i;
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (state->windows[i] == NULL)
            continue;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        _Redraw(renderer);

        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char *argv[]) {
    int i, done;
    SDL_Event event;
    const char *fontname = DEFAULT_FONT;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }
    for (i = 1; i < argc;i++) {
        SDLTest_CommonArg(state, i);
    }
    for (argc--, argv++; argc > 0; argc--, argv++)
    {
        if (strcmp(argv[0], "--help") == 0) {
            usage();
            return 0;
        }

        else if (strcmp(argv[0], "--font") == 0)
        {
            argc--;
            argv++;

            if (argc > 0)
                fontname = argv[0];
            else {
                usage();
                return 0;
            }
        }
    }

    if (!SDLTest_CommonInit(state)) {
        return 2;
    }


#ifdef HAVE_SDL_TTF
    /* Initialize fonts */
    TTF_Init();

    font = TTF_OpenFont(fontname, DEFAULT_PTSIZE);
    if (! font)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find font: %s\n", TTF_GetError());
        exit(-1);
    }
#endif

    SDL_Log("Using font: %s\n", fontname);
    atexit(SDL_Quit);

    InitInput();
    /* Create the windows and initialize the renderers */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }
    Redraw();
    /* Main render loop */
    done = 0;
    while (!done) {
        /* Check for events */
        while (SDL_PollEvent(&event)) {
            SDLTest_CommonEvent(state, &event, &done);
            switch(event.type) {
                case SDL_KEYDOWN: {
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_RETURN:
                             text[0]=0x00;
                             Redraw();
                             break;
                        case SDLK_BACKSPACE:
                             {
                                 int textlen=SDL_strlen(text);

                                 do {
                                     if (textlen==0)
                                     {
                                         break;
                                     }
                                     if ((text[textlen-1] & 0x80) == 0x00)
                                     {
                                         /* One byte */
                                         text[textlen-1]=0x00;
                                         break;
                                     }
                                     if ((text[textlen-1] & 0xC0) == 0x80)
                                     {
                                         /* Byte from the multibyte sequence */
                                         text[textlen-1]=0x00;
                                         textlen--;
                                     }
                                     if ((text[textlen-1] & 0xC0) == 0xC0)
                                     {
                                         /* First byte of multibyte sequence */
                                         text[textlen-1]=0x00;
                                         break;
                                     }
                                 } while(1);

                                 Redraw();
                             }
                             break;
                    }

                    if (done)
                    {
                        break;
                    }

                    SDL_Log("Keyboard: scancode 0x%08X = %s, keycode 0x%08X = %s\n",
                            event.key.keysym.scancode,
                            SDL_GetScancodeName(event.key.keysym.scancode),
                            event.key.keysym.sym, SDL_GetKeyName(event.key.keysym.sym));
                    break;

                case SDL_TEXTINPUT:
                    if (event.text.text[0] == '\0' || event.text.text[0] == '\n' ||
                        markedRect.w < 0)
                        break;

                    SDL_Log("Keyboard: text input \"%s\"\n", event.text.text);

                    if (SDL_strlen(text) + SDL_strlen(event.text.text) < sizeof(text))
                        SDL_strlcat(text, event.text.text, sizeof(text));

                    SDL_Log("text inputed: %s\n", text);

                    /* After text inputed, we can clear up markedText because it */
                    /* is committed */
                    markedText[0] = 0;
                    Redraw();
                    break;

                case SDL_TEXTEDITING:
                    SDL_Log("text editing \"%s\", selected range (%d, %d)\n",
                            event.edit.text, event.edit.start, event.edit.length);

                    strcpy(markedText, event.edit.text);
                    cursor = event.edit.start;
                    Redraw();
                    break;
                }
                break;

            }
        }
    }
    CleanupVideo();
    SDLTest_CommonQuit(state);
    return 0;
}


/* vi: set ts=4 sw=4 expandtab: */
