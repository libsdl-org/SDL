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

#include "SDL_internal.h"

#include <Cocoa/Cocoa.h>

struct SDL_TrayMenu {
    SDL_Tray *tray;
    NSMenu *menu;
};

struct SDL_TrayEntry {
    SDL_Tray *tray;
    SDL_TrayMenu *menu;
    NSMenuItem *item;

    SDL_TrayCallback callback;
    void *userdata;
    SDL_TrayMenu submenu;
};

struct SDL_Tray {
    NSStatusBar *statusBar;
    NSStatusItem *statusItem;
    SDL_TrayMenu menu;

    size_t nEntries;
    SDL_TrayEntry **entries;
};

static NSApplication *app = NULL;

@interface AppDelegate: NSObject <NSApplicationDelegate>
    - (IBAction)menu:(id)sender;
@end

@implementation AppDelegate{}
    - (IBAction)menu:(id)sender
    {
        SDL_TrayEntry *entry = [[sender representedObject] pointerValue];
        if (entry && entry->callback) {
            entry->callback(entry->userdata, entry);
        }
    }
@end

SDL_Tray *SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    SDL_Tray *tray = (SDL_Tray *) SDL_malloc(sizeof(SDL_Tray));

    AppDelegate *delegate = [[AppDelegate alloc] init];
    app = [NSApplication sharedApplication];
    [app setDelegate:delegate];

    if (!tray) {
        return NULL;
    }

    SDL_memset((void *) tray, 0, sizeof(*tray));

    tray->statusItem = nil;
    tray->statusBar = [NSStatusBar systemStatusBar];
    tray->statusItem = [tray->statusBar statusItemWithLength:NSVariableStatusItemLength];
    [app activateIgnoringOtherApps:TRUE];

    if (tooltip) {
        tray->statusItem.button.toolTip = [NSString stringWithUTF8String:tooltip];
    } else {
        tray->statusItem.button.toolTip = nil;
    }

    if (icon) {
        SDL_Surface *iconfmt = SDL_ConvertSurface(icon, SDL_PIXELFORMAT_RGBA32);
        if (!iconfmt) {
            goto skip_putting_an_icon;
        }

        NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **)&iconfmt->pixels
                                                                           pixelsWide:iconfmt->w
                                                                           pixelsHigh:iconfmt->h
                                                                        bitsPerSample:8
                                                                      samplesPerPixel:4
                                                                             hasAlpha:YES
                                                                             isPlanar:NO
                                                                       colorSpaceName:NSCalibratedRGBColorSpace
                                                                          bytesPerRow:iconfmt->pitch
                                                                         bitsPerPixel:32];
        NSImage *iconimg = [[NSImage alloc] initWithSize:NSMakeSize(iconfmt->w, iconfmt->h)];
        [iconimg addRepresentation:bitmap];

        /* A typical icon size is 22x22 on macOS. Failing to resize the icon
           may give oversized status bar buttons. */
        NSImage *iconimg22 = [[NSImage alloc] initWithSize:NSMakeSize(22, 22)];
        [iconimg22 lockFocus];
        [iconimg setSize:NSMakeSize(22, 22)];
        [iconimg drawInRect:NSMakeRect(0, 0, 22, 22)];
        [iconimg22 unlockFocus];

        tray->statusItem.button.image = iconimg22;

        SDL_DestroySurface(iconfmt);
    }

skip_putting_an_icon:
    return tray;
}

void SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon)
{
    if (!icon) {
        tray->statusItem.button.image = nil;
        return;
    }

    SDL_Surface *iconfmt = SDL_ConvertSurface(icon, SDL_PIXELFORMAT_RGBA32);
    if (!iconfmt) {
        /* TODO: Ignore errors silently, as in SDL_CreateTray? */
        tray->statusItem.button.image = nil;
        return;
    }

    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **)&iconfmt->pixels
                                                                       pixelsWide:iconfmt->w
                                                                       pixelsHigh:iconfmt->h
                                                                    bitsPerSample:8
                                                                  samplesPerPixel:4
                                                                         hasAlpha:YES
                                                                         isPlanar:NO
                                                                   colorSpaceName:NSCalibratedRGBColorSpace
                                                                      bytesPerRow:iconfmt->pitch
                                                                     bitsPerPixel:32];
    NSImage *iconimg = [[NSImage alloc] initWithSize:NSMakeSize(iconfmt->w, iconfmt->h)];
    [iconimg addRepresentation:bitmap];

    /* A typical icon size is 22x22 on macOS. Failing to resize the icon
       may give oversized status bar buttons. */
    NSImage *iconimg22 = [[NSImage alloc] initWithSize:NSMakeSize(22, 22)];
    [iconimg22 lockFocus];
    [iconimg setSize:NSMakeSize(22, 22)];
    [iconimg drawInRect:NSMakeRect(0, 0, 22, 22)];
    [iconimg22 unlockFocus];

    tray->statusItem.button.image = iconimg22;

    SDL_DestroySurface(iconfmt);
}

void SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip)
{
    if (tooltip) {
        tray->statusItem.button.toolTip = [NSString stringWithUTF8String:tooltip];
    } else {
        tray->statusItem.button.toolTip = nil;
    }
}

SDL_TrayMenu *SDL_CreateTrayMenu(SDL_Tray *tray)
{
    NSMenu *menu = [[NSMenu alloc] init];
    [menu setAutoenablesItems:FALSE];

    [tray->statusItem setMenu:menu];
    tray->menu.menu = menu;
    tray->menu.tray = tray;
    return &tray->menu;
}

SDL_TrayMenu *SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    NSMenu *menu = [[NSMenu alloc] init];
    [menu setAutoenablesItems:FALSE];

    entry->submenu.menu = menu;
    entry->submenu.tray = entry->tray;
    [entry->menu->menu setSubmenu:menu forItem: entry->item];

    return &entry->submenu;
}

SDL_TrayEntry *SDL_AppendTrayEntry(SDL_TrayMenu *menu, const char *label, SDL_TrayEntryFlags flags)
{
    SDL_TrayEntry *entry = SDL_malloc(sizeof(SDL_TrayEntry));

    if (!entry) {
        return NULL;
    }

    SDL_memset((void *) entry, 0, sizeof(*entry));
    entry->tray = menu->tray;
    entry->menu = menu;

    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:label] action:@selector(menu:) keyEquivalent:@""];

    entry->item = item;

    SDL_TrayEntry **new_entries = (SDL_TrayEntry **) SDL_realloc(entry->tray->entries, (entry->tray->nEntries + 1) * sizeof(SDL_TrayEntry **));

    if (!new_entries) {
        SDL_free(entry);
        return NULL;
    }

    new_entries[entry->tray->nEntries] = entry;
    entry->tray->entries = new_entries;
    entry->tray->nEntries++;

    [item setEnabled:((flags & SDL_TRAYENTRY_DISABLED) ? FALSE : TRUE)];
    [item setState:((flags & SDL_TRAYENTRY_CHECKED) ? NSControlStateValueOn : NSControlStateValueOff)];
    [item setRepresentedObject:[NSValue valueWithPointer:entry]];

    [menu->menu addItem:item];

    return entry;
}

void SDL_AppendTraySeparator(SDL_TrayMenu *menu)
{
    [menu->menu addItem:[NSMenuItem separatorItem]];
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, SDL_bool checked)
{
    [entry->item setState:(checked ? NSControlStateValueOn : NSControlStateValueOff)];
}

SDL_bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    return entry->item.state == NSControlStateValueOn;
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, SDL_bool enabled)
{
    [entry->item setState:(enabled ? YES : NO)];
}

SDL_bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    return entry->item.enabled;
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    entry->callback = callback;
    entry->userdata = userdata;
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    [[NSStatusBar systemStatusBar] removeStatusItem:tray->statusItem];

    for (int i = 0; i < tray->nEntries; i++) {
        SDL_free(tray->entries[i]);
    }

    SDL_free(tray->entries);

    SDL_free(tray);
}
