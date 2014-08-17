/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_COCOA
#include "SDL_timer.h"

#include "SDL_cocoavideo.h"
#include "../../events/SDL_events_c.h"
#include "SDL_assert.h"

@interface SDLApplication : NSApplication

- (void)terminate:(id)sender;

@end

@implementation SDLApplication

// Override terminate to handle Quit and System Shutdown smoothly.
- (void)terminate:(id)sender
{
    SDL_SendQuit();
}

@end // SDLApplication

/* setAppleMenu disappeared from the headers in 10.4 */
@interface NSApplication(NSAppleMenu)
- (void)setAppleMenu:(NSMenu *)menu;
@end

@interface SDLAppDelegate : NSObject <NSApplicationDelegate> {
@public
    BOOL seenFirstActivate;
}

- (id)init;
@end

@implementation SDLAppDelegate : NSObject
- (id)init
{
    self = [super init];
    if (self) {
        seenFirstActivate = NO;
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(focusSomeWindow:)
                                                     name:NSApplicationDidBecomeActiveNotification
                                                   object:nil];
    }

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)focusSomeWindow:(NSNotification *)aNotification
{
    /* HACK: Ignore the first call. The application gets a
     * applicationDidBecomeActive: a little bit after the first window is
     * created, and if we don't ignore it, a window that has been created with
     * SDL_WINDOW_MINIZED will ~immediately be restored.
     */
    if (!seenFirstActivate) {
        seenFirstActivate = YES;
        return;
    }

    SDL_VideoDevice *device = SDL_GetVideoDevice();
    if (device && device->windows) {
        SDL_Window *window = device->windows;
        int i;
        for (i = 0; i < device->num_displays; ++i) {
            SDL_Window *fullscreen_window = device->displays[i].fullscreen_window;
            if (fullscreen_window) {
                if (fullscreen_window->flags & SDL_WINDOW_MINIMIZED) {
                    SDL_RestoreWindow(fullscreen_window);
                }
                return;
            }
        }

        if (window->flags & SDL_WINDOW_MINIMIZED) {
            SDL_RestoreWindow(window);
        } else {
            SDL_RaiseWindow(window);
        }
    }
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
    return (BOOL)SDL_SendDropFile([filename UTF8String]);
}
@end

static SDLAppDelegate *appDelegate = nil;

static NSString *
GetApplicationName(void)
{
    NSString *appName;

    /* Determine the application name */
    appName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDisplayName"];
    if (!appName) {
        appName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
    }

    if (![appName length]) {
        appName = [[NSProcessInfo processInfo] processName];
    }

    return appName;
}

static void
CreateApplicationMenus(void)
{
    NSString *appName;
    NSString *title;
    NSMenu *appleMenu;
    NSMenu *serviceMenu;
    NSMenu *windowMenu;
    NSMenu *viewMenu;
    NSMenuItem *menuItem;
    NSMenu *mainMenu;

    if (NSApp == nil) {
        return;
    }

    mainMenu = [[NSMenu alloc] init];

    /* Create the main menu bar */
    [NSApp setMainMenu:mainMenu];

    [mainMenu release];  /* we're done with it, let NSApp own it. */
    mainMenu = nil;

    /* Create the application menu */
    appName = GetApplicationName();
    appleMenu = [[NSMenu alloc] initWithTitle:@""];

    /* Add menu items */
    title = [@"About " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    [appleMenu addItemWithTitle:@"Preferences…" action:nil keyEquivalent:@","];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    serviceMenu = [[NSMenu alloc] initWithTitle:@""];
    menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Services" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:serviceMenu];

    [NSApp setServicesMenu:serviceMenu];
    [serviceMenu release];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    title = [@"Hide " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

    menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    [menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

    [appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    title = [@"Quit " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

    /* Put menu into the menubar */
    menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:appleMenu];
    [[NSApp mainMenu] addItem:menuItem];
    [menuItem release];

    /* Tell the application object that this is now the application menu */
    [NSApp setAppleMenu:appleMenu];
    [appleMenu release];


    /* Create the window menu */
    windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

    /* Add menu items */
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];

    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

    /* Put menu into the menubar */
    menuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:windowMenu];
    [[NSApp mainMenu] addItem:menuItem];
    [menuItem release];

    /* Tell the application object that this is now the window menu */
    [NSApp setWindowsMenu:windowMenu];
    [windowMenu release];


    /* Add the fullscreen view toggle menu option, if supported */
    if ([NSApp respondsToSelector:@selector(setPresentationOptions:)]) {
        /* Create the view menu */
        viewMenu = [[NSMenu alloc] initWithTitle:@"View"];

        /* Add menu items */
        menuItem = [viewMenu addItemWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
        [menuItem setKeyEquivalentModifierMask:NSControlKeyMask | NSCommandKeyMask];

        /* Put menu into the menubar */
        menuItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
        [menuItem setSubmenu:viewMenu];
        [[NSApp mainMenu] addItem:menuItem];
        [menuItem release];

        [viewMenu release];
    }
}

void
Cocoa_RegisterApp(void)
{ @autoreleasepool
{
    /* This can get called more than once! Be careful what you initialize! */
    ProcessSerialNumber psn;

    if (!GetCurrentProcess(&psn)) {
        TransformProcessType(&psn, kProcessTransformToForegroundApplication);
        SetFrontProcess(&psn);
    }

    if (NSApp == nil) {
        [SDLApplication sharedApplication];
        SDL_assert(NSApp != nil);

        if ([NSApp mainMenu] == nil) {
            CreateApplicationMenus();
        }
        [NSApp finishLaunching];
        NSDictionary *appDefaults = [[NSDictionary alloc] initWithObjectsAndKeys:
            [NSNumber numberWithBool:NO], @"AppleMomentumScrollSupported",
            [NSNumber numberWithBool:NO], @"ApplePressAndHoldEnabled",
            [NSNumber numberWithBool:YES], @"ApplePersistenceIgnoreState",
            nil];
        [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
        [appDefaults release];
    }
    if (NSApp && !appDelegate) {
        appDelegate = [[SDLAppDelegate alloc] init];

        /* If someone else has an app delegate, it means we can't turn a
         * termination into SDL_Quit, and we can't handle application:openFile:
         */
        if (![NSApp delegate]) {
            [(NSApplication *)NSApp setDelegate:appDelegate];
        } else {
            appDelegate->seenFirstActivate = YES;
        }
    }
}}

void
Cocoa_PumpEvents(_THIS)
{ @autoreleasepool
{
    /* Update activity every 30 seconds to prevent screensaver */
    if (_this->suspend_screensaver) {
        SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;
        Uint32 now = SDL_GetTicks();
        if (!data->screensaver_activity ||
            SDL_TICKS_PASSED(now, data->screensaver_activity + 30000)) {
            UpdateSystemActivity(UsrActivity);
            data->screensaver_activity = now;
        }
    }

    for ( ; ; ) {
        NSEvent *event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES ];
        if ( event == nil ) {
            break;
        }

        switch ([event type]) {
        case NSLeftMouseDown:
        case NSOtherMouseDown:
        case NSRightMouseDown:
        case NSLeftMouseUp:
        case NSOtherMouseUp:
        case NSRightMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case NSOtherMouseDragged: /* usually middle mouse dragged */
        case NSMouseMoved:
        case NSScrollWheel:
            Cocoa_HandleMouseEvent(_this, event);
            break;
        case NSKeyDown:
        case NSKeyUp:
        case NSFlagsChanged:
            Cocoa_HandleKeyEvent(_this, event);
            break;
        default:
            break;
        }
        /* Pass through to NSApp to make sure everything stays in sync */
        [NSApp sendEvent:event];
    }
}}

#endif /* SDL_VIDEO_DRIVER_COCOA */

/* vi: set ts=4 sw=4 expandtab: */
