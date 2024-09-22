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
#include <SDL3/SDL_test_font.h>
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

#define WINDOW_WIDTH    640
#define WINDOW_HEIGHT   480

#define MARGIN 32.0f
#define LINE_HEIGHT (FONT_CHARACTER_SIZE + 4.0f)
#define CURSOR_BLINK_INTERVAL_MS    500

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int rendererID;
    bool settings_visible;
    SDL_Texture *settings_icon;
    SDL_FRect settings_rect;
    SDL_PropertiesID text_settings;
    SDL_FRect textRect;
    SDL_FRect markedRect;
    char text[MAX_TEXT_LENGTH];
    char markedText[MAX_TEXT_LENGTH];
    int cursor;
    int cursor_length;
    bool cursor_visible;
    Uint64 last_cursor_change;
    char **candidates;
    int num_candidates;
    int selected_candidate;
    bool horizontal_candidates;
} WindowState;

static SDLTest_CommonState *state;
static WindowState *windowstate;
static const SDL_Color lineColor = { 0, 0, 0, 255 };
static const SDL_Color backColor = { 255, 255, 255, 255 };
static const SDL_Color textColor = { 0, 0, 0, 255 };
static SDL_BlendMode highlight_mode;

static const struct
{
    const char *label;
    const char *setting;
    int value;
} settings[] = {
    { "Text",                   SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT },
    { "Name",                   SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT_NAME },
    { "E-mail",                 SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT_EMAIL },
    { "Username",               SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT_USERNAME },
    { "Password (hidden)",      SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN },
    { "Password (visible)",     SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE },
    { "Number",                 SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_NUMBER },
    { "Numeric PIN (hidden)",   SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_HIDDEN },
    { "Numeric PIN (visible)",  SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_VISIBLE },
    { "",                       NULL },
    { "No capitalization",      SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, SDL_CAPITALIZE_NONE },
    { "Capitalize sentences",   SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, SDL_CAPITALIZE_SENTENCES },
    { "Capitalize words",       SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, SDL_CAPITALIZE_WORDS },
    { "All caps",               SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, SDL_CAPITALIZE_LETTERS },
    { "",                       NULL },
    { "Auto-correct OFF",       SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, false },
    { "Auto-correct ON",        SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, true },
    { "Multiline OFF",          SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN, false },
    { "Multiline ON",           SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN, true }
};

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
        if (!SDL_UpdateTexture(tex, NULL, textureRGBA, UNIFONT_TEXTURE_PITCH)) {
            SDL_Log("unifont error: Failed to update texture %" SDL_PRIu32 " data for renderer %d.\n", textureID, i);
        }
    }

    SDL_free(textureRGBA);
    unifontTextureLoaded[textureID] = 1;
    return -1;
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

static Uint32 utf8_decode(const char *p, size_t len)
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

static WindowState *GetWindowStateForWindowID(SDL_WindowID windowID)
{
    int i;
    SDL_Window *window = SDL_GetWindowFromID(windowID);

    for (i = 0; i < state->num_windows; ++i) {
        if (windowstate[i].window == window) {
            return &windowstate[i];
        }
    }
    return NULL;
}

static void InitInput(WindowState *ctx)
{
    /* Prepare a rect for text input */
    ctx->textRect.x = ctx->textRect.y = 100.0f;
    ctx->textRect.w = DEFAULT_WINDOW_WIDTH - 2 * ctx->textRect.x;
    ctx->textRect.h = 50.0f;
    ctx->markedRect = ctx->textRect;

    ctx->text_settings = SDL_CreateProperties();

    SDL_StartTextInputWithProperties(ctx->window, ctx->text_settings);
}


static void ClearCandidates(WindowState *ctx)
{
    int i;

    for (i = 0; i < ctx->num_candidates; ++i) {
        SDL_free(ctx->candidates[i]);
    }
    SDL_free(ctx->candidates);
    ctx->candidates = NULL;
    ctx->num_candidates = 0;
}

static void SaveCandidates(WindowState *ctx, SDL_Event *event)
{
    int i;

    ClearCandidates(ctx);

    ctx->num_candidates = event->edit_candidates.num_candidates;
    if (ctx->num_candidates > 0) {
        ctx->candidates = (char **)SDL_malloc(ctx->num_candidates * sizeof(*ctx->candidates));
        if (!ctx->candidates) {
            ctx->num_candidates = 0;
            return;
        }
        for (i = 0; i < ctx->num_candidates; ++i) {
            ctx->candidates[i] = SDL_strdup(event->edit_candidates.candidates[i]);
        }
        ctx->selected_candidate = event->edit_candidates.selected_candidate;
        ctx->horizontal_candidates = event->edit_candidates.horizontal;
    }
}

static void DrawCandidates(WindowState *ctx, SDL_FRect *cursorRect)
{
    SDL_Renderer *renderer = ctx->renderer;
    int rendererID = ctx->rendererID;
    int i;
    int output_w = 0, output_h = 0;
    float w = 0.0f, h = 0.0f;
    SDL_FRect candidatesRect, dstRect, underlineRect;

    if (ctx->num_candidates == 0) {
        return;
    }

    /* Calculate the size of the candidate list */
    for (i = 0; i < ctx->num_candidates; ++i) {
        if (!ctx->candidates[i]) {
            continue;
        }

#ifdef HAVE_SDL_TTF
        /* FIXME */
#else
        if (ctx->horizontal_candidates) {
            const char *utext = ctx->candidates[i];
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
            const char *utext = ctx->candidates[i];
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
    for (i = 0; i < ctx->num_candidates; ++i) {
        if (!ctx->candidates[i]) {
            continue;
        }

#ifdef HAVE_SDL_TTF
        /* FIXME */
#else
        dstRect.w = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        dstRect.h = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;

        if (ctx->horizontal_candidates) {
            const char *utext = ctx->candidates[i];
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

            if (i == ctx->selected_candidate) {
                underlineRect.x = start;
                underlineRect.y = dstRect.y + dstRect.h - 2;
                underlineRect.h = 2;
                underlineRect.w = dstRect.x - start;

                SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
                SDL_RenderFillRect(renderer, &underlineRect);
            }
        } else {
            const char *utext = ctx->candidates[i];
            Uint32 codepoint;
            size_t len;
            float start;

            dstRect.x = candidatesRect.x + 3.0f;

            start = dstRect.x + 2 * unifont_glyph_width(' ') * UNIFONT_DRAW_SCALE;
            while ((codepoint = utf8_decode(utext, len = utf8_length(*utext))) != 0) {
                dstRect.x += unifont_draw_glyph(codepoint, rendererID, &dstRect) * UNIFONT_DRAW_SCALE;
                utext += len;
            }

            if (i == ctx->selected_candidate) {
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

static void UpdateTextInputArea(WindowState *ctx, const SDL_FRect *cursorRect)
{
    SDL_Rect rect;
    int cursor_offset = (int)(cursorRect->x - ctx->textRect.x);

    rect.x = (int)ctx->textRect.x;
    rect.y = (int)ctx->textRect.y;
    rect.w = (int)ctx->textRect.w;
    rect.h = (int)ctx->textRect.h;
    SDL_SetTextInputArea(ctx->window, &rect, cursor_offset);
}

static void CleanupVideo(void)
{
    int i;

    for (i = 0; i < state->num_windows; ++i) {
        WindowState *ctx = &windowstate[i];

        SDL_StopTextInput(ctx->window);
        ClearCandidates(ctx);
        SDL_DestroyProperties(ctx->text_settings);
    }
#ifdef HAVE_SDL_TTF
    TTF_CloseFont(font);
    TTF_Quit();
#else
    unifont_cleanup();
#endif
}

static void DrawSettingsButton(WindowState *ctx)
{
    SDL_Renderer *renderer = ctx->renderer;

    SDL_RenderTexture(renderer, ctx->settings_icon, NULL, &ctx->settings_rect);
}

static void ToggleSettings(WindowState *ctx)
{
    if (ctx->settings_visible) {
        ctx->settings_visible = false;
        SDL_StartTextInputWithProperties(ctx->window, ctx->text_settings);
    } else {
        SDL_StopTextInput(ctx->window);
        ctx->settings_visible = true;
    }
}

static int GetDefaultSetting(SDL_PropertiesID props, const char *setting)
{
    if (SDL_strcmp(setting, SDL_PROP_TEXTINPUT_TYPE_NUMBER) == 0) {
        return SDL_TEXTINPUT_TYPE_TEXT;
    }

    if (SDL_strcmp(setting, SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER) == 0) {
        switch (SDL_GetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT)) {
        case SDL_TEXTINPUT_TYPE_TEXT:
            return SDL_CAPITALIZE_SENTENCES;
        case SDL_TEXTINPUT_TYPE_TEXT_NAME:
            return SDL_CAPITALIZE_WORDS;
        default:
            return SDL_CAPITALIZE_NONE;
        }
    }

    if (SDL_strcmp(setting, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN) == 0) {
        return true;
    }

    if (SDL_strcmp(setting, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN) == 0) {
        return true;
    }

    SDL_assert(!"Unknown setting");
    return 0;
}

static void DrawSettings(WindowState *ctx)
{
    SDL_Renderer *renderer = ctx->renderer;
    SDL_FRect checkbox;
    int i;

    checkbox.x = MARGIN;
    checkbox.y = MARGIN;
    checkbox.w = (float)FONT_CHARACTER_SIZE;
    checkbox.h = (float)FONT_CHARACTER_SIZE;

    for (i = 0; i < SDL_arraysize(settings); ++i) {
        if (settings[i].setting) {
            int value = (int)SDL_GetNumberProperty(ctx->text_settings, settings[i].setting, GetDefaultSetting(ctx->text_settings, settings[i].setting));
            if (value == settings[i].value) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_RenderFillRect(renderer, &checkbox);
            }
            SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
            SDL_RenderRect(renderer, &checkbox);
            SDLTest_DrawString(renderer, checkbox.x + checkbox.w + 8.0f, checkbox.y, settings[i].label);
        }
        checkbox.y += LINE_HEIGHT;
    }
}

static void ClickSettings(WindowState *ctx, float x, float y)
{
    int setting = (int)SDL_floorf((y - MARGIN) / LINE_HEIGHT);
    if (setting >= 0 && setting < SDL_arraysize(settings)) {
        SDL_SetNumberProperty(ctx->text_settings, settings[setting].setting, settings[setting].value);
    }
}

static void RedrawWindow(WindowState *ctx)
{
    SDL_Renderer *renderer = ctx->renderer;
    int rendererID = ctx->rendererID;
    SDL_FRect drawnTextRect, cursorRect, underlineRect;
    char text[MAX_TEXT_LENGTH];

    DrawSettingsButton(ctx);

    if (ctx->settings_visible) {
        DrawSettings(ctx);
        return;
    }

    /* Hide the text if it's a password */
    switch ((SDL_TextInputType)SDL_GetNumberProperty(ctx->text_settings, SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT)) {
    case SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN:
    case SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_HIDDEN: {
        size_t len = SDL_utf8strlen(ctx->text);
        SDL_memset(text, '*', len);
        text[len] = '\0';
        break;
    }
    default:
        SDL_strlcpy(text, ctx->text, sizeof(text));
        break;
    }

    SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
    SDL_RenderFillRect(renderer, &ctx->textRect);

    /* Initialize the drawn text rectangle for the cursor */
    drawnTextRect.x = ctx->textRect.x;
    drawnTextRect.y = ctx->textRect.y + (ctx->textRect.h - UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE) / 2;
    drawnTextRect.w = 0.0f;
    drawnTextRect.h = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;

    if (text[0]) {
#ifdef HAVE_SDL_TTF
        SDL_Surface *textSur = TTF_RenderUTF8_Blended(font, text, textColor);
        SDL_Texture *texture;

        /* Vertically center text */
        drawnTextRect.y = ctx->textRect.y + (ctx->textRect.h - textSur->h) / 2;
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

        dstrect.x = ctx->textRect.x;
        dstrect.y = ctx->textRect.y + (ctx->textRect.h - UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE) / 2;
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
    ctx->markedRect.x = ctx->textRect.x + drawnTextRect.w;
    ctx->markedRect.w = ctx->textRect.w - drawnTextRect.w;

    /* Update the drawn text rectangle for composition text, after the committed text */
    drawnTextRect.x += drawnTextRect.w;
    drawnTextRect.w = 0;

    /* Set the cursor to the new location, we'll update it as we go, below */
    cursorRect = drawnTextRect;
    cursorRect.w = 2;
    cursorRect.h = drawnTextRect.h;

    if (ctx->markedText[0]) {
#ifdef HAVE_SDL_TTF
        SDL_Surface *textSur;
        SDL_Texture *texture;
        if (ctx->cursor) {
            char *p = utf8_advance(ctx->markedText, ctx->cursor);
            char c = 0;
            if (!p) {
                p = &ctx->markedText[SDL_strlen(ctx->markedText)];
            }

            c = *p;
            *p = 0;
            TTF_SizeUTF8(font, ctx->markedText, &drawnTextRect.w, NULL);
            cursorRect.x += drawnTextRect.w;
            *p = c;
        }
        textSur = TTF_RenderUTF8_Blended(font, ctx->markedText, textColor);
        /* Vertically center text */
        drawnTextRect.y = ctx->textRect.y + (ctx->textRect.h - textSur->h) / 2;
        drawnTextRect.w = textSur->w;
        drawnTextRect.h = textSur->h;

        texture = SDL_CreateTextureFromSurface(renderer, textSur);
        SDL_DestroySurface(textSur);

        SDL_RenderTexture(renderer, texture, NULL, &drawnTextRect);
        SDL_DestroyTexture(texture);

        if (ctx->cursor_length > 0) {
            /* FIXME: Need to measure text extents */
            cursorRect.w = ctx->cursor_length * UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        }
#else
        int i = 0;
        char *utext = ctx->markedText;
        Uint32 codepoint;
        size_t len;
        SDL_FRect dstrect;

        dstrect.x = drawnTextRect.x;
        dstrect.y = ctx->textRect.y + (ctx->textRect.h - UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE) / 2;
        dstrect.w = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        dstrect.h = UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        drawnTextRect.y = dstrect.y;
        drawnTextRect.h = dstrect.h;

        while ((codepoint = utf8_decode(utext, len = utf8_length(*utext))) != 0) {
            float advance = unifont_draw_glyph(codepoint, rendererID, &dstrect) * UNIFONT_DRAW_SCALE;
            dstrect.x += advance;
            drawnTextRect.w += advance;
            if (i < ctx->cursor) {
                cursorRect.x += advance;
            }
            i++;
            utext += len;
        }

        if (ctx->cursor_length > 0) {
            cursorRect.w = ctx->cursor_length * UNIFONT_GLYPH_SIZE * UNIFONT_DRAW_SCALE;
        }
#endif

        cursorRect.y = drawnTextRect.y;
        cursorRect.h = drawnTextRect.h;

        underlineRect = ctx->markedRect;
        underlineRect.y = drawnTextRect.y + drawnTextRect.h - 2;
        underlineRect.h = 2;
        underlineRect.w = drawnTextRect.w;

        SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
        SDL_RenderFillRect(renderer, &underlineRect);
    }

    /* Draw the cursor */
    Uint64 now = SDL_GetTicks();
    if ((now - ctx->last_cursor_change) >= CURSOR_BLINK_INTERVAL_MS) {
        ctx->cursor_visible = !ctx->cursor_visible;
        ctx->last_cursor_change = now;
    }
    if (ctx->cursor_length > 0) {
        /* We'll show a highlight */
        SDL_SetRenderDrawBlendMode(renderer, highlight_mode);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &cursorRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    } else if (ctx->cursor_visible) {
        SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
        SDL_RenderFillRect(renderer, &cursorRect);
    }

    /* Draw the candidates */
    DrawCandidates(ctx, &cursorRect);

    /* Update the area used to draw composition UI */
    UpdateTextInputArea(ctx, &cursorRect);
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

        RedrawWindow(&windowstate[i]);

        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char *argv[])
{
    bool render_composition = false;
    bool render_candidates = false;
    int i, done;
    SDL_Event event;
    char *fontname = NULL;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

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
            render_composition = true;
            consumed = 1;
        } else if (SDL_strcmp(argv[i], "--render-candidates") == 0) {
            render_candidates = true;
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

    windowstate = (WindowState *)SDL_calloc(state->num_windows, sizeof(*windowstate));
    if (!windowstate) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't allocate window state: %s\n", SDL_GetError());
        return -1;
    }

    fontname = GetResourceFilename(fontname, DEFAULT_FONT);

#ifdef HAVE_SDL_TTF
    /* Initialize fonts */
    TTF_Init();

    font = TTF_OpenFont(fontname, DEFAULT_PTSIZE);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find font: %s\n", SDL_GetError());
        return -1;
    }
#else
    if (unifont_init(fontname) < 0) {
        return -1;
    }
#endif

    SDL_Log("Using font: %s\n", fontname);

    /* Initialize window state */
    for (i = 0; i < state->num_windows; ++i) {
        WindowState *ctx = &windowstate[i];
        SDL_Window *window = state->windows[i];
        SDL_Renderer *renderer = state->renderers[i];
        int icon_w = 0, icon_h = 0;

        SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX, SDL_SCALEMODE_LINEAR);

        ctx->window = window;
        ctx->renderer = renderer;
        ctx->rendererID = i;
        ctx->settings_icon = LoadTexture(renderer, "icon.bmp", true, &icon_w, &icon_h);
        ctx->settings_rect.x = (float)WINDOW_WIDTH - icon_w - MARGIN;
        ctx->settings_rect.y = MARGIN;
        ctx->settings_rect.w = (float)icon_w;
        ctx->settings_rect.h = (float)icon_h;

        InitInput(ctx);

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
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                SDL_FPoint point;
                WindowState *ctx = GetWindowStateForWindowID(event.button.windowID);
                if (!ctx) {
                    break;
                }

                SDL_ConvertEventToRenderCoordinates(ctx->renderer, &event);
                point.x = event.button.x;
                point.y = event.button.y;
                if (SDL_PointInRectFloat(&point, &ctx->settings_rect)) {
                    ToggleSettings(ctx);
                } else if (ctx->settings_visible) {
                    ClickSettings(ctx, point.x, point.y);
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                WindowState *ctx = GetWindowStateForWindowID(event.key.windowID);
                if (!ctx) {
                    break;
                }

                switch (event.key.key) {
                case SDLK_RETURN:
                    ctx->text[0] = 0x00;
                    break;
                case SDLK_BACKSPACE:
                    /* Only delete text if not in editing mode. */
                    if (!ctx->markedText[0]) {
                        size_t textlen = SDL_strlen(ctx->text);

                        do {
                            if (textlen == 0) {
                                break;
                            }
                            if (!(ctx->text[textlen - 1] & 0x80)) {
                                /* One byte */
                                ctx->text[textlen - 1] = 0x00;
                                break;
                            }
                            if ((ctx->text[textlen - 1] & 0xC0) == 0x80) {
                                /* Byte from the multibyte sequence */
                                ctx->text[textlen - 1] = 0x00;
                                textlen--;
                            }
                            if ((ctx->text[textlen - 1] & 0xC0) == 0xC0) {
                                /* First byte of multibyte sequence */
                                ctx->text[textlen - 1] = 0x00;
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
            }
            case SDL_EVENT_TEXT_INPUT: {
                WindowState *ctx = GetWindowStateForWindowID(event.text.windowID);
                if (!ctx) {
                    break;
                }

                if (event.text.text[0] == '\0' || event.text.text[0] == '\n' || ctx->markedRect.w < 0) {
                    break;
                }

                SDL_Log("Keyboard: text input \"%s\"\n", event.text.text);

                if (SDL_strlen(ctx->text) + SDL_strlen(event.text.text) < sizeof(ctx->text)) {
                    SDL_strlcat(ctx->text, event.text.text, sizeof(ctx->text));
                }

                SDL_Log("text inputted: %s\n", ctx->text);

                /* After text inputted, we can clear up markedText because it */
                /* is committed */
                ctx->markedText[0] = 0;
                break;
            }
            case SDL_EVENT_TEXT_EDITING: {
                WindowState *ctx = GetWindowStateForWindowID(event.edit.windowID);
                if (!ctx) {
                    break;
                }

                SDL_Log("text editing \"%s\", selected range (%" SDL_PRIs32 ", %" SDL_PRIs32 ")\n",
                        event.edit.text, event.edit.start, event.edit.length);

                SDL_strlcpy(ctx->markedText, event.edit.text, sizeof(ctx->markedText));
                ctx->cursor = event.edit.start;
                ctx->cursor_length = event.edit.length;
                break;
            }
            case SDL_EVENT_TEXT_EDITING_CANDIDATES: {
                WindowState *ctx = GetWindowStateForWindowID(event.edit.windowID);
                if (!ctx) {
                    break;
                }

                SDL_Log("text candidates:\n");
                for (i = 0; i < event.edit_candidates.num_candidates; ++i) {
                    SDL_Log("%c%s\n", i == event.edit_candidates.selected_candidate ? '>' : ' ', event.edit_candidates.candidates[i]);
                }

                ClearCandidates(ctx);
                SaveCandidates(ctx, &event);
                break;
            }
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
