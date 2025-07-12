/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"
#include "SDL_x11dyn.h"
#include "SDL_x11messagebox.h"

#include <X11/keysym.h>
#include <locale.h>

#ifndef SDL_FORK_MESSAGEBOX
#define SDL_FORK_MESSAGEBOX 1
#endif

#define SDL_SET_LOCALE      1
#define SDL_DIALOG_ELEMENT_PADDING 4
#define SDL_DIALOG_ELEMENT_PADDING_2 12
#define SDL_DIALOG_ELEMENT_PADDING_3 8

#if SDL_FORK_MESSAGEBOX
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#endif

#define MAX_BUTTONS       8   // Maximum number of buttons supported
#define MIN_BUTTON_WIDTH  32  // Minimum button width
#define MIN_DIALOG_WIDTH  10 // Minimum dialog width
#define MIN_DIALOG_HEIGHT 10 // Minimum dialog height

static const char g_MessageBoxFontLatin1[] =
    "-*-*-medium-r-normal--0-120-*-*-p-0-iso8859-1";

static const char *g_MessageBoxFont[] = {
    "-*-*-medium-r-normal--*-120-*-*-*-*-iso10646-1",  // explicitly unicode (iso10646-1)
    "-*-*-medium-r-*--*-120-*-*-*-*-iso10646-1",  // explicitly unicode (iso10646-1)
    "-misc-*-*-*-*--*-*-*-*-*-*-iso10646-1",  // misc unicode (fix for some systems)
    "-*-*-*-*-*--*-*-*-*-*-*-iso10646-1",  // just give me anything Unicode.
    "-*-*-medium-r-normal--*-120-*-*-*-*-iso8859-1",  // explicitly latin1, in case low-ASCII works out.
    "-*-*-medium-r-*--*-120-*-*-*-*-iso8859-1",  // explicitly latin1, in case low-ASCII works out.
    "-misc-*-*-*-*--*-*-*-*-*-*-iso8859-1",  // misc latin1 (fix for some systems)
    "-*-*-*-*-*--*-*-*-*-*-*-iso8859-1",  // just give me anything latin1.
    NULL
};

static const char *g_IconFont = "-*-*-bold-r-normal-*-18-*-*-*-*-*-iso8859-1[33 88 105]";

static const SDL_MessageBoxColor g_default_colors[SDL_MESSAGEBOX_COLOR_COUNT] = {
    { 212, 212, 212 },    // SDL_MESSAGEBOX_COLOR_BACKGROUND,
    { 0, 0, 0 }, // SDL_MESSAGEBOX_COLOR_TEXT,
    { 0, 0, 0 }, // SDL_MESSAGEBOX_COLOR_BUTTON_BORDER,
    { 175, 175, 175 },  // SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND,
    { 255, 255, 255 },  // SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED,
};

typedef struct SDL_MessageBoxButtonDataX11
{
    int length;     // Text length
    int text_a;
    int text_d;
    
    SDL_Rect text_rect;
    SDL_Rect rect; // Rectangle for entire button

    const SDL_MessageBoxButtonData *buttondata; // Button data from caller
} SDL_MessageBoxButtonDataX11;

typedef struct TextLineData
{
    int length;       // String length of this text line
    const char *text; // Text for this line
    SDL_Rect rect;
} TextLineData;

typedef struct SDL_MessageBoxDataX11
{
    Display *display;
    int screen;
    Window window;
    Visual *visual;
    Colormap cmap;
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    XdbeBackBuffer buf;
    bool xdbe; // Whether Xdbe is present or not
#endif
    long event_mask;
    Atom wm_protocols;
    Atom wm_delete_message;
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
    bool xrandr; // Whether Xrandr is present or not
#endif

    int dialog_width;  // Dialog box width.
    int dialog_height; // Dialog box height.

    XFontSet font_set;        // for UTF-8 systems
    XFontStruct *font_struct; // Latin1 (ASCII) fallback.
    int numlines;             // Count of Text lines.
    int text_height;          // Height for text lines.
    TextLineData *linedata;

    char icon_char; // Icon, '\0' indicates that the messsage box has no icon.
    XFontStruct *icon_char_font;
    SDL_Rect icon_box_rect;
    int icon_char_x;
    int icon_char_y;
    
    int *pbuttonid; // Pointer to user return buttonID value.

    int button_press_index; // Index into buttondata/buttonpos for button which is pressed (or -1).
    int mouse_over_index;   // Index into buttondata/buttonpos for button mouse is over (or -1).

    int numbuttons; // Count of buttons.
    const SDL_MessageBoxButtonData *buttondata;
    SDL_MessageBoxButtonDataX11 buttonpos[MAX_BUTTONS];

    XColor xcolor[SDL_MESSAGEBOX_COLOR_COUNT];
    XColor xcolor_black;
    XColor xcolor_red;
    XColor xcolor_red_darker;
    XColor xcolor_white;
    XColor xcolor_yellow;
    XColor xcolor_blue;
    XColor xcolor_bg_shadow;

    const SDL_MessageBoxData *messageboxdata;
} SDL_MessageBoxDataX11;

// Maximum helper for ints.
static SDL_INLINE int IntMax(int a, int b)
{
    return (a > b) ? a : b;
}

static void GetTextWidthHeightForFont(SDL_MessageBoxDataX11 *data, XFontStruct *font, const char *str, int nbytes, int *pwidth, int *pheight, int *font_ascent)
{
    XCharStruct text_structure;
    int font_direction, font_descent;
    X11_XTextExtents(font, str, nbytes,
                     &font_direction, font_ascent, &font_descent,
                     &text_structure);
    *pwidth = text_structure.width;
    *pheight = text_structure.ascent + text_structure.descent;
}

// Return width and height for a string.
static void GetTextWidthHeight(SDL_MessageBoxDataX11 *data, const char *str, int nbytes, int *pwidth, int *pheight, int *font_ascent, int *font_descent)
{
#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8) {
        XFontSetExtents *extents;
        XRectangle overall_ink, overall_logical;
        extents = X11_XExtentsOfFontSet(data->font_set);
        X11_Xutf8TextExtents(data->font_set, str, nbytes, &overall_ink, &overall_logical);
        *pwidth = overall_logical.width;
        *pheight = overall_logical.height;
        *font_ascent = -extents->max_logical_extent.y;
        *font_descent = extents->max_logical_extent.height - *font_ascent;
    } else
#endif
    {
        XCharStruct text_structure;
        int font_direction;
        X11_XTextExtents(data->font_struct, str, nbytes,
                         &font_direction, font_ascent, font_descent,
                         &text_structure);
        *pwidth = text_structure.width;
        *pheight = text_structure.ascent + text_structure.descent;
    }
}

// Return index of button if position x,y is contained therein.
static int GetHitButtonIndex(SDL_MessageBoxDataX11 *data, int x, int y)
{
    int i;
    int numbuttons = data->numbuttons;
    SDL_MessageBoxButtonDataX11 *buttonpos = data->buttonpos;

    for (i = 0; i < numbuttons; i++) {
        SDL_Rect *rect = &buttonpos[i].rect;

        if ((x >= rect->x) &&
            (x <= (rect->x + rect->w)) &&
            (y >= rect->y) &&
            (y <= (rect->y + rect->h))) {
            return i;
        }
    }

    return -1;
}

// Initialize SDL_MessageBoxData structure and Display, etc.
static bool X11_MessageBoxInit(SDL_MessageBoxDataX11 *data, const SDL_MessageBoxData *messageboxdata, int *pbuttonid)
{
    int i;
    int numbuttons = messageboxdata->numbuttons;
    const SDL_MessageBoxButtonData *buttondata = messageboxdata->buttons;
    const SDL_MessageBoxColor *colorhints;

    if (numbuttons > MAX_BUTTONS) {
        return SDL_SetError("Too many buttons (%d max allowed)", MAX_BUTTONS);
    }

    data->dialog_width = MIN_DIALOG_WIDTH;
    data->dialog_height = MIN_DIALOG_HEIGHT;
    data->messageboxdata = messageboxdata;
    data->buttondata = buttondata;
    data->numbuttons = numbuttons;
    data->pbuttonid = pbuttonid;
    
    // Convert flags to icon character
    switch (data->messageboxdata->flags & (SDL_MESSAGEBOX_ERROR | SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_INFORMATION)) {
    case SDL_MESSAGEBOX_ERROR:
        data->icon_char = 'X';
        break;
    case SDL_MESSAGEBOX_WARNING:
        data->icon_char = '!';
        break;
    case SDL_MESSAGEBOX_INFORMATION:
        data->icon_char = 'i';
        break;
    default:
        data->icon_char = '\0';
    }

    data->display = X11_XOpenDisplay(NULL);
    if (!data->display) {
        return SDL_SetError("Couldn't open X11 display");
    }
    
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
    int xrandr_event_base, xrandr_error_base;
    data->xrandr = X11_XRRQueryExtension(data->display, &xrandr_event_base, &xrandr_error_base);
#endif
    
#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8) {
        char **missing = NULL;
        int num_missing = 0;
        int i_font;
        for (i_font = 0; g_MessageBoxFont[i_font]; ++i_font) {
            data->font_set = X11_XCreateFontSet(data->display, g_MessageBoxFont[i_font],
                                                &missing, &num_missing, NULL);
            if (missing) {
                X11_XFreeStringList(missing);
            }
            if (data->font_set) {
                break;
            }
        }
        if (!data->font_set) {
            return SDL_SetError("Couldn't load x11 message box font");
        }
    } else
#endif
    {
        data->font_struct = X11_XLoadQueryFont(data->display, g_MessageBoxFontLatin1);
        if (!data->font_struct) {
            return SDL_SetError("Couldn't load font %s", g_MessageBoxFontLatin1);
        }
    }

    if (data->icon_char != '\0') {
        data->icon_char_font = X11_XLoadQueryFont(data->display, g_IconFont);
        if (!data->icon_char_font) {
            data->icon_char_font = X11_XLoadQueryFont(data->display, g_MessageBoxFontLatin1);
            if (!data->icon_char_font) {
                data->icon_char = '\0';
            }
        } 
    }
    
    if (messageboxdata->colorScheme) {
        colorhints = messageboxdata->colorScheme->colors;
    } else {
        colorhints = g_default_colors;
    }

    // Convert colors to 16 bpc XColor format
    for (i = 0; i < SDL_MESSAGEBOX_COLOR_COUNT; i++) {
        data->xcolor[i].flags = DoRed|DoGreen|DoBlue;
        data->xcolor[i].red = colorhints[i].r * 257;
        data->xcolor[i].green = colorhints[i].g * 257;
        data->xcolor[i].blue = colorhints[i].b * 257;
    }

    if (data->icon_char != '\0') {
        data->xcolor_black.flags = DoRed|DoGreen|DoBlue;
        data->xcolor_black.red = 0;
        data->xcolor_black.green = 0;
        data->xcolor_black.blue = 0;
      
        data->xcolor_black.flags = DoRed|DoGreen|DoBlue;
        data->xcolor_white.red = 65535;
        data->xcolor_white.green = 65535;
        data->xcolor_white.blue = 65535;
        
        data->xcolor_red.flags = DoRed|DoGreen|DoBlue;
        data->xcolor_red.red = 65535;
        data->xcolor_red.green = 0;
        data->xcolor_red.blue = 0;
    
        data->xcolor_red_darker.flags = DoRed|DoGreen|DoBlue;
        data->xcolor_red_darker.red = 40535;
        data->xcolor_red_darker.green = 0;
        data->xcolor_red_darker.blue = 0;
        
        data->xcolor_yellow.flags = DoRed|DoGreen|DoBlue;
        data->xcolor_yellow.red = 65535;
        data->xcolor_yellow.green = 65535;
        data->xcolor_yellow.blue = 0;
        
        data->xcolor_blue.flags = DoRed|DoGreen|DoBlue;
        data->xcolor_blue.red = 0;
        data->xcolor_blue.green = 0;
        data->xcolor_blue.blue = 65535;   
            
        data->xcolor_bg_shadow.flags = DoRed|DoGreen|DoBlue;
        data->xcolor_bg_shadow.red = data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].red - 12500;
        data->xcolor_bg_shadow.green = data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].green - 12500;
        data->xcolor_bg_shadow.blue = data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].blue - 12500;
    }
    
    return true;
}

static int CountLinesOfText(const char *text)
{
    int result = 0;
    while (text && *text) {
        const char *lf = SDL_strchr(text, '\n');
        result++; // even without an endline, this counts as a line.
        text = lf ? lf + 1 : NULL;
    }
    return result;
}

// Calculate and initialize text and button locations.
static bool X11_MessageBoxInitPositions(SDL_MessageBoxDataX11 *data)
{
    int paddingx2;
    int padding2x2;    
    int text_width_max;
    int text_height_total;
    int button_height_max;
    int button_width_max;
    int button_width_total;
    int elems_total_height;
    int text_ix;
    int i;
    int t;
    const SDL_MessageBoxData *messageboxdata = data->messageboxdata;

    /* Optimization and initialization */
    text_height_total = 0;
    text_width_max = 0;
    paddingx2 = SDL_DIALOG_ELEMENT_PADDING * 2;
    padding2x2 = SDL_DIALOG_ELEMENT_PADDING_2 * 2;
    data->icon_char_y = 0;
    data->icon_box_rect.w = 0;
    data->icon_box_rect.h = 0;
    text_ix = 0;
    t = 0;
    button_width_max = MIN_BUTTON_WIDTH;
    button_height_max = 0;
    
    /* Calculate icon sizing */
    if (data->icon_char != '\0') {
        int icon_char_w;
        int icon_char_h;
        int icon_char_a;
        int icon_char_max;
        
        GetTextWidthHeightForFont(data, data->icon_char_font, &data->icon_char, 1, &icon_char_w, &icon_char_h, &icon_char_a);
        data->icon_box_rect.w = icon_char_w + paddingx2;
        data->icon_box_rect.h = icon_char_h + paddingx2;
        icon_char_max = IntMax(data->icon_box_rect.w, data->icon_box_rect.h) + 2;
        data->icon_box_rect.w = icon_char_max;
        data->icon_box_rect.h = icon_char_max;        
        data->icon_box_rect.y = 0;
        data->icon_box_rect.x = 0;
        data->icon_char_y = icon_char_a + data->icon_box_rect.y + (data->icon_box_rect.h - icon_char_h)/2 + 1;
        data->icon_char_x = data->icon_box_rect.x + (data->icon_box_rect.w - icon_char_w)/2 + 1;
        data->icon_box_rect.w += 2;
        data->icon_box_rect.h += 2;
    } 
    
    // Go over text and break linefeeds into separate lines.
    if (messageboxdata && messageboxdata->message[0]) {
        int iascent;
        const char *text = messageboxdata->message;
        const int linecount = CountLinesOfText(text);
        TextLineData *plinedata = (TextLineData *)SDL_malloc(sizeof(TextLineData) * linecount);

        if (!plinedata) {
            return false;
        }

        data->linedata = plinedata;
        data->numlines = linecount;
        iascent = 0;
        
        for (i = 0; i < linecount; i++) {
            const char *lf = SDL_strchr(text, '\n');
            const int length = lf ? (lf - text) : SDL_strlen(text);
            int ascent;
            int descent;
        
            plinedata[i].text = text;

            GetTextWidthHeight(data, text, length, &plinedata[i].rect.w, &plinedata[i].rect.h, &ascent, &descent);

            // Text widths are the largest we've ever seen.
            text_width_max = IntMax(text_width_max, plinedata[i].rect.w);

            plinedata[i].length = length;
            if (lf && (lf > text) && (lf[-1] == '\r')) {
                plinedata[i].length--;
            }

            if (i > 0) {
                plinedata[i].rect.y = ascent + descent + plinedata[i-1].rect.y; 
            } else {
                plinedata[i].rect.y = data->icon_box_rect.y + SDL_DIALOG_ELEMENT_PADDING +  ascent;
                iascent = ascent;
            }
            plinedata[i].rect.x = text_ix = data->icon_box_rect.x + data->icon_box_rect.w + SDL_DIALOG_ELEMENT_PADDING_2;
            text += length + 1;

            // Break if there are no more linefeeds.
            if (!lf) {
                break;
            }
        }
        
        text_height_total = plinedata[linecount-1].rect.y +  plinedata[linecount-1].rect.h - iascent - data->icon_box_rect.y - SDL_DIALOG_ELEMENT_PADDING;  
    }
    
    // Loop through all buttons and calculate the button widths and height.
    for (i = 0; i < data->numbuttons; i++) {
        data->buttonpos[i].buttondata = &data->buttondata[i];
        data->buttonpos[i].length = SDL_strlen(data->buttondata[i].text);

        GetTextWidthHeight(data, data->buttondata[i].text, SDL_strlen(data->buttondata[i].text), &data->buttonpos[i].text_rect.w, &data->buttonpos[i].text_rect.h, &data->buttonpos[i].text_a, &data->buttonpos[i].text_d);

        button_height_max = IntMax(button_height_max, (data->buttonpos[i].text_rect.h  + SDL_DIALOG_ELEMENT_PADDING_3 * 2));
        button_width_max = IntMax(button_width_max, (data->buttonpos[i].text_rect.w  + padding2x2));
       
    }   
    button_width_total = button_width_max * data->numbuttons + SDL_DIALOG_ELEMENT_PADDING * (data->numbuttons - 1);
    button_width_total += padding2x2;
  
    /* Dialog width */
    if (button_width_total < (text_ix + text_width_max + padding2x2)) {
        data->dialog_width = IntMax(data->dialog_width, (text_ix + text_width_max + padding2x2));        
    } else {
        data->dialog_width = IntMax(data->dialog_width, button_width_total);            
    }
    
    /* X position of text and icon */
    t = (data->dialog_width - (text_ix + text_width_max))/2;
    if (data->icon_char != '\0') {
        data->icon_box_rect.y = 0;
        if (data->numlines) {
            data->icon_box_rect.x += t;
            data->icon_char_x += t;
        } else {
            int tt;
            
            tt = t;
            t = (data->dialog_width - data->icon_box_rect.w)/2;
            data->icon_box_rect.x += t;
            data->icon_char_x += t;
            t = tt;
        }
    }         
    for (i = 0; i < data->numlines; i++) {
        data->linedata[i].rect.x += t;
    }
        
    /* Button poistioning */
     for (i = 0; i < data->numbuttons; i++) {
        data->buttonpos[i].rect.w = button_width_max;
        data->buttonpos[i].text_rect.x = (data->buttonpos[i].rect.w - data->buttonpos[i].text_rect.w)/2;
        data->buttonpos[i].rect.h = button_height_max;
        data->buttonpos[i].text_rect.y = data->buttonpos[i].text_a + (data->buttonpos[i].rect.h - data->buttonpos[i].text_rect.h)/2;
        if (i > 0) {
            data->buttonpos[i].rect.x += data->buttonpos[i-1].rect.x + data->buttonpos[i-1].rect.w + SDL_DIALOG_ELEMENT_PADDING_3;
            data->buttonpos[i].text_rect.x += data->buttonpos[i].rect.x;
        }
    }    
    button_width_total = data->buttonpos[data->numbuttons-1].rect.x + data->buttonpos[data->numbuttons-1].rect.w;
    data->dialog_width = IntMax(data->dialog_width, (button_width_total + padding2x2));        
    if (messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT) {
        for (i = 0; i < data->numbuttons; i++) {
            data->buttonpos[i].rect.x += (data->dialog_width - button_width_total)/2;
            data->buttonpos[i].text_rect.x += (data->dialog_width - button_width_total)/2;
            if (data->icon_box_rect.h > text_height_total) {
                data->buttonpos[i].text_rect.y += data->icon_box_rect.h + SDL_DIALOG_ELEMENT_PADDING_2 - 2;
                data->buttonpos[i].rect.y += data->icon_box_rect.h + SDL_DIALOG_ELEMENT_PADDING_2;
            } else {
                int a;
                
                a = 0;
                if (data->numlines) {
                    a = data->linedata[data->numlines - 1].rect.y + data->linedata[data->numlines - 1].rect.h;
                }
                data->buttonpos[i].text_rect.y += a + SDL_DIALOG_ELEMENT_PADDING_2 - 2;
                data->buttonpos[i].rect.y += a + SDL_DIALOG_ELEMENT_PADDING_2;
            }
        }    
    } else {
        for (i = data->numbuttons; i != -1; i--) {
            data->buttonpos[i].rect.x += (data->dialog_width - button_width_total)/2;
            data->buttonpos[i].text_rect.x += (data->dialog_width - button_width_total)/2;
            if (data->icon_box_rect.h > text_height_total) {
                data->buttonpos[i].text_rect.y += data->icon_box_rect.h + SDL_DIALOG_ELEMENT_PADDING_2 - 2;
                data->buttonpos[i].rect.y += data->icon_box_rect.h + SDL_DIALOG_ELEMENT_PADDING_2;
            } else {
                int a;
                
                a = 0;
                if (data->numlines) {
                    a = data->linedata[data->numlines - 1].rect.y + data->linedata[data->numlines - 1].rect.h;
                }
                data->buttonpos[i].text_rect.y += a + SDL_DIALOG_ELEMENT_PADDING_2 - 2;
                data->buttonpos[i].rect.y += a + SDL_DIALOG_ELEMENT_PADDING_2;
            }
        }        
    }

    /* Dialog height */
    elems_total_height = data->buttonpos[data->numbuttons-1].rect.y + data->buttonpos[data->numbuttons-1].rect.h;
    data->dialog_height = IntMax(data->dialog_height, (elems_total_height + padding2x2));            
    t = (data->dialog_height - elems_total_height)/2;
    if (data->icon_char != '\0') {
        data->icon_box_rect.y += t;
        data->icon_char_y += t;    
        data->icon_box_rect.w -= 2;
        data->icon_box_rect.h -= 2;        
    }
     for (i = 0; i < data->numbuttons; i++) {
        data->buttonpos[i].text_rect.y += t;
        data->buttonpos[i].rect.y += t;
    }    
    for (i = 0; i < data->numlines; i++) {
        data->linedata[i].rect.y += t;
    }
    return true;
}

// Free SDL_MessageBoxData data.
static void X11_MessageBoxShutdown(SDL_MessageBoxDataX11 *data)
{
    if (data->font_set) {
        X11_XFreeFontSet(data->display, data->font_set);
        data->font_set = NULL;
    }

    if (data->font_struct) {
        X11_XFreeFont(data->display, data->font_struct);
        data->font_struct = NULL;
    }
    
    if (data->icon_char != '\0') {
        X11_XFreeFont(data->display, data->icon_char_font);
        data->icon_char_font = NULL;
    }

#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    if (SDL_X11_HAVE_XDBE && data->xdbe) {
        X11_XdbeDeallocateBackBufferName(data->display, data->buf);
    }
#endif

    if (data->display) {
        if (data->window != None) {
            X11_XWithdrawWindow(data->display, data->window, data->screen);
            X11_XDestroyWindow(data->display, data->window);
            data->window = None;
        }

        X11_XCloseDisplay(data->display);
        data->display = NULL;
    }

    SDL_free(data->linedata);
}

// Create and set up our X11 dialog box indow.
static bool X11_MessageBoxCreateWindow(SDL_MessageBoxDataX11 *data)
{
    int x, y, i;
    XSizeHints *sizehints;
    XSetWindowAttributes wnd_attr;
    Atom _NET_WM_WINDOW_TYPE, _NET_WM_WINDOW_TYPE_DIALOG;
    Display *display = data->display;
    SDL_WindowData *windowdata = NULL;
    const SDL_MessageBoxData *messageboxdata = data->messageboxdata;
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
#ifdef XRANDR_DISABLED_BY_DEFAULT
    const bool use_xrandr_by_default = false;
#else
    const bool use_xrandr_by_default = true;
#endif
#endif

    if (messageboxdata->window) {
        SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(messageboxdata->window);
        windowdata = messageboxdata->window->internal;
        data->screen = displaydata->screen;
    } else {
        data->screen = DefaultScreen(display);
    }

    data->visual = DefaultVisual(display, data->screen);
    data->cmap = DefaultColormap(display, data->screen);
    for (i = 0; i < SDL_MESSAGEBOX_COLOR_COUNT; i++) {
        X11_XAllocColor(display, data->cmap, &data->xcolor[i]);    
    }
    if (data->icon_char != '\0') {
        X11_XAllocColor(display, data->cmap, &data->xcolor_black);    
        X11_XAllocColor(display, data->cmap, &data->xcolor_white);    
        X11_XAllocColor(display, data->cmap, &data->xcolor_red);    
        X11_XAllocColor(display, data->cmap, &data->xcolor_red_darker);    
        X11_XAllocColor(display, data->cmap, &data->xcolor_yellow);    
        X11_XAllocColor(display, data->cmap, &data->xcolor_blue);    
        X11_XAllocColor(display, data->cmap, &data->xcolor_bg_shadow);    
    }
        
    data->event_mask = ExposureMask |
                       ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
                       StructureNotifyMask | FocusChangeMask | PointerMotionMask;
    wnd_attr.event_mask = data->event_mask;
    wnd_attr.colormap = data->cmap;

    data->window = X11_XCreateWindow(
        display, RootWindow(display, data->screen),
        0, 0,
        data->dialog_width, data->dialog_height,
        0, DefaultDepth(display, data->screen), InputOutput, data->visual,
        CWEventMask | CWColormap, &wnd_attr);
    if (data->window == None) {
        return SDL_SetError("Couldn't create X window");
    }

    if (windowdata) {
        Atom _NET_WM_STATE = X11_XInternAtom(display, "_NET_WM_STATE", False);
        Atom stateatoms[16];
        size_t statecount = 0;
        // Set some message-boxy window states when attached to a parent window...
        // we skip the taskbar since this will pop to the front when the parent window is clicked in the taskbar, etc
        stateatoms[statecount++] = X11_XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
        stateatoms[statecount++] = X11_XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);
        stateatoms[statecount++] = X11_XInternAtom(display, "_NET_WM_STATE_FOCUSED", False);
        stateatoms[statecount++] = X11_XInternAtom(display, "_NET_WM_STATE_MODAL", False);
        SDL_assert(statecount <= SDL_arraysize(stateatoms));
        X11_XChangeProperty(display, data->window, _NET_WM_STATE, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)stateatoms, statecount);

        // http://tronche.com/gui/x/icccm/sec-4.html#WM_TRANSIENT_FOR
        X11_XSetTransientForHint(display, data->window, windowdata->xwindow);
    }

    SDL_X11_SetWindowTitle(display, data->window, (char *)messageboxdata->title);

    // Let the window manager know this is a dialog box
    _NET_WM_WINDOW_TYPE = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    _NET_WM_WINDOW_TYPE_DIALOG = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    X11_XChangeProperty(display, data->window, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                        PropModeReplace,
                        (unsigned char *)&_NET_WM_WINDOW_TYPE_DIALOG, 1);

    // Allow the window to be deleted by the window manager
    data->wm_delete_message = X11_XInternAtom(display, "WM_DELETE_WINDOW", False);
    X11_XSetWMProtocols(display, data->window, &data->wm_delete_message, 1);

    data->wm_protocols = X11_XInternAtom(display, "WM_PROTOCOLS", False);

    if (windowdata) {
        XWindowAttributes attrib;
        Window dummy;

        X11_XGetWindowAttributes(display, windowdata->xwindow, &attrib);
        x = attrib.x + (attrib.width - data->dialog_width) / 2;
        y = attrib.y + (attrib.height - data->dialog_height) / 3;
        X11_XTranslateCoordinates(display, windowdata->xwindow, RootWindow(display, data->screen), x, y, &x, &y, &dummy);
    } else {
        const SDL_VideoDevice *dev = SDL_GetVideoDevice();
        if (dev && dev->displays && dev->num_displays > 0) {
            const SDL_VideoDisplay *dpy = dev->displays[0];
            const SDL_DisplayData *dpydata = dpy->internal;
            x = dpydata->x + ((dpy->current_mode->w - data->dialog_width) / 2);
            y = dpydata->y + ((dpy->current_mode->h - data->dialog_height) / 3);
        }
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
        else if (SDL_GetHintBoolean(SDL_HINT_VIDEO_X11_XRANDR, use_xrandr_by_default) && data->xrandr) {
            XRRScreenResources *screen = X11_XRRGetScreenResourcesCurrent(display, DefaultRootWindow(display));
            if (!screen) {
                goto XRANDRBAIL;
            }
            if (!screen->ncrtc) {
                goto XRANDRBAIL;
            }

            XRRCrtcInfo *crtc_info = X11_XRRGetCrtcInfo(display, screen, screen->crtcs[0]);
            if (crtc_info) {
                x = (crtc_info->width - data->dialog_width) / 2;
                y = (crtc_info->height - data->dialog_height) / 3;
            } else {
                goto XRANDRBAIL;
            }
        }
#endif
        else {
            // oh well. This will misposition on a multi-head setup. Init first next time.
            XRANDRBAIL:
            x = (DisplayWidth(display, data->screen) - data->dialog_width) / 2;
            y = (DisplayHeight(display, data->screen) - data->dialog_height) / 3;
        }
    }
    X11_XMoveWindow(display, data->window, x, y);

    sizehints = X11_XAllocSizeHints();
    if (sizehints) {
        sizehints->flags = USPosition | USSize | PMaxSize | PMinSize;
        sizehints->x = x;
        sizehints->y = y;
        sizehints->width = data->dialog_width;
        sizehints->height = data->dialog_height;

        sizehints->min_width = sizehints->max_width = data->dialog_width;
        sizehints->min_height = sizehints->max_height = data->dialog_height;

        X11_XSetWMNormalHints(display, data->window, sizehints);

        X11_XFree(sizehints);
    }

    X11_XMapRaised(display, data->window);

#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    // Initialise a back buffer for double buffering
    if (SDL_X11_HAVE_XDBE) {
        int xdbe_major, xdbe_minor;
        if (X11_XdbeQueryExtension(display, &xdbe_major, &xdbe_minor) != 0) {
            data->xdbe = true;
            data->buf = X11_XdbeAllocateBackBufferName(display, data->window, XdbeUndefined);
        } else {
            data->xdbe = false;
        }
    }
#endif

    return true;
}

// Draw our message box.
static void X11_MessageBoxDraw(SDL_MessageBoxDataX11 *data, GC ctx, bool utf8)
{
    int i;
    Drawable window = data->window;
    Display *display = data->display;

#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    if (SDL_X11_HAVE_XDBE && data->xdbe) {
        window = data->buf;
        X11_XdbeBeginIdiom(data->display);
    }
#endif

    X11_XSetForeground(display, ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].pixel);
    X11_XFillRectangle(display, window, ctx, 0, 0, data->dialog_width, data->dialog_height);

    if(data->icon_char != '\0') {
        X11_XSetForeground(display, ctx, data->xcolor_bg_shadow.pixel);
        X11_XFillArc(display, window, ctx, data->icon_box_rect.x + 2, data->icon_box_rect.y + 2, data->icon_box_rect.w, data->icon_box_rect.h, 0, 360 * 64);
        switch (data->messageboxdata->flags & (SDL_MESSAGEBOX_ERROR | SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_INFORMATION)) {
        case SDL_MESSAGEBOX_ERROR:
                X11_XSetForeground(display, ctx, data->xcolor_red_darker.pixel);
                X11_XFillArc(display, window, ctx, data->icon_box_rect.x, data->icon_box_rect.y, data->icon_box_rect.w, data->icon_box_rect.h, 0, 360 * 64);
                X11_XSetForeground(display, ctx, data->xcolor_red.pixel);
                X11_XFillArc(display, window, ctx, data->icon_box_rect.x+1, data->icon_box_rect.y+1, data->icon_box_rect.w-2, data->icon_box_rect.h-2, 0, 360 * 64);
                X11_XSetForeground(display, ctx, data->xcolor_white.pixel);
                break;
        case SDL_MESSAGEBOX_WARNING:
                X11_XSetForeground(display, ctx, data->xcolor_black.pixel);
                X11_XFillArc(display, window, ctx, data->icon_box_rect.x, data->icon_box_rect.y, data->icon_box_rect.w, data->icon_box_rect.h, 0, 360 * 64);
                X11_XSetForeground(display, ctx, data->xcolor_yellow.pixel);
                X11_XFillArc(display, window, ctx, data->icon_box_rect.x+1, data->icon_box_rect.y+1, data->icon_box_rect.w-2, data->icon_box_rect.h-2, 0, 360 * 64);
                X11_XSetForeground(display, ctx, data->xcolor_black.pixel);
                break;
        case SDL_MESSAGEBOX_INFORMATION:
                X11_XSetForeground(display, ctx, data->xcolor_white.pixel);
                X11_XFillArc(display, window, ctx, data->icon_box_rect.x, data->icon_box_rect.y, data->icon_box_rect.w, data->icon_box_rect.h, 0, 360 * 64);
                X11_XSetForeground(display, ctx, data->xcolor_blue.pixel);
                X11_XFillArc(display, window, ctx, data->icon_box_rect.x+1, data->icon_box_rect.y+1, data->icon_box_rect.w-2, data->icon_box_rect.h-2, 0, 360 * 64);
                X11_XSetForeground(display, ctx, data->xcolor_white.pixel);
                break;
        default:
                X11_XSetForeground(display, ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);
                X11_XFillArc(display, window, ctx, data->icon_box_rect.x, data->icon_box_rect.y, data->icon_box_rect.w, data->icon_box_rect.h, 0, 360 * 64);
                X11_XSetForeground(display, ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].pixel);
        }
        X11_XSetFont(display, ctx, data->icon_char_font->fid); 
        X11_XDrawString(display, window, ctx, data->icon_char_x, data->icon_char_y, &data->icon_char, 1);
        if (!utf8) {
            X11_XSetFont(display, ctx, data->font_struct->fid); 
        }
    }
    
    X11_XSetForeground(display, ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);
    for (i = 0; i < data->numlines; i++) {
        TextLineData *plinedata = &data->linedata[i];

#ifdef X_HAVE_UTF8_STRING
        if (SDL_X11_HAVE_UTF8) {
            X11_Xutf8DrawString(display, window, data->font_set, ctx,
                                plinedata->rect.x, plinedata->rect.y,
                                plinedata->text, plinedata->length);
        } else
#endif
        {
            X11_XDrawString(display, window, ctx,
                            plinedata->rect.x, plinedata->rect.y,
                            plinedata->text, plinedata->length);
        }
    }

    X11_XSetForeground(display, ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);
    for (i = 0; i < data->numbuttons; i++) {
        SDL_MessageBoxButtonDataX11 *buttondatax11 = &data->buttonpos[i];
        const SDL_MessageBoxButtonData *buttondata = buttondatax11->buttondata;
        
        X11_XSetForeground(display, ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].pixel);
        X11_XFillRectangle(display, window, ctx,
                           buttondatax11->rect.x, buttondatax11->rect.y,
                           buttondatax11->rect.w, buttondatax11->rect.h);

        if (buttondata->flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT) {
            X11_XSetForeground(display, ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED].pixel);
        } else {
            X11_XSetForeground(display, ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].pixel);
        }
        X11_XDrawRectangle(display, window, ctx,
                           buttondatax11->rect.x, buttondatax11->rect.y,
                           buttondatax11->rect.w, buttondatax11->rect.h);

        X11_XSetForeground(display, ctx, (data->mouse_over_index == i) ? data->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED].pixel : data->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);


#ifdef X_HAVE_UTF8_STRING
        if (SDL_X11_HAVE_UTF8) {
            X11_Xutf8DrawString(display, window, data->font_set, ctx,
                                buttondatax11->text_rect.x,
                                buttondatax11->text_rect.y,
                                buttondata->text, buttondatax11->length);
        } else
#endif
        {
            X11_XDrawString(display, window, ctx,
                            buttondatax11->text_rect.x, buttondatax11->text_rect.y,
                            buttondata->text, buttondatax11->length);
        }
    }

#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    if (SDL_X11_HAVE_XDBE && data->xdbe) {
        XdbeSwapInfo swap_info;
        swap_info.swap_window = data->window;
        swap_info.swap_action = XdbeUndefined;
        X11_XdbeSwapBuffers(data->display, &swap_info, 1);
        X11_XdbeEndIdiom(data->display);
    }
#endif
}

// NOLINTNEXTLINE(readability-non-const-parameter): cannot make XPointer a const pointer due to typedef
static Bool X11_MessageBoxEventTest(Display *display, XEvent *event, XPointer arg)
{
    const SDL_MessageBoxDataX11 *data = (const SDL_MessageBoxDataX11 *)arg;
    return ((event->xany.display == data->display) && (event->xany.window == data->window)) ? True : False;
}

// Loop and handle message box event messages until something kills it.
static bool X11_MessageBoxLoop(SDL_MessageBoxDataX11 *data)
{
    GC ctx;
    XGCValues ctx_vals;
    bool close_dialog = false;
    bool has_focus = true;
    KeySym last_key_pressed = XK_VoidSymbol;
    unsigned long gcflags = GCForeground | GCBackground;
#ifdef X_HAVE_UTF8_STRING
    const int have_utf8 = SDL_X11_HAVE_UTF8;
#else
    const int have_utf8 = 0;
#endif

    SDL_zero(ctx_vals);
    ctx_vals.foreground = data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].pixel;
    ctx_vals.background = data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].pixel;

    if (!have_utf8) {
        gcflags |= GCFont;
        ctx_vals.font = data->font_struct->fid;
    }

    ctx = X11_XCreateGC(data->display, data->window, gcflags, &ctx_vals);
    if (ctx == None) {
        return SDL_SetError("Couldn't create graphics context");
    }

    data->button_press_index = -1; // Reset what button is currently depressed.
    data->mouse_over_index = -1;   // Reset what button the mouse is over.

    while (!close_dialog) {
        XEvent e;
        bool draw = true;

        // can't use XWindowEvent() because it can't handle ClientMessage events.
        // can't use XNextEvent() because we only want events for this window.
        X11_XIfEvent(data->display, &e, X11_MessageBoxEventTest, (XPointer)data);

        /* If X11_XFilterEvent returns True, then some input method has filtered the
           event, and the client should discard the event. */
        if ((e.type != Expose) && X11_XFilterEvent(&e, None)) {
            continue;
        }

        switch (e.type) {
        case Expose:
            if (e.xexpose.count > 0) {
                draw = false;
            }
            break;

        case FocusIn:
            // Got focus.
            has_focus = true;
            break;

        case FocusOut:
            // lost focus. Reset button and mouse info.
            has_focus = false;
            data->button_press_index = -1;
            data->mouse_over_index = -1;
            break;

        case MotionNotify:
            if (has_focus) {
                // Mouse moved...
                const int previndex = data->mouse_over_index;
                data->mouse_over_index = GetHitButtonIndex(data, e.xbutton.x, e.xbutton.y);
                if (data->mouse_over_index == previndex) {
                    draw = false;
                }
            }
            break;

        case ClientMessage:
            if (e.xclient.message_type == data->wm_protocols &&
                e.xclient.format == 32 &&
                e.xclient.data.l[0] == data->wm_delete_message) {
                close_dialog = true;
            }
            break;

        case KeyPress:
            // Store key press - we make sure in key release that we got both.
            last_key_pressed = X11_XLookupKeysym(&e.xkey, 0);
            break;

        case KeyRelease:
        {
            Uint32 mask = 0;
            KeySym key = X11_XLookupKeysym(&e.xkey, 0);

            // If this is a key release for something we didn't get the key down for, then bail.
            if (key != last_key_pressed) {
                break;
            }

            if (key == XK_Escape) {
                mask = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
            } else if ((key == XK_Return) || (key == XK_KP_Enter)) {
                mask = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
            }

            if (mask) {
                int i;

                // Look for first button with this mask set, and return it if found.
                for (i = 0; i < data->numbuttons; i++) {
                    SDL_MessageBoxButtonDataX11 *buttondatax11 = &data->buttonpos[i];

                    if (buttondatax11->buttondata->flags & mask) {
                        *data->pbuttonid = buttondatax11->buttondata->buttonID;
                        close_dialog = true;
                        break;
                    }
                }
            }
            break;
        }

        case ButtonPress:
            data->button_press_index = -1;
            if (e.xbutton.button == Button1) {
                // Find index of button they clicked on.
                data->button_press_index = GetHitButtonIndex(data, e.xbutton.x, e.xbutton.y);
            }
            break;

        case ButtonRelease:
            // If button is released over the same button that was clicked down on, then return it.
            if ((e.xbutton.button == Button1) && (data->button_press_index >= 0)) {
                int button = GetHitButtonIndex(data, e.xbutton.x, e.xbutton.y);

                if (data->button_press_index == button) {
                    SDL_MessageBoxButtonDataX11 *buttondatax11 = &data->buttonpos[button];

                    *data->pbuttonid = buttondatax11->buttondata->buttonID;
                    close_dialog = true;
                }
            }
            data->button_press_index = -1;
            break;
        }

        if (draw) {
            // Draw our dialog box.
            X11_MessageBoxDraw(data, ctx, have_utf8);
        }
    }

    X11_XFreeGC(data->display, ctx);
    return true;
}

static bool X11_ShowMessageBoxImpl(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
    bool result = false;
    SDL_MessageBoxDataX11 data;
#if SDL_SET_LOCALE
    char *origlocale;
#endif

    SDL_zero(data);

    if (!SDL_X11_LoadSymbols()) {
        return false;
    }

#if SDL_SET_LOCALE
    origlocale = setlocale(LC_ALL, NULL);
    if (origlocale) {
        origlocale = SDL_strdup(origlocale);
        if (!origlocale) {
            return false;
        }
        (void)setlocale(LC_ALL, "");
    }
#endif

    // This code could get called from multiple threads maybe?
    X11_XInitThreads();

    // Initialize the return buttonID value to -1 (for error or dialogbox closed).
    *buttonID = -1;

    // Init and display the message box.
    if (!X11_MessageBoxInit(&data, messageboxdata, buttonID)) {
        goto done;
    }
    
    if (!X11_MessageBoxInitPositions(&data))  {
        goto done;
    }
    
    if (!X11_MessageBoxCreateWindow(&data))  {
        goto done;
    }    
    
    result = X11_MessageBoxLoop(&data);

done:
    X11_MessageBoxShutdown(&data);
#if SDL_SET_LOCALE
    if (origlocale) {
        (void)setlocale(LC_ALL, origlocale);
        SDL_free(origlocale);
    }
#endif

    return result;
}

// Display an x11 message box.
bool X11_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
#if SDL_FORK_MESSAGEBOX
    // Use a child process to protect against setlocale(). Annoying.
    pid_t pid;
    int fds[2];
    int status = 0;
    bool result = true;

    if (pipe(fds) == -1) {
        return X11_ShowMessageBoxImpl(messageboxdata, buttonID); // oh well.
    }

    pid = fork();
    if (pid == -1) { // failed
        close(fds[0]);
        close(fds[1]);
        return X11_ShowMessageBoxImpl(messageboxdata, buttonID); // oh well.
    } else if (pid == 0) {                                       // we're the child
        int exitcode = 0;
        close(fds[0]);
        result = X11_ShowMessageBoxImpl(messageboxdata, buttonID);
        if (write(fds[1], &result, sizeof(result)) != sizeof(result)) {
            exitcode = 1;
        } else if (write(fds[1], buttonID, sizeof(*buttonID)) != sizeof(*buttonID)) {
            exitcode = 1;
        }
        close(fds[1]);
        _exit(exitcode); // don't run atexit() stuff, static destructors, etc.
    } else {             // we're the parent
        pid_t rc;
        close(fds[1]);
        do {
            rc = waitpid(pid, &status, 0);
        } while ((rc == -1) && (errno == EINTR));

        SDL_assert(rc == pid); // not sure what to do if this fails.

        if ((rc == -1) || (!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
            result = SDL_SetError("msgbox child process failed");
        } else if ((read(fds[0], &result, sizeof(result)) != sizeof(result)) ||
                   (read(fds[0], buttonID, sizeof(*buttonID)) != sizeof(*buttonID))) {
            result = SDL_SetError("read from msgbox child process failed");
            *buttonID = 0;
        }
        close(fds[0]);

        return result;
    }
#else
    return X11_ShowMessageBoxImpl(messageboxdata, buttonID);
#endif
}
#endif // SDL_VIDEO_DRIVER_X11
