/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_PLATFORM_MACOS

#include <Cocoa/Cocoa.h>

#include "../SDL_tray_utils.h"
#include "../../video/SDL_surface_c.h"

/* Forward declaration */
struct SDL_Tray;

/* Objective-C helper class to handle status item button clicks */
@interface SDLTrayClickHandler : NSObject
@property (nonatomic, assign) struct SDL_Tray *tray;
@property (nonatomic, strong) id middleClickMonitor;
- (void)handleClick:(id)sender;
- (void)startMonitoringMiddleClicks;
- (void)stopMonitoringMiddleClicks;
@end

struct SDL_TrayMenu {
    NSMenu *nsmenu;

    int nEntries;
    SDL_TrayEntry **entries;

    SDL_Tray *parent_tray;
    SDL_TrayEntry *parent_entry;
};

struct SDL_TrayEntry {
    NSMenuItem *nsitem;

    SDL_TrayEntryFlags flags;
    SDL_TrayCallback callback;
    void *userdata;
    SDL_TrayMenu *submenu;

    SDL_TrayMenu *parent;
};

struct SDL_Tray {
    NSStatusBar *statusBar;
    NSStatusItem *statusItem;

    SDL_TrayMenu *menu;
    SDLTrayClickHandler *clickHandler;

    void *userdata;
    SDL_TrayClickCallback left_click_callback;
    SDL_TrayClickCallback right_click_callback;
    SDL_TrayClickCallback middle_click_callback;
};

@implementation SDLTrayClickHandler

- (void)handleClick:(id)sender
{
    if (!self.tray) {
        return;
    }

    NSEvent *event = [NSApp currentEvent];
    NSUInteger buttonNumber = [event buttonNumber];

    bool show_menu = false;

    if (buttonNumber == 0) {
        /* Left click */
        if (self.tray->left_click_callback) {
            show_menu = self.tray->left_click_callback(self.tray->userdata, self.tray);
        } else {
            show_menu = true;
        }
    } else if (buttonNumber == 1) {
        /* Right click */
        if (self.tray->right_click_callback) {
            show_menu = self.tray->right_click_callback(self.tray->userdata, self.tray);
        } else {
            show_menu = true;
        }
    } else if (buttonNumber == 2) {
        /* Middle click */
        if (self.tray->middle_click_callback) {
            self.tray->middle_click_callback(self.tray->userdata, self.tray);
        }
    }

    if (show_menu && self.tray->menu) {
        [self.tray->statusItem popUpStatusItemMenu:self.tray->menu->nsmenu];
    }
}

- (void)startMonitoringMiddleClicks
{
    if (self.middleClickMonitor) {
        return;
    }

    __weak SDLTrayClickHandler *weakSelf = self;
    self.middleClickMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskOtherMouseUp handler:^NSEvent *(NSEvent *event) {
        SDLTrayClickHandler *strongSelf = weakSelf;
        if (!strongSelf || !strongSelf.tray || [event buttonNumber] != 2) {
            return event;
        }

        /* Check if the click is within the status item's button bounds */
        NSPoint clickLocation = [event locationInWindow];
        NSWindow *statusItemWindow = strongSelf.tray->statusItem.button.window;

        if (statusItemWindow && event.window == statusItemWindow) {
            NSPoint localPoint = [strongSelf.tray->statusItem.button convertPoint:clickLocation fromView:nil];
            if (NSPointInRect(localPoint, strongSelf.tray->statusItem.button.bounds)) {
                if (strongSelf.tray->middle_click_callback) {
                    strongSelf.tray->middle_click_callback(strongSelf.tray->userdata, strongSelf.tray);
                }
            }
        }

        return event;
    }];
}

- (void)stopMonitoringMiddleClicks
{
    if (self.middleClickMonitor) {
        [NSEvent removeMonitor:self.middleClickMonitor];
        self.middleClickMonitor = nil;
    }
}

@end

static void DestroySDLMenu(SDL_TrayMenu *menu)
{
    for (int i = 0; i < menu->nEntries; i++) {
        if (menu->entries[i] && menu->entries[i]->submenu) {
            DestroySDLMenu(menu->entries[i]->submenu);
        }
        SDL_free(menu->entries[i]);
    }

    SDL_free(menu->entries);

    if (menu->parent_entry) {
        [menu->parent_entry->parent->nsmenu setSubmenu:nil forItem:menu->parent_entry->nsitem];
    } else if (menu->parent_tray) {
        [menu->parent_tray->statusItem setMenu:nil];
    }

    SDL_free(menu);
}

void SDL_UpdateTrays(void)
{
}

SDL_Tray *SDL_CreateTrayWithProperties(SDL_PropertiesID props)
{
    if (!SDL_IsMainThread()) {
        SDL_SetError("This function should be called on the main thread");
        return NULL;
    }

    SDL_Surface *icon = (SDL_Surface *)SDL_GetPointerProperty(props, SDL_PROP_TRAY_CREATE_ICON_POINTER, NULL);
    const char *tooltip = SDL_GetStringProperty(props, SDL_PROP_TRAY_CREATE_TOOLTIP_STRING, NULL);

    if (icon) {
        icon = SDL_ConvertSurface(icon, SDL_PIXELFORMAT_RGBA32);
        if (!icon) {
            return NULL;
        }
    }

    SDL_Tray *tray = (SDL_Tray *)SDL_calloc(1, sizeof(*tray));
    if (!tray) {
        SDL_DestroySurface(icon);
        return NULL;
    }

    tray->userdata = SDL_GetPointerProperty(props, SDL_PROP_TRAY_CREATE_USERDATA_POINTER, NULL);
    tray->left_click_callback = (SDL_TrayClickCallback)SDL_GetPointerProperty(props, SDL_PROP_TRAY_CREATE_LEFTCLICK_CALLBACK_POINTER, NULL);
    tray->right_click_callback = (SDL_TrayClickCallback)SDL_GetPointerProperty(props, SDL_PROP_TRAY_CREATE_RIGHTCLICK_CALLBACK_POINTER, NULL);
    tray->middle_click_callback = (SDL_TrayClickCallback)SDL_GetPointerProperty(props, SDL_PROP_TRAY_CREATE_MIDDLECLICK_CALLBACK_POINTER, NULL);

    tray->statusItem = nil;
    tray->statusBar = [NSStatusBar systemStatusBar];
    tray->statusItem = [tray->statusBar statusItemWithLength:NSVariableStatusItemLength];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:TRUE];

    if (tooltip) {
        tray->statusItem.button.toolTip = [NSString stringWithUTF8String:tooltip];
    } else {
        tray->statusItem.button.toolTip = nil;
    }

    if (icon) {
        NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **)&icon->pixels
                                                                           pixelsWide:icon->w
                                                                           pixelsHigh:icon->h
                                                                        bitsPerSample:8
                                                                      samplesPerPixel:4
                                                                             hasAlpha:YES
                                                                             isPlanar:NO
                                                                       colorSpaceName:NSCalibratedRGBColorSpace
                                                                          bytesPerRow:icon->pitch
                                                                         bitsPerPixel:32];
        NSImage *iconimg = [[NSImage alloc] initWithSize:NSMakeSize(icon->w, icon->h)];
        [iconimg addRepresentation:bitmap];

        /* A typical icon size is 22x22 on macOS. Failing to resize the icon
           may give oversized status bar buttons. */
        NSImage *iconimg22 = [[NSImage alloc] initWithSize:NSMakeSize(22, 22)];
        [iconimg22 lockFocus];
        [iconimg setSize:NSMakeSize(22, 22)];
        [iconimg drawInRect:NSMakeRect(0, 0, 22, 22)];
        [iconimg22 unlockFocus];

        tray->statusItem.button.image = iconimg22;

        SDL_DestroySurface(icon);
    }

    /* Create click handler and set up button to receive clicks */
    tray->clickHandler = [[SDLTrayClickHandler alloc] init];
    tray->clickHandler.tray = tray;

    [tray->statusItem.button setTarget:tray->clickHandler];
    [tray->statusItem.button setAction:@selector(handleClick:)];
    [tray->statusItem.button sendActionOn:(NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp)];

    /* Start monitoring for middle clicks since status items don't receive them via the normal action mechanism */
    [tray->clickHandler startMonitoringMiddleClicks];

    SDL_RegisterTray(tray);

    return tray;
}

SDL_Tray *SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    SDL_Tray *tray;
    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return NULL;
    }
    if (icon) {
        SDL_SetPointerProperty(props, SDL_PROP_TRAY_CREATE_ICON_POINTER, icon);
    }
    if (tooltip) {
        SDL_SetStringProperty(props, SDL_PROP_TRAY_CREATE_TOOLTIP_STRING, tooltip);
    }
    tray = SDL_CreateTrayWithProperties(props);
    SDL_DestroyProperties(props);
    return tray;
}

void SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon)
{
    if (!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        return;
    }

    if (!icon) {
        tray->statusItem.button.image = nil;
        return;
    }

    icon = SDL_ConvertSurface(icon, SDL_PIXELFORMAT_RGBA32);
    if (!icon) {
        tray->statusItem.button.image = nil;
        return;
    }

    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **)&icon->pixels
                                                                       pixelsWide:icon->w
                                                                       pixelsHigh:icon->h
                                                                    bitsPerSample:8
                                                                  samplesPerPixel:4
                                                                         hasAlpha:YES
                                                                         isPlanar:NO
                                                                   colorSpaceName:NSCalibratedRGBColorSpace
                                                                      bytesPerRow:icon->pitch
                                                                     bitsPerPixel:32];
    NSImage *iconimg = [[NSImage alloc] initWithSize:NSMakeSize(icon->w, icon->h)];
    [iconimg addRepresentation:bitmap];

    /* A typical icon size is 22x22 on macOS. Failing to resize the icon
       may give oversized status bar buttons. */
    NSImage *iconimg22 = [[NSImage alloc] initWithSize:NSMakeSize(22, 22)];
    [iconimg22 lockFocus];
    [iconimg setSize:NSMakeSize(22, 22)];
    [iconimg drawInRect:NSMakeRect(0, 0, 22, 22)];
    [iconimg22 unlockFocus];

    tray->statusItem.button.image = iconimg22;

    SDL_DestroySurface(icon);
}

void SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip)
{
    if (!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        return;
    }

    if (tooltip) {
        tray->statusItem.button.toolTip = [NSString stringWithUTF8String:tooltip];
    } else {
        tray->statusItem.button.toolTip = nil;
    }
}

SDL_TrayMenu *SDL_CreateTrayMenu(SDL_Tray *tray)
{
    CHECK_PARAM(!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        SDL_InvalidParamError("tray");
        return NULL;
    }

    SDL_TrayMenu *menu = (SDL_TrayMenu *)SDL_calloc(1, sizeof(*menu));
    if (!menu) {
        return NULL;
    }

    NSMenu *nsmenu = [[NSMenu alloc] init];
    [nsmenu setAutoenablesItems:FALSE];

    /* Don't set menu on statusItem - we handle menu display manually in the click handler */

    tray->menu = menu;
    menu->nsmenu = nsmenu;
    menu->nEntries = 0;
    menu->entries = NULL;
    menu->parent_tray = tray;
    menu->parent_entry = NULL;

    return menu;
}

SDL_TrayMenu *SDL_GetTrayMenu(SDL_Tray *tray)
{
    CHECK_PARAM(!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        SDL_InvalidParamError("tray");
        return NULL;
    }

    return tray->menu;
}

SDL_TrayMenu *SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    CHECK_PARAM(!entry) {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    if (entry->submenu) {
        SDL_SetError("Tray entry submenu already exists");
        return NULL;
    }

    if (!(entry->flags & SDL_TRAYENTRY_SUBMENU)) {
        SDL_SetError("Cannot create submenu for entry not created with SDL_TRAYENTRY_SUBMENU");
        return NULL;
    }

    SDL_TrayMenu *menu = (SDL_TrayMenu *)SDL_calloc(1, sizeof(*menu));
    if (!menu) {
        return NULL;
    }

    NSMenu *nsmenu = [[NSMenu alloc] init];
    [nsmenu setAutoenablesItems:FALSE];

    entry->submenu = menu;
    menu->nsmenu = nsmenu;
    menu->nEntries = 0;
    menu->entries = NULL;
    menu->parent_tray = NULL;
    menu->parent_entry = entry;

    [entry->parent->nsmenu setSubmenu:nsmenu forItem:entry->nsitem];

    return menu;
}

SDL_TrayMenu *SDL_GetTraySubmenu(SDL_TrayEntry *entry)
{
    CHECK_PARAM(!entry) {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    return entry->submenu;
}

const SDL_TrayEntry **SDL_GetTrayEntries(SDL_TrayMenu *menu, int *count)
{
    CHECK_PARAM(!menu) {
        SDL_InvalidParamError("menu");
        return NULL;
    }

    if (count) {
        *count = menu->nEntries;
    }
    return (const SDL_TrayEntry **)menu->entries;
}

void SDL_RemoveTrayEntry(SDL_TrayEntry *entry)
{
    if (!entry) {
        return;
    }

    SDL_TrayMenu *menu = entry->parent;

    bool found = false;
    for (int i = 0; i < menu->nEntries - 1; i++) {
        if (menu->entries[i] == entry) {
            found = true;
        }

        if (found) {
            menu->entries[i] = menu->entries[i + 1];
        }
    }

    if (entry->submenu) {
        DestroySDLMenu(entry->submenu);
    }

    menu->nEntries--;
    SDL_TrayEntry **new_entries = (SDL_TrayEntry **)SDL_realloc(menu->entries, (menu->nEntries + 1) * sizeof(*new_entries));

    /* Not sure why shrinking would fail, but even if it does, we can live with a "too big" array */
    if (new_entries) {
        menu->entries = new_entries;
        menu->entries[menu->nEntries] = NULL;
    }

    [menu->nsmenu removeItem:entry->nsitem];

    SDL_free(entry);
}

SDL_TrayEntry *SDL_InsertTrayEntryAt(SDL_TrayMenu *menu, int pos, const char *label, SDL_TrayEntryFlags flags)
{
    CHECK_PARAM(!menu) {
        SDL_InvalidParamError("menu");
        return NULL;
    }

    CHECK_PARAM(pos < -1 || pos > menu->nEntries) {
        SDL_InvalidParamError("pos");
        return NULL;
    }

    if (pos == -1) {
        pos = menu->nEntries;
    }

    SDL_TrayEntry *entry = (SDL_TrayEntry *)SDL_calloc(1, sizeof(*entry));
    if (!entry) {
        return NULL;
    }

    SDL_TrayEntry **new_entries = (SDL_TrayEntry **)SDL_realloc(menu->entries, (menu->nEntries + 2) * sizeof(*new_entries));
    if (!new_entries) {
        SDL_free(entry);
        return NULL;
    }

    menu->entries = new_entries;
    menu->nEntries++;

    for (int i = menu->nEntries - 1; i > pos; i--) {
        menu->entries[i] = menu->entries[i - 1];
    }

    new_entries[pos] = entry;
    new_entries[menu->nEntries] = NULL;

    NSMenuItem *nsitem;
    if (label == NULL) {
        nsitem = [NSMenuItem separatorItem];
    } else {
        nsitem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:label] action:@selector(menu:) keyEquivalent:@""];
        [nsitem setEnabled:((flags & SDL_TRAYENTRY_DISABLED) ? FALSE : TRUE)];
        [nsitem setState:((flags & SDL_TRAYENTRY_CHECKED) ? NSControlStateValueOn : NSControlStateValueOff)];
        [nsitem setRepresentedObject:[NSValue valueWithPointer:entry]];
    }

    [menu->nsmenu insertItem:nsitem atIndex:pos];

    entry->nsitem = nsitem;
    entry->flags = flags;
    entry->callback = NULL;
    entry->userdata = NULL;
    entry->submenu = NULL;
    entry->parent = menu;

    return entry;
}

void SDL_SetTrayEntryLabel(SDL_TrayEntry *entry, const char *label)
{
    if (!entry) {
        return;
    }

    [entry->nsitem setTitle:[NSString stringWithUTF8String:label]];
}

const char *SDL_GetTrayEntryLabel(SDL_TrayEntry *entry)
{
    CHECK_PARAM(!entry) {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    return [[entry->nsitem title] UTF8String];
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, bool checked)
{
    if (!entry) {
        return;
    }

    [entry->nsitem setState:(checked ? NSControlStateValueOn : NSControlStateValueOff)];
}

bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    if (!entry) {
        return false;
    }

    return entry->nsitem.state == NSControlStateValueOn;
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, bool enabled)
{
    if (!entry || !(entry->flags & SDL_TRAYENTRY_CHECKBOX)) {
        return;
    }

    [entry->nsitem setEnabled:(enabled ? YES : NO)];
}

bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    if (!entry || !(entry->flags & SDL_TRAYENTRY_CHECKBOX)) {
        return false;
    }

    return entry->nsitem.enabled;
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    if (!entry) {
        return;
    }

    entry->callback = callback;
    entry->userdata = userdata;
}

void SDL_ClickTrayEntry(SDL_TrayEntry *entry)
{
	if (!entry) {
		return;
	}

	if (entry->flags & SDL_TRAYENTRY_CHECKBOX) {
		SDL_SetTrayEntryChecked(entry, !SDL_GetTrayEntryChecked(entry));
	}

	if (entry->callback) {
		entry->callback(entry->userdata, entry);
	}
}

SDL_TrayMenu *SDL_GetTrayEntryParent(SDL_TrayEntry *entry)
{
    CHECK_PARAM(!entry) {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    return entry->parent;
}

SDL_TrayEntry *SDL_GetTrayMenuParentEntry(SDL_TrayMenu *menu)
{
    CHECK_PARAM(!menu) {
        SDL_InvalidParamError("menu");
        return NULL;
    }

    return menu->parent_entry;
}

SDL_Tray *SDL_GetTrayMenuParentTray(SDL_TrayMenu *menu)
{
    CHECK_PARAM(!menu) {
        SDL_InvalidParamError("menu");
        return NULL;
    }

    return menu->parent_tray;
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    if (!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        return;
    }

    SDL_UnregisterTray(tray);

    [[NSStatusBar systemStatusBar] removeStatusItem:tray->statusItem];

    if (tray->menu) {
        DestroySDLMenu(tray->menu);
    }

    if (tray->clickHandler) {
        [tray->clickHandler stopMonitoringMiddleClicks];
        tray->clickHandler.tray = NULL;
        tray->clickHandler = nil;
    }

    SDL_free(tray);
}

#endif // SDL_PLATFORM_MACOS
