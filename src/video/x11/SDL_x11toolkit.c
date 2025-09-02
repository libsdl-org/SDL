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

#include "../../SDL_list.h"
#include "SDL_x11video.h"
#include "SDL_x11dyn.h"
#include "SDL_x11toolkit.h"
#include "SDL_x11settings.h"
#include "SDL_x11modes.h"
#include "xsettings-client.h"
#include <X11/keysym.h>
#include <locale.h>

#define SDL_SET_LOCALE 1
#define SDL_GRAB 1

typedef struct SDL_ToolkitIconControlX11
{
    SDL_ToolkitControlX11 parent;
    
    /* Icon type */
    SDL_MessageBoxFlags flags;
    char icon_char;
    
    /* Font */
    XFontStruct *icon_char_font;
    int icon_char_x;
    int icon_char_y;
    int icon_char_a;
    
    /* Colors */
    XColor xcolor_black;
    XColor xcolor_red;
    XColor xcolor_red_darker;
    XColor xcolor_white;
    XColor xcolor_yellow;
    XColor xcolor_blue;
    XColor xcolor_bg_shadow;
} SDL_ToolkitIconControlX11;

typedef struct SDL_ToolkitButtonControlX11
{
    SDL_ToolkitControlX11 parent;
    
    /* Data */
    const SDL_MessageBoxButtonData *data;
    
    /* Text */
    SDL_Rect text_rect;
    int text_a;
    int text_d;
    int str_sz;
    
    /* Callback */
	void *cb_data;
	void (*cb)(struct SDL_ToolkitControlX11 *, void *);
} SDL_ToolkitButtonControlX11;

typedef struct SDL_ToolkitLabelControlX11
{
    SDL_ToolkitControlX11 parent;
    
    char **lines;
    int *y;
    size_t *szs;
    size_t sz;
} SDL_ToolkitLabelControlX11;

typedef struct SDL_ToolkitMenuBarControlX11
{
	SDL_ToolkitControlX11 parent;
	
	SDL_ListNode *menu_items;
} SDL_ToolkitMenuBarControlX11;

typedef SDL_ToolkitMenuBarControlX11 SDL_ToolkitMenuControlX11;

/* Font for icon control */
static const char *g_IconFont = "-*-*-bold-r-normal-*-%d-*-*-*-*-*-iso8859-1[33 88 105]";
#define G_ICONFONT_SIZE 18

/* General UI font */
static const char g_ToolkitFontLatin1[] =
    "-*-*-medium-r-normal--0-%d-*-*-p-0-iso8859-1";
static const char *g_ToolkitFont[] = {
    "-*-*-medium-r-normal--*-%d-*-*-*-*-iso10646-1",  // explicitly unicode (iso10646-1)
    "-*-*-medium-r-*--*-%d-*-*-*-*-iso10646-1",  // explicitly unicode (iso10646-1)
    "-misc-*-*-*-*--*-*-*-*-*-*-iso10646-1",  // misc unicode (fix for some systems)
    "-*-*-*-*-*--*-*-*-*-*-*-iso10646-1",  // just give me anything Unicode.
    "-*-*-medium-r-normal--*-v-*-*-*-*-iso8859-1",  // explicitly latin1, in case low-ASCII works out.
    "-*-*-medium-r-*--*-%d-*-*-*-*-iso8859-1",  // explicitly latin1, in case low-ASCII works out.
    "-misc-*-*-*-*--*-*-*-*-*-*-iso8859-1",  // misc latin1 (fix for some systems)
    "-*-*-*-*-*--*-*-*-*-*-*-iso8859-1",  // just give me anything latin1.
    NULL
};
#define G_TOOLKITFONT_SIZE 120

static const SDL_MessageBoxColor g_default_colors[SDL_MESSAGEBOX_COLOR_COUNT] = {
    { 191, 184, 191 },    // SDL_MESSAGEBOX_COLOR_BACKGROUND,
    { 0, 0, 0 }, // SDL_MESSAGEBOX_COLOR_TEXT,
    { 127, 120, 127 }, // SDL_MESSAGEBOX_COLOR_BUTTON_BORDER,
    { 191, 184, 191 },  // SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND,
    { 235, 235, 235 },  // SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED,
};

int X11Toolkit_SettingsGetInt(XSettingsClient *client, const char *key, int fallback_value) {
    XSettingsSetting *setting = NULL;
    int res = fallback_value;

    if (client) {
        if (xsettings_client_get_setting(client, key, &setting) != XSETTINGS_SUCCESS) {
            goto no_key;
        }

        if (setting->type != XSETTINGS_TYPE_INT) {
            goto no_key;
        }

        res = setting->data.v_int;
    }

no_key:
    if (setting) {
        xsettings_setting_free(setting);
    }

    return res;
}

static float X11Toolkit_GetUIScale(XSettingsClient *client, Display *display)
{
    double scale_factor = 0.0;

    // First use the forced scaling factor specified by the app/user
    const char *hint = SDL_GetHint(SDL_HINT_VIDEO_X11_SCALING_FACTOR);
    if (hint && *hint) {
        double value = SDL_atof(hint);
        if (value >= 1.0f && value <= 10.0f) {
            scale_factor = value;
        }
    }

    // If that failed, try "Xft.dpi" from the XResourcesDatabase...
    // We attempt to read this directly to get the live value, XResourceManagerString
    // is cached per display connection.
    if (scale_factor <= 0.0)
    {
        int status, real_format;
        Atom real_type;
        Atom res_mgr;
        unsigned long items_read, items_left;
        char *resource_manager;
        bool owns_resource_manager = false;

        X11_XrmInitialize();
        res_mgr = X11_XInternAtom(display, "RESOURCE_MANAGER", False);
        status = X11_XGetWindowProperty(display, RootWindow(display, DefaultScreen(display)),
                                        res_mgr, 0L, 8192L, False, XA_STRING,
                                        &real_type, &real_format, &items_read, &items_left,
                                        (unsigned char **)&resource_manager);

        if (status == Success && resource_manager) {
            owns_resource_manager = true;
        } else {
            // Fall back to XResourceManagerString. This will not be updated if the
            // dpi value is later changed but should allow getting the initial value.
            resource_manager = X11_XResourceManagerString(display);
        }

        if (resource_manager) {
            XrmDatabase db;
            XrmValue value;
            char *type;

            db = X11_XrmGetStringDatabase(resource_manager);

            // Get the value of Xft.dpi from the Database
            if (X11_XrmGetResource(db, "Xft.dpi", "String", &type, &value)) {
                if (value.addr && type && SDL_strcmp(type, "String") == 0) {
                    int dpi = SDL_atoi(value.addr);
                    scale_factor  = dpi / 96.0;
                }
            }
            X11_XrmDestroyDatabase(db);

            if (owns_resource_manager) {
                X11_XFree(resource_manager);
            }
        }
    }

    // If that failed, try the XSETTINGS keys...
    if (scale_factor <= 0.0) {
        scale_factor = X11Toolkit_SettingsGetInt(client, "Gdk/WindowScalingFactor", -1);

        // The Xft/DPI key is stored in increments of 1024th
        if (scale_factor <= 0.0) {
            int dpi = X11Toolkit_SettingsGetInt(client, "Xft/DPI", -1);
            if (dpi > 0) {
                scale_factor = (double) dpi / 1024.0;
                scale_factor /= 96.0;
            }
        }
    }

    // If that failed, try the GDK_SCALE envvar...
    if (scale_factor <= 0.0) {
        const char *scale_str = SDL_getenv("GDK_SCALE");
        if (scale_str) {
            scale_factor = SDL_atoi(scale_str);
        }
    }

    // Nothing or a bad value, just fall back to 1.0
    if (scale_factor <= 0.0) {
        scale_factor = 1.0;
    }

    return (float)scale_factor;
}

static void X11Toolkit_SettingsNotify(const char *name, XSettingsAction action, XSettingsSetting *setting, void *data)
{
    SDL_ToolkitWindowX11 *window;
    int i;

    window = data;
	
    if (window->xsettings_first_time) {
        return;
    }
    	
    if (SDL_strcmp(name, SDL_XSETTINGS_GDK_WINDOW_SCALING_FACTOR) == 0 ||
        SDL_strcmp(name, SDL_XSETTINGS_GDK_UNSCALED_DPI) == 0 ||
        SDL_strcmp(name, SDL_XSETTINGS_XFT_DPI) == 0) {
        bool dbe_already_setup;
        bool pixmap_already_setup;
        
        if (window->pixmap) {
            pixmap_already_setup = true;
        } else {
            dbe_already_setup = true;
        }
        
        /* set scale vars */
        window->scale = X11Toolkit_GetUIScale(window->xsettings, window->display);
        window->iscale = (int)SDL_ceilf(window->scale);
        if (roundf(window->scale) == window->scale) {
            window->scale = 0;
        }
        
        /* set up window */
        if (window->scale != 0) {
            window->window_width = SDL_lroundf((window->window_width/window->iscale) * window->scale);
            window->window_height = SDL_lroundf((window->window_height/window->iscale) * window->scale);
            window->pixmap_width = window->window_width;
            window->pixmap_height = window->window_height;
            window->pixmap = true;
        } else {
            window->pixmap = false;
        }
        
        if (window->pixmap) {
            if (!pixmap_already_setup) {
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
                if (SDL_X11_HAVE_XDBE && window->xdbe) {
                    X11_XdbeDeallocateBackBufferName(window->display, window->buf);
                }
#endif
            }

            X11_XFreePixmap(window->display, window->drawable);
            window->drawable = X11_XCreatePixmap(window->display, window->window, window->pixmap_width, window->pixmap_height, window->depth);        
        } else {
            if (!dbe_already_setup) {
                X11_XFreePixmap(window->display, window->drawable);
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
                if (SDL_X11_HAVE_XDBE && window->xdbe) {
                    window->buf = X11_XdbeAllocateBackBufferName(window->display, window->window, XdbeUndefined);
                    window->drawable = window->buf;
                }
#endif
            }
        }
        
        /* setup fonts */
#ifdef X_HAVE_UTF8_STRING
        if (window->font_set) {
            X11_XFreeFontSet(window->display, window->font_set);
        }
#endif
        if (window->font_struct) {
            X11_XFreeFont(window->display, window->font_struct);
        }
    
#ifdef X_HAVE_UTF8_STRING
        window->utf8 = true;
        window->font_set = NULL;
        if (SDL_X11_HAVE_UTF8) {
            char **missing = NULL;
            int num_missing = 0;
            int i_font;
            window->font_struct = NULL;
            for (i_font = 0; g_ToolkitFont[i_font]; ++i_font) {
                char *font;
                
                SDL_asprintf(&font, g_ToolkitFont[i_font], G_TOOLKITFONT_SIZE * window->iscale);
                window->font_set = X11_XCreateFontSet(window->display, font,
                                                    &missing, &num_missing, NULL);
                SDL_free(font);
                if (missing) {
                    X11_XFreeStringList(missing);
                }
                if (window->font_set) {
                    break;
                }
            }
            if (!window->font_set) {
                goto load_font_traditional;
            }
        } else
#endif
        {
            char *font;
            load_font_traditional:

            SDL_asprintf(&font, g_ToolkitFontLatin1, G_TOOLKITFONT_SIZE * window->iscale);
            window->font_struct = X11_XLoadQueryFont(window->display, font);
            SDL_free(font);
            window->utf8 = false;    
        }

        /* notify controls */
        for (i = 0; i < window->controls_sz; i++) {
            if (window->controls[i]->func_on_scale_change) {
                window->controls[i]->func_on_scale_change(window->controls[i]);
            }
            
            if (window->controls[i]->func_calc_size) {
                window->controls[i]->func_calc_size(window->controls[i]);
            }
        }
        
        /* notify cb */
        if (window->cb_on_scale_change) {
            window->cb_on_scale_change(window, window->cb_data);
        }
        
        /* update ev scales */
		if (!window->pixmap) {
			window->ev_scale = window->ev_iscale = 1;
		} else {
			window->ev_scale = window->scale;
			window->ev_iscale = window->iscale;
		}
    }
}


static void X11Toolkit_GetTextWidthHeightForFont(XFontStruct *font, const char *str, int nbytes, int *pwidth, int *pheight, int *font_ascent)
{
    XCharStruct text_structure;
    int font_direction, font_descent;
    X11_XTextExtents(font, str, nbytes,
                     &font_direction, font_ascent, &font_descent,
                     &text_structure);
    *pwidth = text_structure.width;
    *pheight = text_structure.ascent + text_structure.descent;
}

static void X11Toolkit_GetTextWidthHeight(SDL_ToolkitWindowX11 *data, const char *str, int nbytes, int *pwidth, int *pheight, int *font_ascent, int *font_descent)
{
#ifdef X_HAVE_UTF8_STRING
    if (data->utf8) {
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

SDL_ToolkitWindowX11 *X11Toolkit_CreateWindowStruct(SDL_Window *parent, SDL_ToolkitWindowX11 *tkparent, SDL_ToolkitWindowModeX11 mode, const SDL_MessageBoxColor *colorhints)
{
    SDL_ToolkitWindowX11 *window;
    int i;
    #define ErrorFreeRetNull(x, y) SDL_SetError(x); SDL_free(y); return NULL;
    #define ErrorCloseFreeRetNull(x, y, z) X11_XCloseDisplay(z->display); SDL_SetError(x, y); SDL_free(z); return NULL;

    if (!SDL_X11_LoadSymbols()) {
        return NULL;
    }
    
    // This code could get called from multiple threads maybe?
    X11_XInitThreads();
    
    window = (SDL_ToolkitWindowX11 *)SDL_malloc(sizeof(SDL_ToolkitWindowX11));
    if (!window) {
        SDL_SetError("Unable to allocate toolkit window structure");
        return NULL;
    }
    
    window->mode = mode;
    window->tk_parent = tkparent;
    
#if SDL_SET_LOCALE
	if (mode != SDL_TOOLKIT_WINDOW_MODE_X11_CHILD) {
		window->origlocale = setlocale(LC_ALL, NULL);
		if (window->origlocale) {
			window->origlocale = SDL_strdup(window->origlocale);
			if (!window->origlocale) {
				return NULL;
			}
			(void)setlocale(LC_ALL, "");
		}
	}
#endif

    if (parent) {
        SDL_VideoData *videodata = SDL_GetVideoDevice()->internal;
        window->display = videodata->display;
		window->display_close = false;
    } else if (tkparent) {
        window->display = tkparent->display;
		window->display_close = false;		
	} else {
		window->display = X11_XOpenDisplay(NULL);
		window->display_close = true;
		if (!window->display) {
			ErrorFreeRetNull("Couldn't open X11 display", window);
		}
    }    
    
#ifdef SDL_VIDEO_DRIVER_X11_XRSANDR
    int xrandr_event_base, xrandr_error_base;
    window->xrandr = X11_XRRQueryExtension(window->display, &xrandr_event_base, &xrandr_error_base);
#endif
        
    /* Scale/Xsettings */
    window->pixmap = false;
    window->xsettings_first_time = true;
    window->xsettings = xsettings_client_new(window->display, DefaultScreen(window->display), X11Toolkit_SettingsNotify, NULL, window);
    window->xsettings_first_time = false;
    window->scale = X11Toolkit_GetUIScale(window->xsettings, window->display);
    window->iscale = (int)SDL_ceilf(window->scale);
    if (roundf(window->scale) == window->scale) {
        window->scale = 0;
    }
    
#ifdef X_HAVE_UTF8_STRING
    window->utf8 = true;
    window->font_set = NULL;
    if (SDL_X11_HAVE_UTF8) {
        char **missing = NULL;
        int num_missing = 0;
        int i_font;
        window->font_struct = NULL;
        for (i_font = 0; g_ToolkitFont[i_font]; ++i_font) {
            char *font;
            
            SDL_asprintf(&font, g_ToolkitFont[i_font], G_TOOLKITFONT_SIZE * window->iscale);
            window->font_set = X11_XCreateFontSet(window->display, font,
                                                &missing, &num_missing, NULL);
            SDL_free(font);
            if (missing) {
                X11_XFreeStringList(missing);
            }
            if (window->font_set) {
                break;
            }
        }
        if (!window->font_set) {
            goto load_font_traditional;
        }
    } else
#endif
    {
        char *font;
        load_font_traditional:

        SDL_asprintf(&font, g_ToolkitFontLatin1, G_TOOLKITFONT_SIZE * window->iscale);
        window->font_struct = X11_XLoadQueryFont(window->display, font);
        SDL_free(font);
        window->utf8 = false;    
        if (!window->font_struct) {
            ErrorCloseFreeRetNull("Couldn't load font %s", g_ToolkitFontLatin1, window);
        }
    }
     
    if (!colorhints) {
        colorhints = g_default_colors;
    }
    window->color_hints = colorhints;

    // Convert colors to 16 bpc XColor format
    for (i = 0; i < SDL_MESSAGEBOX_COLOR_COUNT; i++) {
        window->xcolor[i].flags = DoRed|DoGreen|DoBlue;
        window->xcolor[i].red = colorhints[i].r * 257;
        window->xcolor[i].green = colorhints[i].g * 257;
        window->xcolor[i].blue = colorhints[i].b * 257;
    }
    
    /* Generate bevel and pressed colors */
    window->xcolor_bevel_l1.flags = DoRed|DoGreen|DoBlue;
    window->xcolor_bevel_l1.red = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].red + 12500, 0, 65535);
    window->xcolor_bevel_l1.green = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].green + 12500, 0, 65535);
    window->xcolor_bevel_l1.blue = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].blue + 12500, 0, 65535);

    window->xcolor_bevel_l2.flags = DoRed|DoGreen|DoBlue;
    window->xcolor_bevel_l2.red = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].red + 32500, 0, 65535);
    window->xcolor_bevel_l2.green = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].green + 32500, 0, 65535);
    window->xcolor_bevel_l2.blue = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].blue + 32500, 0, 65535);

    window->xcolor_bevel_d.flags = DoRed|DoGreen|DoBlue;
    window->xcolor_bevel_d.red = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].red - 22500, 0, 65535);
    window->xcolor_bevel_d.green = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].green - 22500, 0, 65535);
    window->xcolor_bevel_d.blue = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].blue - 22500, 0, 65535);
    
    window->xcolor_pressed.flags = DoRed|DoGreen|DoBlue;
    window->xcolor_pressed.red = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].red - 12500, 0, 65535);
    window->xcolor_pressed.green = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].green - 12500, 0, 65535);
    window->xcolor_pressed.blue = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].blue - 12500, 0, 65535);
     
    window->xcolor_disabled_text.flags = DoRed|DoGreen|DoBlue;
    window->xcolor_disabled_text.red = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].red + 19500, 0, 65535);
    window->xcolor_disabled_text.green = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].green + 19500, 0, 65535);
    window->xcolor_disabled_text.blue = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].blue + 19500, 0, 65535);
      
    
    /* Screen */
    window->parent = parent;
    if (parent) {
        SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(parent);
        window->screen = displaydata->screen;
    } else {
        window->screen = DefaultScreen(window->display);
    }
	
	/* Visuals */
	if (mode == SDL_TOOLKIT_WINDOW_MODE_X11_CHILD) {
	    window->visual = parent->internal->visual;
		window->cmap = parent->internal->colormap;
		X11_GetVisualInfoFromVisual(window->display, window->visual, &window->vi);
  		window->depth = window->vi.depth;	
	} else {
		window->visual = DefaultVisual(window->display, window->screen); 
		window->cmap = DefaultColormap(window->display, window->screen);
		window->depth = DefaultDepth(window->display, window->screen);
		X11_GetVisualInfoFromVisual(window->display, window->visual, &window->vi);		
	}

	/* Allocate colors */
    for (i = 0; i < SDL_MESSAGEBOX_COLOR_COUNT; i++) {
        X11_XAllocColor(window->display, window->cmap, &window->xcolor[i]);    
    }
    X11_XAllocColor(window->display, window->cmap, &window->xcolor_bevel_l1);    
    X11_XAllocColor(window->display, window->cmap, &window->xcolor_bevel_l2);    
    X11_XAllocColor(window->display, window->cmap, &window->xcolor_bevel_d);        
    X11_XAllocColor(window->display, window->cmap, &window->xcolor_pressed);    
	X11_XAllocColor(window->display, window->cmap, &window->xcolor_disabled_text);        
      
    /* Control list */
    window->has_focus = false;
    window->controls = NULL;
    window->controls_sz = 0;
    window->dyn_controls_sz = 0;
    window->fiddled_control = NULL;
    window->dyn_controls = NULL;
    
    /* Menu windows */
    window->popup_windows = NULL;
  
    return window;
}

static void X11Toolkit_AddControlToWindow(SDL_ToolkitWindowX11 *window, SDL_ToolkitControlX11 *control) {
	/* Add to controls list */
    window->controls_sz++;
    if (window->controls_sz == 1) {
        window->controls = (struct SDL_ToolkitControlX11 **)SDL_malloc(sizeof(struct SDL_ToolkitControlX11 *));
    } else {
        window->controls = (struct SDL_ToolkitControlX11 **)SDL_realloc(window->controls, sizeof(struct SDL_ToolkitControlX11 *) * window->controls_sz);        
    }
    window->controls[window->controls_sz - 1] = control;
    
    /* If dynamic, add it to the dynamic controls list too */
    if (control->dynamic) {
        window->dyn_controls_sz++;
        if (window->dyn_controls_sz == 1) {
            window->dyn_controls = (struct SDL_ToolkitControlX11 **)SDL_malloc(sizeof(struct SDL_ToolkitControlX11 *));
        } else {
            window->dyn_controls = (struct SDL_ToolkitControlX11 **)SDL_realloc(window->dyn_controls, sizeof(struct SDL_ToolkitControlX11 *) * window->dyn_controls_sz);        
        }
        window->dyn_controls[window->dyn_controls_sz - 1] = control;        
    }

	/* If selected, set currently focused control to it */
    if (control->selected) {
        window->focused_control = control;
    }    
}

bool X11Toolkit_CreateWindowRes(SDL_ToolkitWindowX11 *data, int w, int h, int cx, int cy, char *title)
{
    int x, y;
    XSizeHints *sizehints;
    XSetWindowAttributes wnd_attr;
    Atom _NET_WM_WINDOW_TYPE, _NET_WM_WINDOW_TYPE_DIALOG, _NET_WM_WINDOW_TYPE_DROPDOWN_MENU, _NET_WM_WINDOW_TYPE_TOOLTIP;
    SDL_WindowData *windowdata = NULL;
    Display *display = data->display;
    XGCValues ctx_vals;
    Window root_win;
    Window parent_win;
    unsigned long gcflags = GCForeground | GCBackground;
    unsigned long valuemask;
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
#ifdef XRANDR_DISABLED_BY_DEFAULT
    const bool use_xrandr_by_default = false;
#else
    const bool use_xrandr_by_default = true;
#endif
#endif
	
    if (data->scale == 0) {
        data->window_width = w;    
        data->window_height = h;    
    } else {
        data->window_width = SDL_lroundf((w/data->iscale) * data->scale);
        data->window_height = SDL_lroundf((h/data->iscale) * data->scale);
        data->pixmap_width = w;
        data->pixmap_height = h;
        data->pixmap = true;
    }
    
    if (data->parent) {
        windowdata = data->parent->internal;
    }
    
	valuemask = CWEventMask | CWColormap;
    data->event_mask = ExposureMask |
                       ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
                       StructureNotifyMask | FocusChangeMask | PointerMotionMask;
    wnd_attr.event_mask = data->event_mask;
    wnd_attr.colormap = data->cmap;
	if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_MENU || data->mode== SDL_TOOLKIT_WINDOW_MODE_X11_TOOLTIP) {
		valuemask |= CWOverrideRedirect | CWSaveUnder;
		wnd_attr.save_under = True;
		wnd_attr.override_redirect = True;
	}
    root_win = RootWindow(display, data->screen);
    if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_CHILD) {
		parent_win = windowdata->xwindow;
	} else {
		parent_win = root_win;
	}
	
    data->window = X11_XCreateWindow(
        display, parent_win,
        0, 0,
        data->window_width, data->window_height,
        0, data->depth, InputOutput, data->visual,
        valuemask, &wnd_attr);
    if (data->window == None) {
        return SDL_SetError("Couldn't create X window");
    }

    if (windowdata && data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_DIALOG) {
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
    }

    if (windowdata && data->mode != SDL_TOOLKIT_WINDOW_MODE_X11_CHILD) {
		X11_XSetTransientForHint(display, data->window, windowdata->xwindow);
	}
	
    if (data->tk_parent) {
		X11_XSetTransientForHint(display, data->window, data->tk_parent->window);
	}

    SDL_X11_SetWindowTitle(display, data->window, title);

    // Let the window manager the type of the window
    if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_DIALOG) {
		_NET_WM_WINDOW_TYPE = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
		_NET_WM_WINDOW_TYPE_DIALOG = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
		X11_XChangeProperty(display, data->window, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
							PropModeReplace,
							(unsigned char *)&_NET_WM_WINDOW_TYPE_DIALOG, 1);
	} else if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_MENU) {
		_NET_WM_WINDOW_TYPE = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
		_NET_WM_WINDOW_TYPE_DROPDOWN_MENU = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
		X11_XChangeProperty(display, data->window, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
							PropModeReplace,
							(unsigned char *)&_NET_WM_WINDOW_TYPE_DROPDOWN_MENU, 1);
	} else if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_TOOLTIP) {
		_NET_WM_WINDOW_TYPE = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
		_NET_WM_WINDOW_TYPE_TOOLTIP = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
		X11_XChangeProperty(display, data->window, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
							PropModeReplace,
							(unsigned char *)&_NET_WM_WINDOW_TYPE_TOOLTIP, 1);
	} 	

    // Allow the window to be deleted by the window manager
    data->wm_delete_message = X11_XInternAtom(display, "WM_DELETE_WINDOW", False);
    X11_XSetWMProtocols(display, data->window, &data->wm_delete_message, 1);

    data->wm_protocols = X11_XInternAtom(display, "WM_PROTOCOLS", False);

	if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_MENU || data->mode== SDL_TOOLKIT_WINDOW_MODE_X11_TOOLTIP) {
		x = cx;
		y = cy;
		goto MOVEWINDOW;
	}
    if (windowdata) {
        XWindowAttributes attrib;
        Window dummy;

        X11_XGetWindowAttributes(display, windowdata->xwindow, &attrib);
        x = attrib.x + (attrib.width - data->window_width) / 2;
        y = attrib.y + (attrib.height - data->window_height) / 3;
        X11_XTranslateCoordinates(display, windowdata->xwindow, RootWindow(display, data->screen), x, y, &x, &y, &dummy);
    } else {
        const SDL_VideoDevice *dev = SDL_GetVideoDevice();
        if (dev && dev->displays && dev->num_displays > 0) {
            const SDL_VideoDisplay *dpy = dev->displays[0];
            const SDL_DisplayData *dpydata = dpy->internal;
            x = dpydata->x + ((dpy->current_mode->w - data->window_width) / 2);
            y = dpydata->y + ((dpy->current_mode->h - data->window_height) / 3);
        }
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
        else if (SDL_GetHintBoolean(SDL_HINT_VIDEO_X11_XRANDR, use_xrandr_by_default) && data->xrandr) {
            XRRScreenResources *screen_res;            
            XRRCrtcInfo *crtc_info;
            RROutput default_out;
        
            screen_res = X11_XRRGetScreenResourcesCurrent(display, root_win);
            if (!screen_res) {
                goto NOXRANDR;
            }
            
            default_out = X11_XRRGetOutputPrimary(display, root_win);
            if (default_out != None) {
                XRROutputInfo *out_info;
    
                out_info = X11_XRRGetOutputInfo(display, screen_res, default_out);            
                if (out_info->connection != RR_Connected) {        
                    X11_XRRFreeOutputInfo(out_info);
                    goto FIRSTOUTPUTXRANDR;
                }        
                
                crtc_info = X11_XRRGetCrtcInfo(display, screen_res, out_info->crtc);
                if (crtc_info) {
                    x = (crtc_info->width - data->window_width) / 2;
                    y = (crtc_info->height - data->window_height) / 3;
                    X11_XRRFreeOutputInfo(out_info);
                    X11_XRRFreeCrtcInfo(crtc_info);
                    X11_XRRFreeScreenResources(screen_res);
                } else {
                    X11_XRRFreeOutputInfo(out_info);
                    goto NOXRANDR;
                }    
            } else {
                    FIRSTOUTPUTXRANDR:
                    if (screen_res->noutput > 0) {
                        XRROutputInfo *out_info;
    
                        out_info = X11_XRRGetOutputInfo(display, screen_res, screen_res->outputs[0]);            
                        if (!out_info) {
                            goto FIRSTCRTCXRANDR;
                        }
                        
                        crtc_info = X11_XRRGetCrtcInfo(display, screen_res, out_info->crtc);
                        if (!crtc_info) {
                            X11_XRRFreeOutputInfo(out_info);
                            goto FIRSTCRTCXRANDR;
                        }
                        
                        x = (crtc_info->width - data->window_width) / 2;
                        y = (crtc_info->height - data->window_height) / 3;
                        X11_XRRFreeOutputInfo(out_info);
                        X11_XRRFreeCrtcInfo(crtc_info);
                        X11_XRRFreeScreenResources(screen_res);
                        goto MOVEWINDOW;
                    }
                    
                    FIRSTCRTCXRANDR:
                    if (!screen_res->ncrtc) {
                        X11_XRRFreeScreenResources(screen_res);
                        goto NOXRANDR;
                    }

                    crtc_info = X11_XRRGetCrtcInfo(display, screen_res, screen_res->crtcs[0]);
                    if (crtc_info) {
                        x = (crtc_info->width - data->window_width) / 2;
                        y = (crtc_info->height - data->window_height) / 3;
                        X11_XRRFreeCrtcInfo(crtc_info);
                        X11_XRRFreeScreenResources(screen_res);
                    } else {
                        X11_XRRFreeScreenResources(screen_res);
                        goto NOXRANDR;
                    }        
            }
        }
#endif
        else {
            // oh well. This will misposition on a multi-head setup. Init first next time.
            NOXRANDR:
            x = (DisplayWidth(display, data->screen) - data->window_width) / 2;
            y = (DisplayHeight(display, data->screen) - data->window_height) / 3;
        }
    }
    MOVEWINDOW:
    X11_XMoveWindow(display, data->window, x, y);
	data->window_x = x;
	data->window_y = y;
	
    sizehints = X11_XAllocSizeHints();
    if (sizehints) {
        sizehints->flags = USPosition | USSize | PMaxSize | PMinSize;
        sizehints->x = x;
        sizehints->y = y;
        sizehints->width = data->window_width;
        sizehints->height = data->window_height;

        sizehints->min_width = sizehints->max_width = data->window_width;
        sizehints->min_height = sizehints->max_height = data->window_height;

        X11_XSetWMNormalHints(display, data->window, sizehints);

        X11_XFree(sizehints);
    }

    X11_XMapRaised(display, data->window);
    
    data->drawable = data->window;
    
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    // Initialise a back buffer for double buffering
    if (SDL_X11_HAVE_XDBE && !data->pixmap) {
        int xdbe_major, xdbe_minor;
        if (X11_XdbeQueryExtension(display, &xdbe_major, &xdbe_minor) != 0) {
            data->xdbe = true;
            data->buf = X11_XdbeAllocateBackBufferName(display, data->window, XdbeUndefined);
            data->drawable = data->buf;
        } else {
            data->xdbe = false;
        }
    }
#endif

    if (data->pixmap) { 
        data->drawable = X11_XCreatePixmap(display, data->window, data->pixmap_width, data->pixmap_height, data->depth);
    }
    
    SDL_zero(ctx_vals);
    ctx_vals.foreground = data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].pixel;
    ctx_vals.background = data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].pixel;
    if (!data->utf8) {
        gcflags |= GCFont;
        ctx_vals.font = data->font_struct->fid;
    }
    data->ctx = X11_XCreateGC(data->display, data->drawable, gcflags, &ctx_vals);
    if (data->ctx == None) {
        return SDL_SetError("Couldn't create graphics context");
    }

    data->close = false;
    data->key_control_esc = data->key_control_enter = NULL;
    if (!data->pixmap) {
        data->ev_scale = data->ev_iscale = 1;
    } else {
        data->ev_scale = data->scale;
        data->ev_iscale = data->iscale;
    }
    
#if SDL_GRAB
	if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_MENU || data->mode== SDL_TOOLKIT_WINDOW_MODE_X11_TOOLTIP) {
		X11_XGrabPointer(display, data->window, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
		X11_XGrabKeyboard(display, data->window, False, GrabModeAsync, GrabModeAsync, CurrentTime);
	}
#endif
	
    return true;
}

static void X11Toolkit_DrawWindow(SDL_ToolkitWindowX11 *data) {
    int i;
    
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    if (SDL_X11_HAVE_XDBE && data->xdbe && !data->pixmap) {
        X11_XdbeBeginIdiom(data->display);
    }
#endif

    X11_XSetForeground(data->display, data->ctx, data->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].pixel);
    if (data->pixmap) {
        X11_XFillRectangle(data->display, data->drawable, data->ctx, 0, 0, data->pixmap_width, data->pixmap_height);
    } else {
        X11_XFillRectangle(data->display, data->drawable, data->ctx, 0, 0, data->window_width, data->window_height);
    }
    
    for (i = 0; i < data->controls_sz; i++) {
        SDL_ToolkitControlX11 *control;
        
        control = data->controls[i];
        if (control) {
            if (control->func_draw) {
                control->func_draw(control);
            }
        }
    }
       
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    if (SDL_X11_HAVE_XDBE && data->xdbe && !data->pixmap) {
        XdbeSwapInfo swap_info;
        swap_info.swap_window = data->window;
        swap_info.swap_action = XdbeUndefined;
        X11_XdbeSwapBuffers(data->display, &swap_info, 1);
        X11_XdbeEndIdiom(data->display);
    }
#endif

    if (data->pixmap) {
        XImage *pixmap_image;
        XImage *put_image;
        SDL_Surface *pixmap_surface;
        SDL_Surface *put_surface;
        
        /* FIXME: Implement SHM transport? */
        pixmap_image = X11_XGetImage(data->display, data->drawable, 0, 0 , data->pixmap_width, data->pixmap_height, AllPlanes, ZPixmap);    
        pixmap_surface = SDL_CreateSurfaceFrom(data->pixmap_width, data->pixmap_height, X11_GetPixelFormatFromVisualInfo(data->display, &data->vi), pixmap_image->data, pixmap_image->bytes_per_line);
        put_surface = SDL_ScaleSurface(pixmap_surface, data->window_width, data->window_height, SDL_SCALEMODE_LINEAR);
        put_image = X11_XCreateImage(data->display, data->visual, data->vi.depth, ZPixmap, 0, put_surface->pixels, data->window_width, data->window_height, 32, put_surface->pitch);
        X11_XPutImage(data->display, data->window, data->ctx, put_image, 0, 0, 0, 0, data->window_width, data->window_height);
        
        X11_XDestroyImage(pixmap_image);
        /* Needed because XDestroyImage results in a double-free otherwise */
        put_image->data = NULL;
        X11_XDestroyImage(put_image);
        SDL_DestroySurface(pixmap_surface);
        SDL_DestroySurface(put_surface);
    }

    X11_XFlush(data->display);
}

static SDL_ToolkitControlX11 *X11Toolkit_GetControlMouseIsOn(SDL_ToolkitWindowX11 *data, int x, int y)
{
    int i;

    for (i = 0; i < data->controls_sz; i++) {
        SDL_Rect *rect = &data->controls[i]->rect;
        if ((x >= rect->x) &&
            (x <= (rect->x + rect->w)) &&
            (y >= rect->y) &&
            (y <= (rect->y + rect->h))) {
            return data->controls[i];
        }
    }

    return NULL;
}

// NOLINTNEXTLINE(readability-non-const-parameter): cannot make XPointer a const pointer due to typedef
static Bool X11Toolkit_EventTest(Display *display, XEvent *event, XPointer arg)
{
    SDL_ToolkitWindowX11 *data = (SDL_ToolkitWindowX11 *)arg;
	        
    if (event->xany.display != data->display) {
		return False;
	}
	
	if (event->xany.window == data->window) {
		return True;
	} 

	return False;
}

void X11Toolkit_ProcessWindowEvents(SDL_ToolkitWindowX11 *data, XEvent *e) {
	/* If X11_XFilterEvent returns True, then some input method has filtered the
		event, and the client should discard the event. */
	if ((e->type != Expose) && X11_XFilterEvent(e, None)) {
		return;
	}

	data->draw = false;
	data->e = e;
	
	switch (e->type) {
        case Expose:
            data->draw = true;
            break;
        case ClientMessage:
            if (e->xclient.message_type == data->wm_protocols &&
                e->xclient.format == 32 &&
                e->xclient.data.l[0] == data->wm_delete_message) {
                data->close = true;
            }
            break;
        case FocusIn:
            data->has_focus = true;
            break;
        case FocusOut:
            data->has_focus = false;
            if (data->fiddled_control) {
                data->fiddled_control->selected = false;
            }
            data->fiddled_control = NULL;
            for (data->ev_i = 0; data->ev_i < data->controls_sz; data->ev_i++) {
                data->controls[data->ev_i]->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
            }
            break;
        case MotionNotify:
            if (data->has_focus) {                
                data->previous_control = data->fiddled_control;
                data->fiddled_control = X11Toolkit_GetControlMouseIsOn(data, SDL_lroundf((e->xbutton.x/ data->ev_scale)* data->ev_iscale), SDL_lroundf((e->xbutton.y/ data->ev_scale)* data->ev_iscale));
                if (data->previous_control) {
                    data->previous_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
                    if (data->previous_control->func_on_state_change) {
                        data->previous_control->func_on_state_change(data->previous_control);
                    }        
                    data->draw = true;
                }
                if (data->fiddled_control) {
                    if (data->fiddled_control->dynamic) {
                        data->fiddled_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_HOVER;
						if (data->fiddled_control->func_on_state_change) {
							data->fiddled_control->func_on_state_change(data->fiddled_control);
						}      
                        data->draw = true;    
                    } else {
                        data->fiddled_control = NULL;
                    }
                } 
            }
            break;
        case ButtonPress:
            data->previous_control = data->fiddled_control;
            if (data->previous_control) {
                data->previous_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
				if (data->previous_control->func_on_state_change) {
					data->previous_control->func_on_state_change(data->previous_control);
				}
                data->draw = true;
            }
            if (e->xbutton.button == Button1) {
                data->fiddled_control = X11Toolkit_GetControlMouseIsOn(data, SDL_lroundf((e->xbutton.x/ data->ev_scale)* data->ev_iscale), SDL_lroundf((e->xbutton.y/ data->ev_scale)* data->ev_iscale));
                if (data->fiddled_control) {
                    data->fiddled_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED_HELD;
					if (data->fiddled_control->func_on_state_change) {
						data->fiddled_control->func_on_state_change(data->fiddled_control);
					}   
                    data->draw = true;
                } 
            }
            break;
        case ButtonRelease:
			if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_MENU || data->mode== SDL_TOOLKIT_WINDOW_MODE_X11_TOOLTIP) {
				int cx;
				int cy;
				
				cx = e->xbutton.x;
				cy = e->xbutton.y;
				
				if (cy < 0 || cx < 0) {
					data->close = true;
				}
				
				if (cy > data->window_height || cx > data->window_width) {
					data->close = true;
				}
			}

            if ((e->xbutton.button == Button1) && (data->fiddled_control)) {
                SDL_ToolkitControlX11 *control;
                
                control = X11Toolkit_GetControlMouseIsOn(data, SDL_lroundf((e->xbutton.x/ data->ev_scale)* data->ev_iscale), SDL_lroundf((e->xbutton.y/ data->ev_scale)* data->ev_iscale));
                if (data->fiddled_control == control) {
                    data->fiddled_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED;
                    if (data->fiddled_control->func_on_state_change) {
                        data->fiddled_control->func_on_state_change(data->fiddled_control);
                    }
                    data->fiddled_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
                    data->draw = true;
                }
            }
            break;
        case KeyPress:
            data->last_key_pressed = X11_XLookupKeysym(&e->xkey, 0);
           
            if (data->last_key_pressed == XK_Escape) {
                for (data->ev_i = 0; data->ev_i < data->controls_sz; data->ev_i++) {                
                    if(data->controls[data->ev_i]->is_default_esc) {
                        data->controls[data->ev_i]->state = SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED;
                        data->draw = true;
                        data->key_control_esc = data->controls[data->ev_i];
                    }
                }
            } else if ((data->last_key_pressed == XK_Return) || (data->last_key_pressed == XK_KP_Enter)) {
                for (data->ev_i = 0; data->ev_i < data->controls_sz; data->ev_i++) {                
                    if(data->controls[data->ev_i]->selected) {
                        data->controls[data->ev_i]->state = SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED;
                        data->draw = true;
                        data->key_control_enter = data->controls[data->ev_i];
                    }
                }
            }
            break;
        case KeyRelease:
        {
            KeySym key = X11_XLookupKeysym(&e->xkey, 0);
            
			if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_MENU || data->mode== SDL_TOOLKIT_WINDOW_MODE_X11_TOOLTIP) {
				data->close = true;
			}
			
            // If this is a key release for something we didn't get the key down for, then bail.
            if (key != data->last_key_pressed) {
                break;
            }
            
            if (key == XK_Escape) {
                if (data->key_control_esc) {
                    if (data->key_control_esc->func_on_state_change) {
                        data->key_control_esc->func_on_state_change(data->key_control_esc);
                    }        
                }
            } else if ((key == XK_Return) || (key == XK_KP_Enter)) {
                if (data->key_control_enter) {
                    if (data->key_control_enter->func_on_state_change) {
                        data->key_control_enter->func_on_state_change(data->key_control_enter);
                    }        
                }
            } else if (key == XK_Tab || key == XK_Left || key == XK_Right) {
				if (data->focused_control) {
					data->focused_control->selected = false;
				}
                data->draw = true;
                for (data->ev_i = 0; data->ev_i < data->dyn_controls_sz; data->ev_i++) {                
                    if (data->dyn_controls[data->ev_i] == data->focused_control) {
                        int next_index;
                        
                        if (key == XK_Left) {
                            next_index = data->ev_i - 1;
                        } else { 
                            next_index = data->ev_i + 1;
                        }
                        if ((next_index >= data->dyn_controls_sz) || (next_index < 0)) {
                            if (key == XK_Right || key == XK_Left) {
                                next_index = data->ev_i;
                            } else {
                                next_index = 0;
                            }
                        }
                        data->focused_control = data->dyn_controls[next_index];
                        data->focused_control->selected = true;
                        break;
                    }
                }
            }
            break;
            }

	}		
        
	if (data->draw) {
		X11Toolkit_DrawWindow(data);
	}
}

void X11Toolkit_DoWindowEventLoop(SDL_ToolkitWindowX11 *data) {
   while (!data->close) {
		XEvent e;
	
		/* Process settings events */
		X11_XPeekEvent(data->display, &e);
		if (data->xsettings) {
            xsettings_client_process_event(data->xsettings, &e);
        }  
		
		/* Do actual event loop */
        X11_XIfEvent(data->display, &e, X11Toolkit_EventTest, (XPointer)data);
        X11Toolkit_ProcessWindowEvents(data, &e);
	}
}


void X11Toolkit_ResizeWindow(SDL_ToolkitWindowX11 *data, int w, int h) {    
    if (!data->pixmap) {
        data->window_width = w;    
        data->window_height = h;    
    } else {
        data->window_width = SDL_lroundf((w/data->iscale) * data->scale);
        data->window_height = SDL_lroundf((h/data->iscale) * data->scale);
        data->pixmap_width = w;
        data->pixmap_height = h;
        X11_XFreePixmap(data->display, data->drawable);
        data->drawable = X11_XCreatePixmap(data->display, data->window, data->pixmap_width, data->pixmap_height, data->depth);        
    }

    X11_XResizeWindow(data->display, data->window, data->window_width, data->window_height);
}

static void X11Toolkit_DestroyIconControl(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitIconControlX11 *icon_control;
    
    icon_control = (SDL_ToolkitIconControlX11 *)control;
    X11_XFreeFont(control->window->display, icon_control->icon_char_font);
    SDL_free(control);
}

static void X11Toolkit_DrawIconControl(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitIconControlX11 *icon_control;

    icon_control = (SDL_ToolkitIconControlX11 *)control;
    control->rect.w -= 2 * control->window->iscale;
    control->rect.h -= 2 * control->window->iscale;
    X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_bg_shadow.pixel);
    X11_XFillArc(control->window->display, control->window->drawable, control->window->ctx, control->rect.x + (2 * control->window->iscale), control->rect.y + (2* control->window->iscale), control->rect.w, control->rect.h, 0, 360 * 64);
    
    switch (icon_control->flags & (SDL_MESSAGEBOX_ERROR | SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_INFORMATION)) {
        case SDL_MESSAGEBOX_ERROR:
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_red_darker.pixel);
                X11_XFillArc(control->window->display, control->window->drawable, control->window->ctx, control->rect.x, control->rect.y, control->rect.w, control->rect.h, 0, 360 * 64);
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_red.pixel);
                X11_XFillArc(control->window->display, control->window->drawable, control->window->ctx, control->rect.x+(1* control->window->iscale), control->rect.y+(1* control->window->iscale), control->rect.w-(2* control->window->iscale), control->rect.h-(2* control->window->iscale), 0, 360 * 64);
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_white.pixel);
                break;
        case SDL_MESSAGEBOX_WARNING:
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_black.pixel);
                X11_XFillArc(control->window->display, control->window->drawable, control->window->ctx, control->rect.x, control->rect.y, control->rect.w, control->rect.h, 0, 360 * 64);
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_yellow.pixel);
                X11_XFillArc(control->window->display, control->window->drawable, control->window->ctx, control->rect.x+(1* control->window->iscale), control->rect.y+(1* control->window->iscale), control->rect.w-(2* control->window->iscale), control->rect.h-(2* control->window->iscale), 0, 360 * 64);
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_black.pixel);
                break;
        case SDL_MESSAGEBOX_INFORMATION:
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_white.pixel);
                X11_XFillArc(control->window->display, control->window->drawable, control->window->ctx, control->rect.x, control->rect.y, control->rect.w, control->rect.h, 0, 360 * 64);
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_blue.pixel);
                X11_XFillArc(control->window->display, control->window->drawable, control->window->ctx, control->rect.x+(1* control->window->iscale), control->rect.y+(1* control->window->iscale), control->rect.w-(2* control->window->iscale), control->rect.h-(2* control->window->iscale), 0, 360 * 64);
                X11_XSetForeground(control->window->display, control->window->ctx, icon_control->xcolor_white.pixel);
                break;
    }
    X11_XSetFont(control->window->display, control->window->ctx, icon_control->icon_char_font->fid); 
    X11_XDrawString(control->window->display, control->window->drawable, control->window->ctx, control->rect.x + icon_control->icon_char_x, control->rect.y + icon_control->icon_char_y, &icon_control->icon_char, 1);
    if (!control->window->utf8) {
        X11_XSetFont(control->window->display, control->window->ctx, control->window->font_struct->fid); 
    }
    
    control->rect.w += 2 * control->window->iscale;
    control->rect.h += 2 * control->window->iscale;
}

static void X11Toolkit_CalculateIconControl(SDL_ToolkitControlX11 *base_control) {
    SDL_ToolkitIconControlX11 *control;
    int icon_char_w;
    int icon_char_h;
    int icon_char_max;
    
    control = (SDL_ToolkitIconControlX11 *)base_control;
    X11Toolkit_GetTextWidthHeightForFont(control->icon_char_font, &control->icon_char, 1, &icon_char_w, &icon_char_h, &control->icon_char_a);
    base_control->rect.w = icon_char_w + SDL_TOOLKIT_X11_ELEMENT_PADDING * 2 * base_control->window->iscale;
    base_control->rect.h = icon_char_h + SDL_TOOLKIT_X11_ELEMENT_PADDING * 2 * base_control->window->iscale;
    icon_char_max = SDL_max(base_control->rect.w, base_control->rect.h) + 2;
    base_control->rect.w = icon_char_max;
    base_control->rect.h = icon_char_max;        
    base_control->rect.y = 0;
    base_control->rect.x = 0;
    control->icon_char_y = control->icon_char_a + (base_control->rect.h - icon_char_h)/2 + 1;
    control->icon_char_x = (base_control->rect.w - icon_char_w)/2 + 1;
    base_control->rect.w += 2 * base_control->window->iscale;
    base_control->rect.h += 2 * base_control->window->iscale;
}

static void X11Toolkit_OnIconControlScaleChange(SDL_ToolkitControlX11 *base_control) {
    SDL_ToolkitIconControlX11 *control;
    char *font;
    
    control = (SDL_ToolkitIconControlX11 *)base_control;
    X11_XFreeFont(base_control->window->display, control->icon_char_font);
    SDL_asprintf(&font, g_IconFont, G_ICONFONT_SIZE * base_control->window->iscale);
    control->icon_char_font = X11_XLoadQueryFont(base_control->window->display, font);
    SDL_free(font);
    if (!control->icon_char_font) {
        SDL_asprintf(&font, g_ToolkitFontLatin1, G_TOOLKITFONT_SIZE * base_control->window->iscale);
        control->icon_char_font = X11_XLoadQueryFont(base_control->window->display, font);
        SDL_free(font);
    } 
}

SDL_ToolkitControlX11 *X11Toolkit_CreateIconControl(SDL_ToolkitWindowX11 *window, SDL_MessageBoxFlags flags) {
    SDL_ToolkitIconControlX11 *control;
    SDL_ToolkitControlX11 *base_control;
    char *font;

    /* Create control struct */
    control = (SDL_ToolkitIconControlX11 *)SDL_malloc(sizeof(SDL_ToolkitIconControlX11));
    base_control = (SDL_ToolkitControlX11 *)control;
    if (!control) {
        SDL_SetError("Unable to allocate icon control structure");
        return NULL;
    }
    
    /* Fill out struct */
    base_control->window = window;
    base_control->func_draw = X11Toolkit_DrawIconControl;
    base_control->func_free = X11Toolkit_DestroyIconControl;
    base_control->func_on_state_change = NULL;
    base_control->func_calc_size = X11Toolkit_CalculateIconControl;
    base_control->func_on_scale_change = X11Toolkit_OnIconControlScaleChange;
    base_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
    base_control->selected = false;
    base_control->dynamic = false;  
    base_control->is_default_enter = false;  
    base_control->is_default_esc = false;  
    control->flags = flags;
    
    /* Load font */
    SDL_asprintf(&font, g_IconFont, G_ICONFONT_SIZE * window->iscale);
    control->icon_char_font = X11_XLoadQueryFont(window->display, font);
    SDL_free(font);
    if (!control->icon_char_font) {
        SDL_asprintf(&font, g_ToolkitFontLatin1, G_TOOLKITFONT_SIZE * window->iscale);
        control->icon_char_font = X11_XLoadQueryFont(window->display, font);
        SDL_free(font);
        if (!control->icon_char_font) {
            SDL_free(control);
            return NULL;
        }
    } 
    
    /* Set colors */
    switch (flags & (SDL_MESSAGEBOX_ERROR | SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_INFORMATION)) {
    case SDL_MESSAGEBOX_ERROR:
        control->icon_char = 'X';
        control->xcolor_white.flags = DoRed|DoGreen|DoBlue;
        control->xcolor_white.red = 65535;
        control->xcolor_white.green = 65535;
        control->xcolor_white.blue = 65535;
        control->xcolor_red.flags = DoRed|DoGreen|DoBlue;
        control->xcolor_red.red = 65535;
        control->xcolor_red.green = 0;
        control->xcolor_red.blue = 0;
        control->xcolor_red_darker.flags = DoRed|DoGreen|DoBlue;
        control->xcolor_red_darker.red = 40535;
        control->xcolor_red_darker.green = 0;
        control->xcolor_red_darker.blue = 0;
        X11_XAllocColor(window->display, window->cmap, &control->xcolor_white);    
        X11_XAllocColor(window->display, window->cmap, &control->xcolor_red);    
        X11_XAllocColor(window->display, window->cmap, &control->xcolor_red_darker);    
        break;
    case SDL_MESSAGEBOX_WARNING:
        control->icon_char = '!';
        control->xcolor_black.flags = DoRed|DoGreen|DoBlue;
        control->xcolor_black.red = 0;
        control->xcolor_black.green = 0;
        control->xcolor_black.blue = 0;
        control->xcolor_yellow.flags = DoRed|DoGreen|DoBlue;
        control->xcolor_yellow.red = 65535;
        control->xcolor_yellow.green = 65535;
        control->xcolor_yellow.blue = 0;
        X11_XAllocColor(window->display, window->cmap, &control->xcolor_black);    
        X11_XAllocColor(window->display, window->cmap, &control->xcolor_yellow);    
        break;
    case SDL_MESSAGEBOX_INFORMATION:
        control->icon_char = 'i';
        control->xcolor_white.flags = DoRed|DoGreen|DoBlue;
        control->xcolor_white.red = 65535;
        control->xcolor_white.green = 65535;
        control->xcolor_white.blue = 65535;       
        control->xcolor_blue.flags = DoRed|DoGreen|DoBlue;
        control->xcolor_blue.red = 0;
        control->xcolor_blue.green = 0;
        control->xcolor_blue.blue = 65535;   
        X11_XAllocColor(window->display, window->cmap, &control->xcolor_white);    
        X11_XAllocColor(window->display, window->cmap, &control->xcolor_blue);    
        break;
    default:
        X11_XFreeFont(window->display, control->icon_char_font);
        SDL_free(control);
        return NULL;
    }
    control->xcolor_bg_shadow.flags = DoRed|DoGreen|DoBlue;
    control->xcolor_bg_shadow.red = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].red - 12500, 0, 65535);
    control->xcolor_bg_shadow.green = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].green - 12500, 0, 65535);
    control->xcolor_bg_shadow.blue = SDL_clamp(window->xcolor[SDL_MESSAGEBOX_COLOR_BACKGROUND].blue - 12500, 0, 65535);
    X11_XAllocColor(window->display, window->cmap, &control->xcolor_bg_shadow);    

    /* Sizing and positioning */
    X11Toolkit_CalculateIconControl(base_control);
    
    X11Toolkit_AddControlToWindow(window, base_control);       
    return base_control;
}

bool X11Toolkit_NotifyControlOfSizeChange(SDL_ToolkitControlX11 *control) {
    if (control->func_calc_size) {
        control->func_calc_size(control);
        return true;
    } else {
        return false;
    }
}

static void X11Toolkit_CalculateButtonControl(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitButtonControlX11 *button_control;
    int text_d;
    
    button_control = (SDL_ToolkitButtonControlX11 *)control;
    X11Toolkit_GetTextWidthHeight(control->window, button_control->data->text, button_control->str_sz, &button_control->text_rect.w, &button_control->text_rect.h, &button_control->text_a, &text_d);
    //control->rect.w = SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * 2 * control->window->iscale + button_control->text_rect.w;
    //control->rect.h = SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * 2 * control->window->iscale + button_control->text_rect.h;    
    button_control->text_rect.x = (control->rect.w - button_control->text_rect.w)/2;
    button_control->text_rect.y = button_control->text_a + (control->rect.h - button_control->text_rect.h)/2;
    if (control->window->utf8) {
        button_control->text_rect.y -= 2 * control->window->iscale;
    } else {
        button_control->text_rect.y -= 4 * control->window->iscale;    
    }
}


static void X11Toolkit_DrawButtonControl(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitButtonControlX11 *button_control;
    
    button_control = (SDL_ToolkitButtonControlX11 *)control;
    X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);
    /* Draw bevel */
    if (control->state == SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED || control->state == SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED_HELD) {
            X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_d.pixel);
            X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x, control->rect.y,
                                   control->rect.w, control->rect.h);
            
            X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_l2.pixel);
            X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x, control->rect.y,
                                   control->rect.w - (1* control->window->iscale), control->rect.h - (1* control->window->iscale));
                
           
            X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_l1.pixel);
            X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x + 1 * control->window->iscale, control->rect.y + 1 * control->window->iscale,
                                   control->rect.w - 3 * control->window->iscale, control->rect.h - 2 * control->window->iscale);
                                                       
            X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].pixel);
            X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                               control->rect.x + 1 * control->window->iscale, control->rect.y + 1 * control->window->iscale,
                               control->rect.w - 3 * control->window->iscale, control->rect.h - 3 * control->window->iscale);

            X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_pressed.pixel);
            X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                               control->rect.x + 2 * control->window->iscale, control->rect.y + 2 * control->window->iscale,
                               control->rect.w - 4 * control->window->iscale, control->rect.h - 4 * control->window->iscale);
        } else {
            if (control->selected) {
                X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_d.pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x, control->rect.y,
                                   control->rect.w, control->rect.h);

                X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_l2.pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x + 1 * control->window->iscale, control->rect.y + 1 * control->window->iscale,
                                   control->rect.w - 3 * control->window->iscale, control->rect.h - 3 * control->window->iscale);
          
                X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x + 2 * control->window->iscale, control->rect.y + 2 * control->window->iscale,
                                   control->rect.w - 4 * control->window->iscale, control->rect.h - 4 * control->window->iscale);
           
                X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_l1.pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x + 2 * control->window->iscale, control->rect.y + 2 * control->window->iscale,
                                   control->rect.w - 5 * control->window->iscale, control->rect.h - 5 * control->window->iscale);
           
                X11_XSetForeground(control->window->display, control->window->ctx, (control->state == SDL_TOOLKIT_CONTROL_STATE_X11_HOVER) ? control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED].pixel : control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x + 3 * control->window->iscale, control->rect.y + 3 * control->window->iscale,
                                   control->rect.w - 6 * control->window->iscale, control->rect.h - 6 * control->window->iscale);
            } else {
                X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_d.pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x, control->rect.y,
                                   control->rect.w, control->rect.h);

                X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_l2.pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x, control->rect.y,
                                   control->rect.w - 1 * control->window->iscale, control->rect.h - 1 * control->window->iscale);
          
                X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x + 1 * control->window->iscale, control->rect.y + 1 * control->window->iscale,
                                   control->rect.w - 2 * control->window->iscale, control->rect.h - 2 * control->window->iscale);
           
                X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor_bevel_l1.pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x + 1 * control->window->iscale, control->rect.y + 1 * control->window->iscale,
                                   control->rect.w - 3 * control->window->iscale, control->rect.h - 3 * control->window->iscale);
           
                X11_XSetForeground(control->window->display, control->window->ctx, (control->state == SDL_TOOLKIT_CONTROL_STATE_X11_HOVER) ? control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED].pixel : control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].pixel);
                X11_XFillRectangle(control->window->display, control->window->drawable, control->window->ctx,
                                   control->rect.x + 2 * control->window->iscale, control->rect.y + 2 * control->window->iscale,
                                   control->rect.w - 4 * control->window->iscale, control->rect.h - 4 * control->window->iscale);
            }
        }
                                                                                             
        X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);
#ifdef X_HAVE_UTF8_STRING
        if (control->window->utf8) {
            X11_Xutf8DrawString(control->window->display, control->window->drawable, control->window->font_set, control->window->ctx,
                                control->rect.x + button_control->text_rect.x,
                                control->rect.y + button_control->text_rect.y,
                                button_control->data->text, button_control->str_sz);
        } else
#endif
        {
            X11_XDrawString(control->window->display, control->window->drawable, control->window->ctx,
                            control->rect.x + button_control->text_rect.x, control->rect.y + button_control->text_rect.y,
                            button_control->data->text, button_control->str_sz);
        }
}

static void X11Toolkit_OnButtonControlStateChange(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitButtonControlX11 *button_control;
    
    button_control = (SDL_ToolkitButtonControlX11 *)control;
    if (button_control->cb && control->state == SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED) {
        button_control->cb(control, button_control->cb_data);
    }
}

static void X11Toolkit_DestroyGenericControl(SDL_ToolkitControlX11 *control) {
    SDL_free(control);
}

SDL_ToolkitControlX11 *X11Toolkit_CreateButtonControl(SDL_ToolkitWindowX11 *window, const SDL_MessageBoxButtonData *data) {
    SDL_ToolkitButtonControlX11 *control;
    SDL_ToolkitControlX11 *base_control;
    int text_d;
            
    control = (SDL_ToolkitButtonControlX11 *)SDL_malloc(sizeof(SDL_ToolkitButtonControlX11));
    base_control = (SDL_ToolkitControlX11 *)control;
    if (!control) {
        SDL_SetError("Unable to allocate button control structure");
        return NULL;
    }
    base_control->window = window;
    base_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
    base_control->func_calc_size = X11Toolkit_CalculateButtonControl;
    base_control->func_draw = X11Toolkit_DrawButtonControl;
    base_control->func_on_state_change = X11Toolkit_OnButtonControlStateChange;
    base_control->func_free = X11Toolkit_DestroyGenericControl;
    base_control->func_on_scale_change = NULL;
    base_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
    base_control->selected = false;
    base_control->dynamic = true;    
    base_control->is_default_enter = false;  
    base_control->is_default_esc = false;  
    if (data->flags & SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT) {
        base_control->is_default_esc = true;  
    }
    if (data->flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT) {
        base_control->is_default_enter = true;  
        base_control->selected = true;
    }
    control->data = data;
    control->str_sz = SDL_strlen(control->data->text);
    control->cb = NULL;
    X11Toolkit_GetTextWidthHeight(base_control->window, control->data->text, control->str_sz, &control->text_rect.w, &control->text_rect.h, &control->text_a, &text_d);
    base_control->rect.w = SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * 2 * window->iscale + control->text_rect.w;
    base_control->rect.h = SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * 2 * window->iscale + control->text_rect.h;    
    control->text_rect.x = control->text_rect.y = 0;
    X11Toolkit_CalculateButtonControl(base_control);
    X11Toolkit_AddControlToWindow(window, base_control);       
    return base_control;
}

void X11Toolkit_RegisterCallbackForButtonControl(SDL_ToolkitControlX11 *control, void *data, void (*cb)(struct SDL_ToolkitControlX11 *, void *)) {
    SDL_ToolkitButtonControlX11 *button_control;
    
    button_control = (SDL_ToolkitButtonControlX11 *)control;
    button_control->cb_data = data;
    button_control->cb = cb;
}

const SDL_MessageBoxButtonData *X11Toolkit_GetButtonControlData(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitButtonControlX11 *button_control;
    
    button_control = (SDL_ToolkitButtonControlX11 *)control;
    return button_control->data;
}

void X11Toolkit_DestroyWindow(SDL_ToolkitWindowX11 *data) {
    int i;
    
    if (!data) {
        return;
    }
   
#if SDL_GRAB
	if (data->mode == SDL_TOOLKIT_WINDOW_MODE_X11_MENU || data->mode== SDL_TOOLKIT_WINDOW_MODE_X11_TOOLTIP) {
		X11_XUngrabPointer(data->display, CurrentTime);
		X11_XUngrabKeyboard(data->display, CurrentTime);
	}
#endif

    for (i = 0; i < data->controls_sz; i++) {
        if (data->controls[i]->func_free) {
            data->controls[i]->func_free(data->controls[i]);
        }
    }
    if (data->controls) {
	    SDL_free(data->controls);
	}
    if (data->dyn_controls) {
	    SDL_free(data->dyn_controls);
	}
	
	if (data->popup_windows) {
		SDL_ListClear(&data->popup_windows);
	}
	
    if (data->pixmap) { 
        X11_XFreePixmap(data->display, data->drawable);
    }
    
#ifdef X_HAVE_UTF8_STRING
    if (data->font_set) {
        X11_XFreeFontSet(data->display, data->font_set);
        data->font_set = NULL;
    }
#endif

    if (data->font_struct) {
        X11_XFreeFont(data->display, data->font_struct);
        data->font_struct = NULL;
    }

#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    if (SDL_X11_HAVE_XDBE && data->xdbe && !data->pixmap) {
        X11_XdbeDeallocateBackBufferName(data->display, data->buf);
    }
#endif

    if (data->xsettings) {
        xsettings_client_destroy(data->xsettings);
    }
    
    X11_XFreeGC(data->display, data->ctx);

    if (data->display) {
        if (data->window != None) {
            X11_XWithdrawWindow(data->display, data->window, data->screen);
            X11_XDestroyWindow(data->display, data->window);
            data->window = None;
        }
	
		if (data->display_close) {
			X11_XCloseDisplay(data->display);
		}
        data->display = NULL;
    }
    
#if SDL_SET_LOCALE
    if (data->origlocale && (data->mode != SDL_TOOLKIT_WINDOW_MODE_X11_CHILD)) {
        (void)setlocale(LC_ALL, data->origlocale);
        SDL_free(data->origlocale);
    }
#endif

    SDL_free(data);
}

static int X11Toolkit_CountLinesOfText(const char *text)
{
    int result = 0;
    while (text && *text) {
        const char *lf = SDL_strchr(text, '\n');
        result++; // even without an endline, this counts as a line.
        text = lf ? lf + 1 : NULL;
    }
    return result;
}

static void X11Toolkit_DrawLabelControl(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitLabelControlX11 *label_control;
    int i;
    
    label_control = (SDL_ToolkitLabelControlX11 *)control;
    X11_XSetForeground(control->window->display, control->window->ctx, control->window->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);
    for (i = 0; i < label_control->sz; i++) {
#ifdef X_HAVE_UTF8_STRING
        if (control->window->utf8) {
            X11_Xutf8DrawString(control->window->display, control->window->drawable, control->window->font_set, control->window->ctx,
                                control->rect.x, control->rect.y + label_control->y[i],
                                label_control->lines[i], label_control->szs[i]);
        } else
#endif
        {
            X11_XDrawString(control->window->display, control->window->drawable, control->window->ctx,
                                control->rect.x, control->rect.y + label_control->y[i],
                                label_control->lines[i], label_control->szs[i]);
        }
    }
}

static void X11Toolkit_DestroyLabelControl(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitLabelControlX11 *label_control;
    
    label_control = (SDL_ToolkitLabelControlX11 *)control;
    SDL_free(label_control->lines);
    SDL_free(label_control->szs);
    SDL_free(label_control->y);
    SDL_free(label_control);
}


static void X11Toolkit_CalculateLabelControl(SDL_ToolkitControlX11 *base_control) {
    SDL_ToolkitLabelControlX11 *control;
    int ascent;
    int descent;
    int w;
    int h;
    int i;
    
    control = (SDL_ToolkitLabelControlX11 *)base_control;
    for (i = 0; i < control->sz; i++) {
        X11Toolkit_GetTextWidthHeight(base_control->window, control->lines[i], control->szs[i], &w, &h, &ascent, &descent);

        if (i > 0) {
            control->y[i] = ascent + descent + control->y[i-1];
            base_control->rect.h += ascent + descent + h;
        } else {
            control->y[i] = ascent;
            base_control->rect.h = h;
        }
    }
}

SDL_ToolkitControlX11 *X11Toolkit_CreateLabelControl(SDL_ToolkitWindowX11 *window, char *utf8) {
    SDL_ToolkitLabelControlX11 *control;
    SDL_ToolkitControlX11 *base_control;
    int ascent;
    int descent;
    int i;
    
    if (!utf8) {
        return NULL;
    }    
    control = (SDL_ToolkitLabelControlX11 *)SDL_malloc(sizeof(SDL_ToolkitLabelControlX11));
    base_control = (SDL_ToolkitControlX11 *)control;
    if (!control) {
        SDL_SetError("Unable to allocate label control structure");
        return NULL;
    }
    base_control->window = window;
    base_control->func_draw = X11Toolkit_DrawLabelControl;
    base_control->func_on_state_change = NULL;
    base_control->func_calc_size = X11Toolkit_CalculateLabelControl;
    base_control->func_free  = X11Toolkit_DestroyLabelControl;
    base_control->func_on_scale_change = NULL;
    base_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
    base_control->selected = false;
    base_control->dynamic = false;    
    base_control->rect.w = 0;
    base_control->rect.h = 0;
    base_control->is_default_enter = false;  
    base_control->is_default_esc = false;  
    control->sz = X11Toolkit_CountLinesOfText(utf8);
    control->lines = (char **)SDL_malloc(sizeof(char *) * control->sz);
    control->y = (int *)SDL_calloc(control->sz, sizeof(int));
    control->szs = (size_t *)SDL_calloc(control->sz, sizeof(size_t));
    for (i = 0; i < control->sz; i++) {
        const char *lf = SDL_strchr(utf8, '\n');
        const int length = lf ? (lf - utf8) : SDL_strlen(utf8);
        int w;
        int h;
        
        control->lines[i] = utf8;
        X11Toolkit_GetTextWidthHeight(window, utf8, length, &w, &h, &ascent, &descent);
        base_control->rect.w = SDL_max(base_control->rect.w, w);

        control->szs[i] = length;
        if (lf && (lf > utf8) && (lf[-1] == '\r')) {
            control->szs[i]--;
        }

        if (i > 0) {
            control->y[i] = ascent + descent + control->y[i-1];
            base_control->rect.h += ascent + descent + h;
        } else {
            control->y[i] = ascent;
            base_control->rect.h = h;
        }
        utf8 += length + 1;

        if (!lf) {
            break;
        }
    }
        
    X11Toolkit_AddControlToWindow(window, base_control);       
    return base_control;
}

int X11Toolkit_GetIconControlCharY(SDL_ToolkitControlX11 *control) {
    SDL_ToolkitIconControlX11 *icon_control;
    
    icon_control = (SDL_ToolkitIconControlX11 *)control;
    return icon_control->icon_char_y - icon_control->icon_char_a;
}

void X11Toolkit_SignalWindowClose(SDL_ToolkitWindowX11 *data) {
    data->close = true;
}

static void X11Toolkit_CalculateMenuBarControl(SDL_ToolkitControlX11 *base_control) {
    SDL_ToolkitMenuBarControlX11 *control;
	SDL_ListNode *cursor;
    int prev_x;
    int prev_w;
    int max_h;
	int i;
   
    i = prev_x = max_h = 0;
 	base_control->rect.x = base_control->rect.y = 0;
    control = (SDL_ToolkitMenuBarControlX11 *)base_control;
	cursor = control->menu_items;
    while (cursor) {
		SDL_ToolkitMenuItemX11 *item;
		int ascent;
		int descent;
		
		item = cursor->entry;
		item->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
		X11Toolkit_GetTextWidthHeight(base_control->window, item->utf8, SDL_strlen(item->utf8), &item->utf8_rect.w, &item->utf8_rect.h, &ascent, &descent);
		max_h = SDL_max(max_h, item->utf8_rect.h);
		item->utf8_rect.y = ascent + SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * base_control->window->iscale;
		if (base_control->window->utf8) {
			item->utf8_rect.y -= 2 * base_control->window->iscale;
		} else {
			item->utf8_rect.y -= 4 * base_control->window->iscale;
		}
        if (i > 0) {
            item->utf8_rect.x = prev_x + prev_w + SDL_TOOLKIT_X11_ELEMENT_PADDING_4 * base_control->window->iscale;
        } else {
            item->utf8_rect.x = SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * base_control->window->iscale;
        }		
        
		item->hover_rect.w = item->utf8_rect.w + SDL_TOOLKIT_X11_ELEMENT_PADDING_5 * 2 * base_control->window->iscale;
  		item->hover_rect.h = item->utf8_rect.h + SDL_TOOLKIT_X11_ELEMENT_PADDING_5 * 2 * base_control->window->iscale ;
		item->hover_rect.x = item->utf8_rect.x - SDL_TOOLKIT_X11_ELEMENT_PADDING_5 * base_control->window->iscale;
		item->hover_rect.y = item->utf8_rect.y - ascent - SDL_TOOLKIT_X11_ELEMENT_PADDING_5 * base_control->window->iscale;
		if (base_control->window->utf8) {
			item->hover_rect.y += 2 * base_control->window->iscale;
		} else {
			item->hover_rect.y += 4 * base_control->window->iscale;
		}
		
        i++;
		cursor = cursor->next;
		prev_x = item->utf8_rect.x;
		prev_w = item->utf8_rect.w;
	}
	base_control->rect.w = prev_x + prev_w + SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * base_control->window->iscale;
	base_control->rect.h = max_h + SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * 2 * base_control->window->iscale;
}

static void X11Toolkit_DrawMenuBarControl(SDL_ToolkitControlX11 *base_control) {
    SDL_ToolkitMenuBarControlX11 *control;
   	SDL_ListNode *cursor;
	
    control = (SDL_ToolkitMenuBarControlX11 *)base_control;
    
    X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_bevel_d.pixel);
    X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x, base_control->rect.y,
                                   base_control->rect.w, 1 *base_control->window->iscale);
                                   
    X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_bevel_l2.pixel);
    X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x, base_control->rect.y + 1 *base_control->window->iscale,
                                   base_control->rect.w, 1 *base_control->window->iscale);
                                   
    X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x, base_control->rect.y + base_control->rect.h - 1 *base_control->window->iscale,
                                   base_control->rect.w, 1 *base_control->window->iscale);
                                   
    X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_bevel_d.pixel);
    X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x, base_control->rect.y + base_control->rect.h - 2 *base_control->window->iscale,
                                   base_control->rect.w, 1 *base_control->window->iscale);
                                   
	cursor = control->menu_items;
    while (cursor) {
		SDL_ToolkitMenuItemX11 *item;
		
		item = cursor->entry;
		
		if (item->state == SDL_TOOLKIT_CONTROL_STATE_X11_HOVER) {
			X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED].pixel);
		} else if (item->state == SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED || item->state == SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED_HELD) {
			X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_pressed.pixel);
		}
		if (item->state != SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL) {
			X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx, item->hover_rect.x, item->hover_rect.y, item->hover_rect.w, item->hover_rect.h);
		}
                                   
		if (item->disabled) {
			X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_disabled_text.pixel);
		} else {
			X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);
		}
#ifdef X_HAVE_UTF8_STRING
        if (base_control->window->utf8) {
            X11_Xutf8DrawString(base_control->window->display, base_control->window->drawable, base_control->window->font_set, base_control->window->ctx,
                                base_control->rect.x + item->utf8_rect.x,
                                base_control->rect.y + item->utf8_rect.y,
                                item->utf8, SDL_strlen(item->utf8));
        } else
#endif
        {
			X11_XDrawString(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                            base_control->rect.x + item->utf8_rect.x, base_control->rect.y + item->utf8_rect.y,
                            item->utf8, SDL_strlen(item->utf8));
        }
		cursor = cursor->next;
	}
}

void X11Toolkit_OnMenuBarControlStateChange(SDL_ToolkitControlX11 *base_control) {
    SDL_ToolkitMenuBarControlX11 *control;
	SDL_ToolkitMenuItemX11 *item_to_open;
	SDL_ListNode *cursor;
	int x;
	int y;
	
	x = SDL_lroundf((base_control->window->e->xbutton.x/base_control->window->ev_scale)* base_control->window->ev_iscale);
	y = SDL_lroundf((base_control->window->e->xbutton.y/base_control->window->ev_scale)* base_control->window->ev_iscale);
	
	item_to_open = NULL;
    control = (SDL_ToolkitMenuBarControlX11 *)base_control;
	cursor = control->menu_items;
    while (cursor) {
		SDL_ToolkitMenuItemX11 *item;
        SDL_Rect *rect;

		item = cursor->entry;
		rect = &item->hover_rect;

		item->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
        if ((x >= rect->x) &&
            (x <= (rect->x + rect->w)) &&
            (y >= rect->y) &&
            (y <= (rect->y + rect->h))) {
			item_to_open = item;
			item_to_open->state = base_control->state;
            base_control->window->draw = true;
        }
        
		cursor = cursor->next;
	}

	if (item_to_open) {
		if (item_to_open->state == SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED) {
			SDL_ToolkitWindowX11 *window;
			SDL_ToolkitControlX11 *menu_control;
			Window dummy;
			
			X11_XTranslateCoordinates(base_control->window->display, base_control->window->window, RootWindow(base_control->window->display, base_control->window->screen), item_to_open->hover_rect.x, item_to_open->hover_rect.y + item_to_open->hover_rect.h, &x, &y, &dummy);
			window = X11Toolkit_CreateWindowStruct(NULL, base_control->window, SDL_TOOLKIT_WINDOW_MODE_X11_MENU, base_control->window->color_hints);
			SDL_ListAdd(&base_control->window->popup_windows, window);
			menu_control = X11Toolkit_CreateMenuControl(window, item_to_open->sub_menu);
			X11Toolkit_CreateWindowRes(window, menu_control->rect.w, menu_control->rect.h, x, y,  NULL);
			X11Toolkit_DoWindowEventLoop(window);
			X11Toolkit_DestroyWindow(window);
			item_to_open->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
		}
	}
}


SDL_ToolkitControlX11 *X11Toolkit_CreateMenuBarControl(SDL_ToolkitWindowX11 *window, SDL_ListNode *menu_items)
{
    SDL_ToolkitMenuBarControlX11 *control;
    SDL_ToolkitControlX11 *base_control;
    
    if (!menu_items) {
        return NULL;
    }    
    
    control = (SDL_ToolkitMenuBarControlX11 *)SDL_malloc(sizeof(SDL_ToolkitMenuBarControlX11));
    base_control = (SDL_ToolkitControlX11 *)control;
    if (!control) {
        SDL_SetError("Unable to allocate menu bar control structure");
        return NULL;
    }
    
	base_control->window = window;
    base_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
    base_control->func_calc_size = X11Toolkit_CalculateMenuBarControl;
    base_control->func_draw = X11Toolkit_DrawMenuBarControl;
    base_control->func_on_state_change = X11Toolkit_OnMenuBarControlStateChange;
    base_control->func_free = X11Toolkit_DestroyGenericControl;
    base_control->func_on_scale_change = NULL;
    base_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
    base_control->selected = false;
    base_control->dynamic = true;    
    base_control->is_default_enter = false;  
    base_control->is_default_esc = false;  
	control->menu_items = menu_items;
	X11Toolkit_CalculateMenuBarControl(base_control);
    X11Toolkit_AddControlToWindow(window, base_control);     
    return base_control;
}

static void X11Toolkit_CalculateMenuControl(SDL_ToolkitControlX11 *base_control) {
    SDL_ToolkitMenuControlX11 *control;
	SDL_ListNode *cursor;
    int max_w;
    int max_h;
	int prev_y;
	int prev_h;
	int ascent;
	int descent;
	
	prev_h = prev_y = max_w = max_h = 0;
 	base_control->rect.x = base_control->rect.y = 0;
    control = (SDL_ToolkitMenuControlX11 *)base_control;
    
	cursor = control->menu_items;
    while (cursor) {
		SDL_ToolkitMenuItemX11 *item;		
		item = cursor->entry;
		X11Toolkit_GetTextWidthHeight(base_control->window, item->utf8, SDL_strlen(item->utf8), &item->utf8_rect.w, &item->utf8_rect.h, &ascent, &descent);

		max_w = SDL_max(max_w, item->utf8_rect.w);
		max_h = SDL_max(max_h, item->utf8_rect.h);
		cursor = cursor->next;
	}
	
	cursor = control->menu_items;
    while (cursor) {
		SDL_ToolkitMenuItemX11 *item;
		item = cursor->entry;
		X11Toolkit_GetTextWidthHeight(base_control->window, item->utf8, SDL_strlen(item->utf8), &item->utf8_rect.w, &item->utf8_rect.h, &ascent, &descent);
		item->hover_rect.w = max_w + SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * 2 * base_control->window->iscale;
		item->hover_rect.h = max_h + SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * 2 * base_control->window->iscale;
		item->utf8_rect.x = SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * base_control->window->iscale;
		item->hover_rect.y = prev_y + prev_h;
		item->utf8_rect.y = item->hover_rect.y + ascent + (item->hover_rect.h - item->utf8_rect.h)/2;
		item->hover_rect.x = 0;
		if (base_control->window->utf8) {
			item->utf8_rect.y -= 2 * base_control->window->iscale;
		} else {
			item->utf8_rect.y -= 4 * base_control->window->iscale;
		}
		
		prev_h = item->hover_rect.h;
		prev_y = item->hover_rect.y;
		cursor = cursor->next;
	}
	base_control->rect.w = max_w + SDL_TOOLKIT_X11_ELEMENT_PADDING_3 * 2 * base_control->window->iscale;
	base_control->rect.h = prev_y + prev_h;
}

static void X11Toolkit_DrawMenuControl(SDL_ToolkitControlX11 *base_control) {
    SDL_ToolkitMenuControlX11 *control;
   	SDL_ListNode *cursor;
	
    control = (SDL_ToolkitMenuControlX11 *)base_control;
    
	X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_bevel_d.pixel);
	X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x, base_control->rect.y,
                                   base_control->rect.w, base_control->rect.h);

	X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_bevel_l2.pixel);
	X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x, base_control->rect.y,
                                   base_control->rect.w - 1 * base_control->window->iscale, base_control->rect.h - 1 * base_control->window->iscale);
          
	X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER].pixel);
	X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x + 1 * base_control->window->iscale, base_control->rect.y + 1 * base_control->window->iscale,
                                   base_control->rect.w - 2 * base_control->window->iscale, base_control->rect.h - 2 * base_control->window->iscale);
           
	X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_bevel_l1.pixel);
	X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x + 1 * base_control->window->iscale, base_control->rect.y + 1 * base_control->window->iscale,
                                   base_control->rect.w - 3 * base_control->window->iscale, base_control->rect.h - 3 * base_control->window->iscale);
           
	X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND].pixel);
	X11_XFillRectangle(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                                   base_control->rect.x + 2 * base_control->window->iscale, base_control->rect.y + 2 * base_control->window->iscale,
                                   base_control->rect.w - 4 * base_control->window->iscale, base_control->rect.h - 4 * base_control->window->iscale);
                                   
	cursor = control->menu_items;
    while (cursor) {
		SDL_ToolkitMenuItemX11 *item;
		
		item = cursor->entry;
                                   
		if (item->disabled) {
			X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor_disabled_text.pixel);
		} else {
			X11_XSetForeground(base_control->window->display, base_control->window->ctx, base_control->window->xcolor[SDL_MESSAGEBOX_COLOR_TEXT].pixel);
		}
#ifdef X_HAVE_UTF8_STRING
        if (base_control->window->utf8) {
            X11_Xutf8DrawString(base_control->window->display, base_control->window->drawable, base_control->window->font_set, base_control->window->ctx,
                                base_control->rect.x + item->utf8_rect.x,
                                base_control->rect.y + item->utf8_rect.y,
                                item->utf8, SDL_strlen(item->utf8));
        } else
#endif
        {
			X11_XDrawString(base_control->window->display, base_control->window->drawable, base_control->window->ctx,
                            base_control->rect.x + item->utf8_rect.x, base_control->rect.y + item->utf8_rect.y,
                            item->utf8, SDL_strlen(item->utf8));
        }
		cursor = cursor->next;
	}
}

SDL_ToolkitControlX11 *X11Toolkit_CreateMenuControl(SDL_ToolkitWindowX11 *window, SDL_ListNode *menu_items)
{
    SDL_ToolkitMenuControlX11 *control;
    SDL_ToolkitControlX11 *base_control;
    
    if (!menu_items) {
        return NULL;
    }    
    
    control = (SDL_ToolkitMenuControlX11 *)SDL_malloc(sizeof(SDL_ToolkitMenuControlX11));
    base_control = (SDL_ToolkitControlX11 *)control;
    if (!control) {
        SDL_SetError("Unable to allocate menu control structure");
        return NULL;
    }
    
	base_control->window = window;
    base_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
    base_control->func_calc_size = X11Toolkit_CalculateMenuControl;
    base_control->func_draw = X11Toolkit_DrawMenuControl;
    base_control->func_on_state_change = NULL;
    base_control->func_free = X11Toolkit_DestroyGenericControl;
    base_control->func_on_scale_change = NULL;
    base_control->state = SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL;
    base_control->selected = false;
    base_control->dynamic = true;    
    base_control->is_default_enter = false;  
    base_control->is_default_esc = false;  
	control->menu_items = menu_items;
	X11Toolkit_CalculateMenuControl(base_control);
    X11Toolkit_AddControlToWindow(window, base_control);     
    return base_control;
}



#endif // SDL_VIDEO_DRIVER_X11
