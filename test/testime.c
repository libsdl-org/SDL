/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* A simple program to test the Input Method support in the SDL library (2.0+)
   If you build without SDL_ttf, you can use the GNU Unifont hex file instead.
   Download at http://unifoundry.com/unifont.html */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#ifdef HAVE_SDL_TTF
#include "SDL_ttf.h"
#endif

#include <SDL3/SDL_test_common.h>
#include "testutils.h"

#ifdef HAVE_SDL_TTF
#define DEFAULT_PTSIZE 30
#endif

#ifdef HAVE_SDL_TTF
#ifdef SDL_PLATFORM_MACOS
#define DEFAULT_FONT "/System/Library/Fonts/华文细黑.ttf"
#elif defined(SDL_PLATFORM_WIN32)
/* Some japanese font present on at least Windows 8.1. */
#define DEFAULT_FONT "C:\\Windows\\Fonts\\yugothic.ttf"
#else
#define DEFAULT_FONT "NoDefaultFont.ttf"
#endif
#else
#define DEFAULT_FONT "unifont-15.1.05.hex"
#endif
#define MAX_TEXT_LENGTH 256

#define CURSOR_BLINK_INTERVAL_MS    500

static SDLTest_CommonState *state;
static SDL_FRect textRect, markedRect;
static SDL_Color lineColor = { 0, 0, 0, 255 };
static SDL_Color backColor = { 255, 255, 255, 255 };
static SDL_Color textColor = { 0, 0, 0, 255 };
static char text[MAX_TEXT_LENGTH], markedText[MAX_TEXT_LENGTH];
static int cursor = 0;
static int cursor_length = 0;
static SDL_bool cursor_visible;
static Uint64 last_cursor_change;
static SDL_BlendMode highlight_mode;
static char **candidates;
static int num_candidates;
static int selected_candidate;
static SDL_bool horizontal_candidates;
#ifdef HAVE_SDL_TTF
static TTF_Font *font;
#else
#define UNIFONT_MAX_CODEPOINT     0x1ffff
#define UNIFONT_NUM_GLYPHS        0x20000
#define UNIFONT_REPLACEMENT       0xFFFD
/* Using 512x512 textures that are supported everywhere. */
#define UNIFONT_TEXTURE_WIDTH     512
#define UNIFONT_GLYPH_SIZE        16
#define UNIFONT_GLYPH_BORDER      1
#define UNIFONT_GLYPH_AREA        (UNIFONT_GLYPH_BORDER + UNIFONT_GLYPH_SIZE + UNIFONT_GLYPH_BORDER)
#define UNIFONT_GLYPHS_IN_ROW     (UNIFONT_TEXTURE_WIDTH / UNIFONT_GLYPH_AREA)
#define UNIFONT_GLYPHS_IN_TEXTURE (UNIFONT_GLYPHS_IN_ROW * UNIFONT_GLYPHS_IN_ROW)
#define UNIFONT_NUM_TEXTURES      ((UNIFONT_NUM_GLYPHS + UNIFONT_GLYPHS_IN_TEXTURE - 1) / UNIFONT_GLYPHS_IN_TEXTURE)
#define UNIFONT_TEXTURE_SIZE      (UNIFONT_TEXTURE_WIDTH * UNIFONT_TEXTURE_WIDTH * 4)
#define UNIFONT_TEXTURE_PITCH     (UNIFONT_TEXTURE_WIDTH * 4)
#define UNIFONT_DRAW_SCALE        2.0f
static struct UnifontGlyph
{
    Uint8 width;
    Uint8 data[32];
} * unifontGlyph;
static SDL_Texture **unifontTexture;
static Uint8 unifontTextureLoaded[UNIFONT_NUM_TEXTURES] = { 0 };

/* Unifont loading code start */

static Uint8 dehex(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 255;
}

static Uint8 dehex2(char c1, char c2)
{
    return (dehex(c1) << 4) | dehex(c2);
}

static Uint8 validate_hex(const char *cp, size_t len, Uint32 *np)
{
    Uint32 n = 0;
    for (; len > 0; cp++, len--) {
        Uint8 c = dehex(*cp);
        if (c == 255) {
            return 0;
        }
        n = (n << 4) | c;
    }
    if (np) {
        *np = n;
    }
    return 1;
}

static int unifont_init(const char *fontname)
{
    Uint8 hexBuffer[65];
    Uint32 numGlyphs = 0;
    int lineNumber = 1;
    size_t bytesRead;
    SDL_IOStream *hexFile;
    const size_t unifontGlyphSize = UNIFONT_NUM_GLYPHS * sizeof(struct UnifontGlyph);
    const size_t unifontTextureSize = UNIFONT_NUM_TEXTURES * state->num_windows * sizeof(void *);
    char *filename;

    /* Allocate memory for the glyph data so the file can be closed after initialization. */
    unifontGlyph = (struct UnifontGlyph *)SDL_malloc(unifontGlyphSize);
    if (!unifontGlyph) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Failed to allocate %d KiB for glyph data.\n", (int)(unifontGlyphSize + 1023) / 1024);
        return -1;
    }
    SDL_memset(unifontGlyph, 0, unifontGlyphSize);

    /* Allocate memory for texture pointers for all renderers. */
    unifontTexture = (SDL_Texture **)SDL_malloc(unifontTextureSize);
    if (!unifontTexture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Failed to allocate %d KiB for texture pointer data.\n", (int)(unifontTextureSize + 1023) / 1024);
        return -1;
    }
    SDL_memset(unifontTexture, 0, unifontTextureSize);

    filename = GetResourceFilename(NULL, fontname);
    if (!filename) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory\n");
        return -1;
    }
    hexFile = SDL_IOFromFile(filename, "rb");
    SDL_free(filename);
    if (!hexFile) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Failed to open font file: %s\n", fontname);
        return -1;
    }

    /* Read all the glyph data into memory to make it accessible later when textures are created. */
    do {
        int i, codepointHexSize;
        size_t bytesOverread;
        Uint8 glyphWidth;
        Uint32 codepoint;

        bytesRead = SDL_ReadIO(hexFile, hexBuffer, 9);
        if (numGlyphs > 0 && bytesRead == 0) {
            break; /* EOF */
        }
        if ((numGlyphs == 0 && bytesRead == 0) || (numGlyphs > 0 && bytesRead < 9)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Unexpected end of hex file.\n");
            return -1;
        }

        /* Looking for the colon that separates the codepoint and glyph data at position 2, 4, 6 and 8. */
        if (hexBuffer[2] == ':') {
            codepointHexSize = 2;
        } else if (hexBuffer[4] == ':') {
            codepointHexSize = 4;
        } else if (hexBuffer[6] == ':') {
            codepointHexSize = 6;
        } else if (hexBuffer[8] == ':') {
            codepointHexSize = 8;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Could not find codepoint and glyph data separator symbol in hex file on line %d.\n", lineNumber);
            return -1;
        }

        if (!validate_hex((const char *)hexBuffer, codepointHexSize, &codepoint)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Malformed hexadecimal number in hex file on line %d.\n", lineNumber);
            return -1;
        }
        if (codepoint > UNIFONT_MAX_CODEPOINT) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "unifont: Codepoint on line %d exceeded limit of 0x%x.\n", lineNumber, UNIFONT_MAX_CODEPOINT);
        }

        /* If there was glyph data read in the last file read, move it to the front of the buffer. */
        bytesOverread = 8 - codepointHexSize;
        if (codepointHexSize < 8) {
            SDL_memmove(hexBuffer, hexBuffer + codepointHexSize + 1, bytesOverread);
        }
        bytesRead = SDL_ReadIO(hexFile, hexBuffer + bytesOverread, 33 - bytesOverread);

        if (bytesRead < (33 - bytesOverread)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Unexpected end of hex file.\n");
            return -1;
        }
        if (hexBuffer[32] == '\n') {
            glyphWidth = 8;
        } else {
            glyphWidth = 16;
            bytesRead = SDL_ReadIO(hexFile, hexBuffer + 33, 32);
            if (bytesRead < 32) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Unexpected end of hex file.\n");
                return -1;
            }
        }

        if (!validate_hex((const char *)hexBuffer, glyphWidth * 4, NULL)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Malformed hexadecimal glyph data in hex file on line %d.\n", lineNumber);
            return -1;
        }

        if (codepoint <= UNIFONT_MAX_CODEPOINT) {
            if (unifontGlyph[codepoint].width > 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "unifont: Ignoring duplicate codepoint 0x%08" SDL_PRIx32 " in hex file on line %d.\n", codepoint, lineNumber);
            } else {
                unifontGlyph[codepoint].width = glyphWidth;
                /* Pack the hex data into a more compact form. */
                for (i = 0; i < glyphWidth * 2; i++) {
                    unifontGlyph[codepoint].data[i] = dehex2(hexBuffer[i * 2], hexBuffer[i * 2 + 1]);
                }
                numGlyphs++;
            }
        }

        lineNumber++;
    } while (bytesRead > 0);

    SDL_CloseIO(hexFile);
    SDL_Log("unifont: Loaded %" SDL_PRIu32 " glyphs.\n", numGlyphs);
    return 0;
}

static void unifont_make_rgba(const Uint8 *src, Uint8 *dst, Uint8 width)
{
    int i, j;
    Uint8 *row = dst;

    for (i = 0; i < width * 2; i++) {
        Uint8 data = src[i];
        for (j = 0; j < 8; j++) {
            if (data & 0x80) {
                row[0] = textColor.r;
                row[1] = textColor.g;
                row[2] = textColor.b;
                row[3] = textColor.a;
            } else {
                row[0] = 0;
                row[1] = 0;
                row[2] = 0;
                row[3] = 0;
            }
            data <<= 1;
            row += 4;
        }

        if (width == 8 || (width == 16 && i % 2 == 1)) {
            dst += UNIFONT_TEXTURE_PITCH;
            row = dst;
        }
    }
}

static int unifont_load_texture(Uint32 textureID)
{
    int i;
    Uint8 *textureRGBA;

    if (textureID >= UNIFONT_NUM_TEXTURES) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Tried to load out of range texture %" SDL_PRIu32 "\n", textureID);
        return -1;
    }

    textureRGBA = (Uint8 *)SDL_malloc(UNIFONT_TEXTURE_SIZE);
    if (!textureRGBA) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Failed to allocate %d MiB for a texture.\n", UNIFONT_TEXTURE_SIZE / 1024 / 1024);
        return -1;
    }
    SDL_memset(textureRGBA, 0, UNIFONT_TEXTURE_SIZE);

    /* Copy the glyphs into memory in RGBA format. */
    for (i = 0; i < UNIFONT_GLYPHS_IN_TEXTURE; i++) {
        Uint32 codepoint = UNIFONT_GLYPHS_IN_TEXTURE * textureID + i;
        if (unifontGlyph[codepoint].width > 0) {
            const Uint32 cInTex = codepoint % UNIFONT_GLYPHS_IN_TEXTURE;
            const size_t offset = ((size_t)cInTex / UNIFONT_GLYPHS_IN_ROW) * UNIFONT_TEXTURE_PITCH * UNIFONT_GLYPH_AREA + (cInTex % UNIFONT_GLYPHS_IN_ROW) * UNIFONT_GLYPH_AREA * 4;
            unifont_make_rgba(unifontGlyph[codepoint].data, textureRGBA + offset, unifontGlyph[codepoint].width);
        }
    }

    /* Create textures and upload the RGBA data from above. */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_Texture *tex = unifontTexture[UNIFONT_NUM_TEXTURES * i + textureID];
        if (state->windows[i] == NULL || renderer == NULL || tex != NULL) {
            continue;
        }
        tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, UNIFONT_TEXTURE_WIDTH, UNIFONT_TEXTURE_WIDTH);
        if (tex == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "unifont: Failed to create texture %" SDL_PRIu32 " for renderer %d.\n", textureID, i);
            return -1;
        }
        unifontTexture[UNIFONT_NUM_TEXTURES * i + textureID] = tex;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        if (SDL_UpdateTexture(tex, NULL, textureRGBA, UNIFONT_TEXTURE_PITCH) != 0) {
            SDL_Log("unifont error: Failed to update texture %" SDL_PRIu32 " data for renderer %d.\n", textureID, i);
        }
    }

    SDL_free(textureRGBA);
    unifontTextureLoaded[textureID] = 1;
    return 0;
}

static int unifont_glyph_width(Uint32 codepoint)
{
    if (codepoint > UNIFONT_MAX_CODEPOINT ||
        unifontGlyph[codepoint].width == 0) {
        codepoint = UNIFONT_REPLACEMENT;
    }
    return unifontGlyph[codepoint].width;
}

static int unifont_draw_glyph(Uint32 codepoint, int rendererID, SDL_FRect *dst)
{
    SDL_Texture *texture;
    Uint32 textureID;
    SDL_FRect srcrect;
    srcrect.w = srcrect.h = (float)UNIFONT_GLYPH_SIZE;

    if (codepoint > UNIFONT_MAX_CODEPOINT ||
        unifontGlyph[codepoint].width == 0) {
        codepoint = UNIFONT_REPLACEMENT;
    }

    textureID = codepoint / UNIFONT_GLYPHS_IN_TEXTURE;
    if (!unifontTextureLoaded[textureID]) {
        if (unifont_load_texture(textureID) < 0) {
            return 0;
        }
    }
    texture = unifontTexture[UNIFONT_NUM_TEXTURES * rendererID + textureID];
    if (texture) {
        const Uint32 cInTex = codepoint % UNIFONT_GLYPHS_IN_TEXTURE;
        srcrect.x = (float)(cInTex % UNIFONT_GLYPHS_IN_ROW * UNIFONT_GLYPH_AREA);
        srcrect.y = (float)(cInTex / UNIFONT_GLYPHS_IN_ROW * UNIFONT_GLYPH_AREA);
        SDL_RenderTexture(state->renderers[rendererID], texture, &srcrect, dst);
    }
    return unifontGlyph[codepoint].width;
}

static void unifont_cleanup(void)
{
    int i, j;
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (state->windows[i] == NULL || !renderer) {
            continue;
        }
        for (j = 0; j < UNIFONT_NUM_TEXTURES; j++) {
            SDL_Texture *tex = unifontTexture[UNIFONT_NUM_TEXTURES * i + j];
            if (tex) {
                SDL_DestroyTexture(tex);
            }
        }
    }

    for (j = 0; j < UNIFONT_NUM_TEXTURES; j++) {
        unifontTextureLoaded[j] = 0;
    }

    SDL_free(unifontTexture);
    SDL_free(unifontGlyph);
}

/* Unifont code end */
#endif

static size_t utf8_length(unsigned char c)
{
    c = (unsigned char)(0xff & c);
    if (c < 0x80) {
        return 1;
    } else if ((c >> 5) == 0x6) {
        return 2;
    } else if ((c >> 4) == 0xe) {
        return 3;
    } else if ((c >> 3) == 0x1e) {
        return 4;
    }
    return 0;
}

#ifdef HAVE_SDL_TTF
static char *utf8_next(char *p)
{
    size_t len = utf8_length(*p);
    size_t i = 0;
    if (!len) {
        return 0;
    }

    for (; i < len; ++i) {
        ++p;
        if (!*p) {
            return 0;
        }
    }
    return p;
}


static char *utf8_advance(char *p, size_t distance)
{
    size_t i = 0;
    for (; i < distance && p; ++i) {
        p = utf8_next(p);
    }
    return p;
}
#endif

static Uint32 utf8_decode(char *p, size_t len)
{
    Uint32 codepoint = 0;
    size_t i = 0;
    if (!len) {
        return 0;
    }

    for (; i < len; ++i) {
        if (i == 0) {
            codepoint = (0xff >> len) & *p;
        } else {
            codepoint <<= 6;
            codepoint |= 0x3f & *p;
        }
        if (!*p) {
            return 0;
        }
        p++;
    }

    return codepoint;
}

static void InitInput(void)
{
    int i;

    /* Prepare a rect for text input */
    textRect.x = textRect.y = 100.0f;
    textRect.w = DEFAULT_WINDOW_WIDTH - 2 * textRect.x;
    textRect.h = 50.0f;

    text[0] = 0;
    markedRect = textRect;
    markedText[0] = 0;

    for (i = 0; i < state->num_windows; ++i) {
        SDL_StartTextInput(state->windows[i]);
    }
}


static void ClearCandidates(void)
{
    int i;

    for (i = 0; i < num_candidates; ++i) {
        SDL_free(candidates[i]);
    }
    SDL_free(candidates);
    candidates = NULL;
    num_candidates = 0;
}

static void SaveCandidates(SDL_Event *event)
{
    int i;

    ClearCandidates();

    num_candidates = event->edit_candidates.num_candidates;
    if (num_candidates > 0) {
        candidates = (char **)SDL_malloc(num_candidates * sizeof(*candidates));
        if (!candidates) {
            num_candidates = 0;
            return;
        }
        for (i = 0; i < num_candidates; ++i) {
            candidates[i] = SDL_strdup(event->edit_candidates.candidates[i]);
        }
        selected_candidate = event->edit_candidates.selected_candidate;
        horizontal_candidates = event->edit_candidates.horizontal;
    }
}

static void DrawCandidates(int rendererID, SDL_FRect *cursorRect)
{
    SDL_Renderer *renderer = state->renderers[rendererID];
    int i;
    int output_w = 0, output_h = 0;
    float w = 0.0f, h = 0.0f;
    SDL_FRect candidatesRect, dstRect, underlineRect;

    if (num_candidates == 0) {
        return;
    }

    /* Calculate the size of the candidate list */
    for (i = 0; i < num_candidates; ++i) {
        if (!candidates[i]) {
            continue;
        }

#ifdef HAVE_SDL_TTF
        /* FIXME */
#else
        if (horizontal_candidates) {
            char *utext = candidates[i];
            Uint32 codepoint;
            size_t len;
            float advance = 0.0f;

            if (i > 0) {
                advance += unifont_glyph_width(' ') * UNIFONT_DRAW_SCALE;
            }
            while ((codepoint = utf8_decode(utext, len = utf8_length(*utext))) != 0) {
                advance += unifont_glyph_width(codepoint) * UNIFONT_DRAW_SCALE;
                utext += len;
            }
            w += advance;
            h = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        } else {
            char *utext = candidates[i];
            Uint32 codepoint;
            size_t len;
            float advance = 0.0f;

            while ((codepoint = utf8_decode(utext, len = utf8_length(*utext))) != 0) {
                advance += unifont_glyph_width(codepoint) * UNIFONT_DRAW_SCALE;
                utext += len;
            }
            w = SDL_max(w, advance);
            if (i > 0) {
                h += 2.0f;
            }
            h += UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        }
#endif
    }

    /* Position the candidate window */
    SDL_GetCurrentRenderOutputSize(renderer, &output_w, &output_h);
    candidatesRect.x = cursorRect->x;
    candidatesRect.y = cursorRect->y + cursorRect->h + 2.0f;
    candidatesRect.w = 1.0f + 2.0f + w + 2.0f + 1.0f;
    candidatesRect.h = 1.0f + 2.0f + h + 2.0f + 1.0f;
    if ((candidatesRect.x + candidatesRect.w) > output_w) {
        candidatesRect.x = (output_w - candidatesRect.w);
        if (candidatesRect.x < 0.0f) {
            candidatesRect.x = 0.0f;
        }
    }

    /* Draw the candidate background */
    SDL_SetRenderDrawColor(renderer, 0xAA, 0xAA, 0xAA, 0xFF);
    SDL_RenderFillRect(renderer, &candidatesRect);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderRect(renderer, &candidatesRect);

    /* Draw the candidates */
    dstRect.x = candidatesRect.x + 3.0f;
    dstRect.y = candidatesRect.y + 3.0f;
    for (i = 0; i < num_candidates; ++i) {
        if (!candidates[i]) {
            continue;
        }

#ifdef HAVE_SDL_TTF
        /* FIXME */
#else
        dstRect.w = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        dstRect.h = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;

        if (horizontal_candidates) {
            char *utext = candidates[i];
            Uint32 codepoint;
            size_t len;
            float start;

            if (i > 0) {
                dstRect.x += unifont_draw_glyph(' ', rendererID, &dstRect) * UNIFONT_DRAW_SCALE;
            }

            start = dstRect.x + 2 * unifont_glyph_width(' ') * UNIFONT_DRAW_SCALE;
            while ((codepoint = utf8_decode(utext, len = utf8_length(*utext))) != 0) {
                dstRect.x += unifont_draw_glyph(codepoint, rendererID, &dstRect) * UNIFONT_DRAW_SCALE;
                utext += len;
            }

            if (i == selected_candidate) {
                underlineRect.x = start;
                underlineRect.y = dstRect.y + dstRect.h - 2;
                underlineRect.h = 2;
                underlineRect.w = dstRect.x - start;

                SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
                SDL_RenderFillRect(renderer, &underlineRect);
            }
        } else {
            char *utext = candidates[i];
            Uint32 codepoint;
            size_t len;
            float start;

            dstRect.x = candidatesRect.x + 3.0f;

            start = dstRect.x + 2 * unifont_glyph_width(' ') * UNIFONT_DRAW_SCALE;
            while ((codepoint = utf8_decode(utext, len = utf8_length(*utext))) != 0) {
                dstRect.x += unifont_draw_glyph(codepoint, rendererID, &dstRect) * UNIFONT_DRAW_SCALE;
                utext += len;
            }

            if (i == selected_candidate) {
                underlineRect.x = start;
                underlineRect.y = dstRect.y + dstRect.h - 2;
                underlineRect.h = 2;
                underlineRect.w = dstRect.x - start;

                SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
                SDL_RenderFillRect(renderer, &underlineRect);
            }

            if (i > 0) {
                dstRect.y += 2.0f;
            }
            dstRect.y += dstRect.h;
        }
#endif
    }
}

static void UpdateTextInputArea(SDL_Window *window, const SDL_FRect *cursorRect)
{
    SDL_Rect rect;
    int cursor_offset = (int)(cursorRect->x - textRect.x);

    rect.x = (int)textRect.x;
    rect.y = (int)textRect.y;
    rect.w = (int)textRect.w;
    rect.h = (int)textRect.h;
    SDL_SetTextInputArea(window, &rect, cursor_offset);
}

static void CleanupVideo(void)
{
    SDL_StopTextInput(state->windows[0]);
    ClearCandidates();
#ifdef HAVE_SDL_TTF
    TTF_CloseFont(font);
    TTF_Quit();
#else
    unifont_cleanup();
#endif
}

static void RedrawWindow(int rendererID)
{
    SDL_Renderer *renderer = state->renderers[rendererID];
    SDL_FRect drawnTextRect, cursorRect, underlineRect;

    SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
    SDL_RenderFillRect(renderer, &textRect);

    /* Initialize the drawn text rectangle for the cursor */
    drawnTextRect.x = textRect.x;
    drawnTextRect.y = textRect.y + (textRect.h - UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE) / 2;
    drawnTextRect.w = 0.0f;
    drawnTextRect.h = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;

    if (*text) {
#ifdef HAVE_SDL_TTF
        SDL_Surface *textSur = TTF_RenderUTF8_Blended(font, text, textColor);
        SDL_Texture *texture;

        /* Vertically center text */
        drawnTextRect.y = textRect.y + (textRect.h - textSur->h) / 2;
        drawnTextRect.w = textSur->w;
        drawnTextRect.h = textSur->h;

        texture = SDL_CreateTextureFromSurface(renderer, textSur);
        SDL_DestroySurface(textSur);

        SDL_RenderTexture(renderer, texture, NULL, &drawnTextRect);
        SDL_DestroyTexture(texture);
#else
        char *utext = text;
        Uint32 codepoint;
        size_t len;
        SDL_FRect dstrect;

        dstrect.x = textRect.x;
        dstrect.y = textRect.y + (textRect.h - UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE) / 2;
        dstrect.w = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        dstrect.h = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        drawnTextRect.y = dstrect.y;
        drawnTextRect.h = dstrect.h;

        while ((codepoint = utf8_decode(utext, len = utf8_length(*utext))) != 0) {
            float advance = unifont_draw_glyph(codepoint, rendererID, &dstrect) * UNIFONT_DRAW_SCALE;
            dstrect.x += advance;
            drawnTextRect.w += advance;
            utext += len;
        }
#endif
    }

    /* The marked text rectangle is the text area that hasn't been filled by committed text */
    markedRect.x = textRect.x + drawnTextRect.w;
    markedRect.w = textRect.w - drawnTextRect.w;
    if (markedRect.w < 0) {
        /* Stop text input because we cannot hold any more characters */
        SDL_StopTextInput(state->windows[0]);
        return;
    } else {
        SDL_StartTextInput(state->windows[0]);
    }

    /* Update the drawn text rectangle for composition text, after the committed text */
    drawnTextRect.x += drawnTextRect.w;
    drawnTextRect.w = 0;

    /* Set the cursor to the new location, we'll update it as we go, below */
    cursorRect = drawnTextRect;
    cursorRect.w = 2;
    cursorRect.h = drawnTextRect.h;

    if (markedText[0]) {
#ifdef HAVE_SDL_TTF
        SDL_Surface *textSur;
        SDL_Texture *texture;
        if (cursor) {
            char *p = utf8_advance(markedText, cursor);
            char c = 0;
            if (!p) {
                p = &markedText[SDL_strlen(markedText)];
            }

            c = *p;
            *p = 0;
            TTF_SizeUTF8(font, markedText, &drawnTextRect.w, NULL);
            cursorRect.x += drawnTextRect.w;
            *p = c;
        }
        textSur = TTF_RenderUTF8_Blended(font, markedText, textColor);
        /* Vertically center text */
        drawnTextRect.y = textRect.y + (textRect.h - textSur->h) / 2;
        drawnTextRect.w = textSur->w;
        drawnTextRect.h = textSur->h;

        texture = SDL_CreateTextureFromSurface(renderer, textSur);
        SDL_DestroySurface(textSur);

        SDL_RenderTexture(renderer, texture, NULL, &drawnTextRect);
        SDL_DestroyTexture(texture);

        if (cursor_length > 0) {
            /* FIXME: Need to measure text extents */
            cursorRect.w = cursor_length * UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        }
#else
        int i = 0;
        char *utext = markedText;
        Uint32 codepoint;
        size_t len;
        SDL_FRect dstrect;

        dstrect.x = drawnTextRect.x;
        dstrect.y = textRect.y + (textRect.h - UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE) / 2;
        dstrect.w = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        dstrect.h = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        drawnTextRect.y = dstrect.y;
        drawnTextRect.h = dstrect.h;

        while ((codepoint = utf8_decode(utext, len = utf8_length(*utext))) != 0) {
            float advance = unifont_draw_glyph(codepoint, rendererID, &dstrect) * UNIFONT_DRAW_SCALE;
            dstrect.x += advance;
            drawnTextRect.w += advance;
            if (i < cursor) {
                cursorRect.x += advance;
            }
            i++;
            utext += len;
        }

        if (cursor_length > 0) {
            cursorRect.w = cursor_length * UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        }
#endif

        cursorRect.y = drawnTextRect.y;
        cursorRect.h = drawnTextRect.h;

        underlineRect = markedRect;
        underlineRect.y = drawnTextRect.y + drawnTextRect.h - 2;
        underlineRect.h = 2;
        underlineRect.w = drawnTextRect.w;

        SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
        SDL_RenderFillRect(renderer, &underlineRect);
    }

    /* Draw the cursor */
    Uint64 now = SDL_GetTicks();
    if ((now - last_cursor_change) >= CURSOR_BLINK_INTERVAL_MS) {
        cursor_visible = !cursor_visible;
        last_cursor_change = now;
    }
    if (cursor_length > 0) {
        /* We'll show a highlight */
        SDL_SetRenderDrawBlendMode(renderer, highlight_mode);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &cursorRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    } else if (cursor_visible) {
        SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
        SDL_RenderFillRect(renderer, &cursorRect);
    }

    /* Draw the candidates */
    DrawCandidates(rendererID, &cursorRect);

    /* Update the area used to draw composition UI */
    UpdateTextInputArea(state->windows[0], &cursorRect);
}

static void Redraw(void)
{
    int i;
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (state->windows[i] == NULL) {
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        /* Sending in the window id to let the font renderers know which one we're working with. */
        RedrawWindow(i);

        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char *argv[])
{
    SDL_bool render_composition = SDL_FALSE;
    SDL_bool render_candidates = SDL_FALSE;
    int i, done;
    SDL_Event event;
    char *fontname = NULL;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (SDL_strcmp(argv[i], "--font") == 0) {
            if (*argv[i + 1]) {
                fontname = argv[i + 1];
                consumed = 2;
            }
        } else if (SDL_strcmp(argv[i], "--render-composition") == 0) {
            render_composition = SDL_TRUE;
            consumed = 1;
        } else if (SDL_strcmp(argv[i], "--render-candidates") == 0) {
            render_candidates = SDL_TRUE;
            consumed = 1;
        }
        if (consumed <= 0) {
            static const char *options[] = { "[--font fontfile] [--render-composition] [--render-candidates]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (render_composition && render_candidates) {
        SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition,candidates");
    } else if (render_composition) {
        SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
    } else if (render_candidates) {
        SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "candidates");
    }

    if (!SDLTest_CommonInit(state)) {
        return 2;
    }

    fontname = GetResourceFilename(fontname, DEFAULT_FONT);

#ifdef HAVE_SDL_TTF
    /* Initialize fonts */
    TTF_Init();

    font = TTF_OpenFont(fontname, DEFAULT_PTSIZE);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find font: %s\n", TTF_GetError());
        return -1;
    }
#else
    if (unifont_init(fontname) < 0) {
        return -1;
    }
#endif

    SDL_Log("Using font: %s\n", fontname);

    InitInput();
    /* Create the windows and initialize the renderers */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }
    highlight_mode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR,
                                                SDL_BLENDFACTOR_ZERO,
                                                SDL_BLENDOPERATION_ADD,
                                                SDL_BLENDFACTOR_ZERO,
                                                SDL_BLENDFACTOR_ONE,
                                                SDL_BLENDOPERATION_ADD);

    /* Main render loop */
    done = 0;
    while (!done) {
        /* Check for events */
        while (SDL_PollEvent(&event)) {
            SDLTest_CommonEvent(state, &event, &done);
            switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                switch (event.key.key) {
                case SDLK_RETURN:
                    text[0] = 0x00;
                    break;
                case SDLK_BACKSPACE:
                    /* Only delete text if not in editing mode. */
                    if (!markedText[0]) {
                        size_t textlen = SDL_strlen(text);

                        do {
                            if (textlen == 0) {
                                break;
                            }
                            if (!(text[textlen - 1] & 0x80)) {
                                /* One byte */
                                text[textlen - 1] = 0x00;
                                break;
                            }
                            if ((text[textlen - 1] & 0xC0) == 0x80) {
                                /* Byte from the multibyte sequence */
                                text[textlen - 1] = 0x00;
                                textlen--;
                            }
                            if ((text[textlen - 1] & 0xC0) == 0xC0) {
                                /* First byte of multibyte sequence */
                                text[textlen - 1] = 0x00;
                                break;
                            }
                        } while (1);
                    }
                    break;
                default:
                    break;
                }

                if (done) {
                    break;
                }

                SDL_Log("Keyboard: scancode 0x%08X = %s, keycode 0x%08" SDL_PRIX32 " = %s\n",
                        event.key.scancode,
                        SDL_GetScancodeName(event.key.scancode),
                        SDL_static_cast(Uint32, event.key.key),
                        SDL_GetKeyName(event.key.key));
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (event.text.text[0] == '\0' || event.text.text[0] == '\n' || markedRect.w < 0) {
                    break;
                }

                SDL_Log("Keyboard: text input \"%s\"\n", event.text.text);

                if (SDL_strlen(text) + SDL_strlen(event.text.text) < sizeof(text)) {
                    SDL_strlcat(text, event.text.text, sizeof(text));
                }

                SDL_Log("text inputted: %s\n", text);

                /* After text inputted, we can clear up markedText because it */
                /* is committed */
                markedText[0] = 0;
                break;

            case SDL_EVENT_TEXT_EDITING:
                SDL_Log("text editing \"%s\", selected range (%" SDL_PRIs32 ", %" SDL_PRIs32 ")\n",
                        event.edit.text, event.edit.start, event.edit.length);

                SDL_strlcpy(markedText, event.edit.text, sizeof(markedText));
                cursor = event.edit.start;
                cursor_length = event.edit.length;
                break;

            case SDL_EVENT_TEXT_EDITING_CANDIDATES:
                SDL_Log("text candidates:\n");
                for (i = 0; i < event.edit_candidates.num_candidates; ++i) {
                    SDL_Log("%c%s\n", i == event.edit_candidates.selected_candidate ? '>' : ' ', event.edit_candidates.candidates[i]);
                }

                ClearCandidates();
                SaveCandidates(&event);
                break;

            default:
                break;
            }
        }

        Redraw();
    }
    SDL_free(fontname);
    CleanupVideo();
    SDLTest_CommonQuit(state);
    return 0;
}
