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

#ifdef SDL_VIDEO_DRIVER_COCOA

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1090
#error SDL for macOS must be built with a 10.9 SDK or above.
#endif /* MAC_OS_X_VERSION_MAX_ALLOWED < 1090 */

#include <float.h> /* For FLT_MAX */

#include "../../events/SDL_dropevents_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../SDL_sysvideo.h"

#include "SDL_cocoamouse.h"
#include "SDL_cocoaopengl.h"
#include "SDL_cocoaopengles.h"
#include "SDL_cocoavideo.h"

/* #define DEBUG_COCOAWINDOW */

#ifdef DEBUG_COCOAWINDOW
#define DLog(fmt, ...) printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define DLog(...) \
    do {          \
    } while (0)
#endif

#ifndef MAC_OS_X_VERSION_10_12
#define NSEventModifierFlagCapsLock NSAlphaShiftKeyMask
#endif
#ifndef NSAppKitVersionNumber10_13_2
#define NSAppKitVersionNumber10_13_2 1561.2
#endif
#ifndef NSAppKitVersionNumber10_14
#define NSAppKitVersionNumber10_14 1671
#endif

@implementation SDL_CocoaWindowData

@end

@interface NSWindow (SDL)
#if MAC_OS_X_VERSION_MAX_ALLOWED < 101000 /* Added in the 10.10 SDK */
@property(readonly) NSRect contentLayoutRect;
#endif

/* This is available as of 10.13.2, but isn't in public headers */
@property(nonatomic) NSRect mouseConfinementRect;
@end

@interface SDLWindow : NSWindow <NSDraggingDestination>
/* These are needed for borderless/fullscreen windows */
- (BOOL)canBecomeKeyWindow;
- (BOOL)canBecomeMainWindow;
- (void)sendEvent:(NSEvent *)event;
- (void)doCommandBySelector:(SEL)aSelector;

/* Handle drag-and-drop of files onto the SDL window. */
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender;
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender;
- (BOOL)wantsPeriodicDraggingUpdates;
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem;

- (SDL_Window *)findSDLWindow;
@end

@implementation SDLWindow

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    /* Only allow using the macOS native fullscreen toggle menubar item if the
     * window is resizable and not in a SDL fullscreen mode.
     */
    if ([menuItem action] == @selector(toggleFullScreen:)) {
        SDL_Window *window = [self findSDLWindow];
        if (window == NULL) {
            return NO;
        } else if (window->flags & SDL_WINDOW_FULLSCREEN) {
            return NO;
        } else if ((window->flags & SDL_WINDOW_RESIZABLE) == 0) {
            return NO;
        }
    }
    return [super validateMenuItem:menuItem];
}

- (BOOL)canBecomeKeyWindow
{
    SDL_Window *window = [self findSDLWindow];
    if (window && !(window->flags & (SDL_WINDOW_TOOLTIP | SDL_WINDOW_NOT_FOCUSABLE))) {
        return YES;
    } else {
        return NO;
    }
}

- (BOOL)canBecomeMainWindow
{
    SDL_Window *window = [self findSDLWindow];
    if (window && !SDL_WINDOW_IS_POPUP(window)) {
        return YES;
    } else {
        return NO;
    }
}

- (void)sendEvent:(NSEvent *)event
{
    id delegate;
    [super sendEvent:event];

    if ([event type] != NSEventTypeLeftMouseUp) {
        return;
    }

    delegate = [self delegate];
    if (![delegate isKindOfClass:[Cocoa_WindowListener class]]) {
        return;
    }

    if ([delegate isMoving]) {
        [delegate windowDidFinishMoving];
    }
}

/* We'll respond to selectors by doing nothing so we don't beep.
 * The escape key gets converted to a "cancel" selector, etc.
 */
- (void)doCommandBySelector:(SEL)aSelector
{
    /*NSLog(@"doCommandBySelector: %@\n", NSStringFromSelector(aSelector));*/
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    if (([sender draggingSourceOperationMask] & NSDragOperationGeneric) == NSDragOperationGeneric) {
        return NSDragOperationGeneric;
    }

    return NSDragOperationNone; /* no idea what to do with this, reject it. */
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
    if (([sender draggingSourceOperationMask] & NSDragOperationGeneric) == NSDragOperationGeneric) {
        SDL_Window *sdlwindow = [self findSDLWindow];
        NSPoint point = [sender draggingLocation];
        float x, y;
        x = point.x;
        y = (sdlwindow->h - point.y);
        SDL_SendDropPosition(sdlwindow, x, y);
        return NSDragOperationGeneric;
    }

    return NSDragOperationNone; /* no idea what to do with this, reject it. */
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    @autoreleasepool {
        NSPasteboard *pasteboard = [sender draggingPasteboard];
        NSArray *types = [NSArray arrayWithObject:NSFilenamesPboardType];
        NSString *desiredType = [pasteboard availableTypeFromArray:types];
        SDL_Window *sdlwindow = [self findSDLWindow];
        NSData *data;
        NSArray *array;
        NSPoint point;
        SDL_Mouse *mouse;
        float x, y;

        if (desiredType == nil) {
            return NO; /* can't accept anything that's being dropped here. */
        }

        data = [pasteboard dataForType:desiredType];
        if (data == nil) {
            return NO;
        }

        SDL_assert([desiredType isEqualToString:NSFilenamesPboardType]);
        array = [pasteboard propertyListForType:@"NSFilenamesPboardType"];

        /* Code addon to update the mouse location */
        point = [sender draggingLocation];
        mouse = SDL_GetMouse();
        x = point.x;
        y = (sdlwindow->h - point.y);
        if (x >= 0.0f && x < (float)sdlwindow->w && y >= 0.0f && y < (float)sdlwindow->h) {
            SDL_SendMouseMotion(0, sdlwindow, mouse->mouseID, 0, x, y);
        }
        /* Code addon to update the mouse location */

        for (NSString *path in array) {
            NSURL *fileURL = [NSURL fileURLWithPath:path];
            NSNumber *isAlias = nil;

            [fileURL getResourceValue:&isAlias forKey:NSURLIsAliasFileKey error:nil];

            /* If the URL is an alias, resolve it. */
            if ([isAlias boolValue]) {
                NSURLBookmarkResolutionOptions opts = NSURLBookmarkResolutionWithoutMounting | NSURLBookmarkResolutionWithoutUI;
                NSData *bookmark = [NSURL bookmarkDataWithContentsOfURL:fileURL error:nil];
                if (bookmark != nil) {
                    NSURL *resolvedURL = [NSURL URLByResolvingBookmarkData:bookmark
                                                                   options:opts
                                                             relativeToURL:nil
                                                       bookmarkDataIsStale:nil
                                                                     error:nil];

                    if (resolvedURL != nil) {
                        fileURL = resolvedURL;
                    }
                }
            }

            if (!SDL_SendDropFile(sdlwindow, NULL, [[fileURL path] UTF8String])) {
                return NO;
            }
        }

        SDL_SendDropComplete(sdlwindow);
        return YES;
    }
}

- (BOOL)wantsPeriodicDraggingUpdates
{
    return NO;
}

- (SDL_Window *)findSDLWindow
{
    SDL_Window *sdlwindow = NULL;
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    /* !!! FIXME: is there a better way to do this? */
    if (_this) {
        for (sdlwindow = _this->windows; sdlwindow; sdlwindow = sdlwindow->next) {
            NSWindow *nswindow = ((__bridge SDL_CocoaWindowData *)sdlwindow->driverdata).nswindow;
            if (nswindow == self) {
                break;
            }
        }
    }

    return sdlwindow;
}

@end

SDL_bool b_inModeTransition;

static CGFloat SqDistanceToRect(const NSPoint *point, const NSRect *rect)
{
    NSPoint edge = *point;
    CGFloat left = NSMinX(*rect), right = NSMaxX(*rect);
    CGFloat bottom = NSMinX(*rect), top = NSMaxY(*rect);
    NSPoint delta;

    if (point->x < left) {
        edge.x = left;
    } else if (point->x > right) {
        edge.x = right;
    }

    if (point->y < bottom) {
        edge.y = bottom;
    } else if (point->y > top) {
        edge.y = top;
    }

    delta = NSMakePoint(edge.x - point->x, edge.y - point->y);
    return delta.x * delta.x + delta.y * delta.y;
}

static NSScreen *ScreenForPoint(const NSPoint *point)
{
    NSScreen *screen;

    /* Do a quick check first to see if the point lies on a specific screen*/
    for (NSScreen *candidate in [NSScreen screens]) {
        if (NSPointInRect(*point, [candidate frame])) {
            screen = candidate;
            break;
        }
    }

    /* Find the screen the point is closest to */
    if (!screen) {
        CGFloat closest = MAXFLOAT;
        for (NSScreen *candidate in [NSScreen screens]) {
            NSRect screenRect = [candidate frame];

            CGFloat sqdist = SqDistanceToRect(point, &screenRect);
            if (sqdist < closest) {
                screen = candidate;
                closest = sqdist;
            }
        }
    }

    return screen;
}

static NSScreen *ScreenForRect(const NSRect *rect)
{
    NSPoint center = NSMakePoint(NSMidX(*rect), NSMidY(*rect));
    return ScreenForPoint(&center);
}

static void ConvertNSRect(BOOL fullscreen, NSRect *r)
{
    r->origin.y = CGDisplayPixelsHigh(kCGDirectMainDisplay) - r->origin.y - r->size.height;
}

static void ScheduleContextUpdates(SDL_CocoaWindowData *data)
{
/* We still support OpenGL as long as Apple offers it, deprecated or not, so disable deprecation warnings about it. */
#ifdef SDL_VIDEO_OPENGL

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

    NSOpenGLContext *currentContext;
    NSMutableArray *contexts;
    if (!data || !data.nscontexts) {
        return;
    }

    currentContext = [NSOpenGLContext currentContext];
    contexts = data.nscontexts;
    @synchronized(contexts) {
        for (SDLOpenGLContext *context in contexts) {
            if (context == currentContext) {
                [context update];
            } else {
                [context scheduleUpdate];
            }
        }
    }

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif /* SDL_VIDEO_OPENGL */
}

/* !!! FIXME: this should use a hint callback. */
static int GetHintCtrlClickEmulateRightClick(void)
{
    return SDL_GetHintBoolean(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, SDL_FALSE);
}

static NSUInteger GetWindowWindowedStyle(SDL_Window *window)
{
    /* IF YOU CHANGE ANY FLAGS IN HERE, PLEASE READ
       the NSWindowStyleMaskBorderless comments in SetupWindowData()! */

    /* always allow miniaturization, otherwise you can't programmatically
       minimize the window, whether there's a title bar or not */
    NSUInteger style = NSWindowStyleMaskMiniaturizable;

    if (!SDL_WINDOW_IS_POPUP(window)) {
        if (window->flags & SDL_WINDOW_BORDERLESS) {
            style |= NSWindowStyleMaskBorderless;
        } else {
            style |= (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable);
        }
        if (window->flags & SDL_WINDOW_RESIZABLE) {
            style |= NSWindowStyleMaskResizable;
        }
    } else {
        style |= NSWindowStyleMaskBorderless;
    }
    return style;
}

static NSUInteger GetWindowStyle(SDL_Window *window)
{
    NSUInteger style = 0;

    if (window->flags & SDL_WINDOW_FULLSCREEN) {
        style = NSWindowStyleMaskBorderless;
    } else {
        style = GetWindowWindowedStyle(window);
    }
    return style;
}

static SDL_bool SetWindowStyle(SDL_Window *window, NSUInteger style)
{
    SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
    NSWindow *nswindow = data.nswindow;

    /* The view responder chain gets messed with during setStyleMask */
    if ([data.sdlContentView nextResponder] == data.listener) {
        [data.sdlContentView setNextResponder:nil];
    }

    [nswindow setStyleMask:style];

    /* The view responder chain gets messed with during setStyleMask */
    if ([data.sdlContentView nextResponder] != data.listener) {
        [data.sdlContentView setNextResponder:data.listener];
    }

    return SDL_TRUE;
}

static SDL_bool ShouldAdjustCoordinatesForGrab(SDL_Window *window)
{
    SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

    if (!data || [data.listener isMovingOrFocusClickPending]) {
        return SDL_FALSE;
    }

    if (!(window->flags & SDL_WINDOW_INPUT_FOCUS)) {
        return SDL_FALSE;
    }

    if ((window->flags & SDL_WINDOW_MOUSE_GRABBED) || (window->mouse_rect.w > 0 && window->mouse_rect.h > 0)) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static SDL_bool AdjustCoordinatesForGrab(SDL_Window *window, float x, float y, CGPoint *adjusted)
{
    if (window->mouse_rect.w > 0 && window->mouse_rect.h > 0) {
        SDL_Rect window_rect;
        SDL_Rect mouse_rect;

        window_rect.x = 0;
        window_rect.y = 0;
        window_rect.w = window->w;
        window_rect.h = window->h;

        if (SDL_GetRectIntersection(&window->mouse_rect, &window_rect, &mouse_rect)) {
            float left = (float)window->x + mouse_rect.x;
            float right = left + mouse_rect.w - 1;
            float top = (float)window->y + mouse_rect.y;
            float bottom = top + mouse_rect.h - 1;
            if (x < left || x > right || y < top || y > bottom) {
                adjusted->x = SDL_clamp(x, left, right);
                adjusted->y = SDL_clamp(y, top, bottom);
                return SDL_TRUE;
            }
            return SDL_FALSE;
        }
    }

    if (window->flags & SDL_WINDOW_MOUSE_GRABBED) {
        float left = (float)window->x;
        float right = left + window->w - 1;
        float top = (float)window->y;
        float bottom = top + window->h - 1;
        if (x < left || x > right || y < top || y > bottom) {
            adjusted->x = SDL_clamp(x, left, right);
            adjusted->y = SDL_clamp(y, top, bottom);
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static void Cocoa_UpdateClipCursor(SDL_Window *window)
{
    SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_13_2) {
        NSWindow *nswindow = data.nswindow;
        SDL_Rect mouse_rect;

        SDL_zero(mouse_rect);

        if (ShouldAdjustCoordinatesForGrab(window)) {
            SDL_Rect window_rect;

            window_rect.x = 0;
            window_rect.y = 0;
            window_rect.w = window->w;
            window_rect.h = window->h;

            if (window->mouse_rect.w > 0 && window->mouse_rect.h > 0) {
                SDL_GetRectIntersection(&window->mouse_rect, &window_rect, &mouse_rect);
            }

            if ((window->flags & SDL_WINDOW_MOUSE_GRABBED) != 0 &&
                SDL_RectEmpty(&mouse_rect)) {
                SDL_memcpy(&mouse_rect, &window_rect, sizeof(mouse_rect));
            }
        }

        if (SDL_RectEmpty(&mouse_rect)) {
            nswindow.mouseConfinementRect = NSZeroRect;
        } else {
            NSRect rect;
            rect.origin.x = mouse_rect.x;
            rect.origin.y = [nswindow contentLayoutRect].size.height - mouse_rect.y - mouse_rect.h;
            rect.size.width = mouse_rect.w;
            rect.size.height = mouse_rect.h;
            nswindow.mouseConfinementRect = rect;
        }
    } else {
        /* Move the cursor to the nearest point in the window */
        if (ShouldAdjustCoordinatesForGrab(window)) {
            float x, y;
            CGPoint cgpoint;

            SDL_GetGlobalMouseState(&x, &y);
            if (AdjustCoordinatesForGrab(window, x, y, &cgpoint)) {
                Cocoa_HandleMouseWarp(cgpoint.x, cgpoint.y);
                CGDisplayMoveCursorToPoint(kCGDirectMainDisplay, cgpoint);
            }
        }
    }
}

static SDL_Window *GetTopmostWindow(SDL_Window *window)
{
    SDL_Window *topmost = window;

    /* Find the topmost parent */
    while (topmost->parent != NULL) {
        topmost = topmost->parent;
    }

    return topmost;
}

static void Cocoa_SetKeyboardFocus(SDL_Window *window)
{
    SDL_Window *topmost = GetTopmostWindow(window);
    SDL_CocoaWindowData *topmost_data;

    topmost_data = (__bridge SDL_CocoaWindowData *)topmost->driverdata;
    topmost_data.keyboard_focus = window;
    SDL_SetKeyboardFocus(window);
}

static void Cocoa_SendExposedEventIfVisible(SDL_Window *window)
{
    NSWindow *nswindow = ((__bridge SDL_CocoaWindowData *)window->driverdata).nswindow;
    if ([nswindow occlusionState] & NSWindowOcclusionStateVisible) {
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_EXPOSED, 0, 0);
    }
}

static void Cocoa_WaitForMiniaturizable(SDL_Window *window)
{
    NSWindow *nswindow = ((__bridge SDL_CocoaWindowData *)window->driverdata).nswindow;
    NSButton *button = [nswindow standardWindowButton:NSWindowMiniaturizeButton];
    if (button) {
        int iterations = 0;
        while (![button isEnabled] && (iterations < 100)) {
            SDL_Delay(10);
            SDL_PumpEvents();
            iterations++;
        }
    }
}

static SDL_bool Cocoa_IsZoomed(SDL_Window *window)
{
    SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

    data.checking_zoom = YES;
    const SDL_bool ret = [data.nswindow isZoomed];
    data.checking_zoom = NO;

    return ret;
}

@implementation Cocoa_WindowListener

- (void)listen:(SDL_CocoaWindowData *)data
{
    NSNotificationCenter *center;
    NSWindow *window = data.nswindow;
    NSView *view = data.sdlContentView;

    _data = data;
    observingVisible = YES;
    wasCtrlLeft = NO;
    wasVisible = [window isVisible];
    isFullscreenSpace = NO;
    inFullscreenTransition = NO;
    pendingWindowOperation = PENDING_OPERATION_NONE;
    isMoving = NO;
    isMiniaturizing = NO;
    isDragAreaRunning = NO;
    pendingWindowWarpX = pendingWindowWarpY = FLT_MAX;

    center = [NSNotificationCenter defaultCenter];

    if ([window delegate] != nil) {
        [center addObserver:self selector:@selector(windowDidExpose:) name:NSWindowDidExposeNotification object:window];
        [center addObserver:self selector:@selector(windowWillMove:) name:NSWindowWillMoveNotification object:window];
        [center addObserver:self selector:@selector(windowDidMove:) name:NSWindowDidMoveNotification object:window];
        [center addObserver:self selector:@selector(windowDidResize:) name:NSWindowDidResizeNotification object:window];
        [center addObserver:self selector:@selector(windowWillMiniaturize:) name:NSWindowWillMiniaturizeNotification object:window];
        [center addObserver:self selector:@selector(windowDidMiniaturize:) name:NSWindowDidMiniaturizeNotification object:window];
        [center addObserver:self selector:@selector(windowDidDeminiaturize:) name:NSWindowDidDeminiaturizeNotification object:window];
        [center addObserver:self selector:@selector(windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:window];
        [center addObserver:self selector:@selector(windowDidResignKey:) name:NSWindowDidResignKeyNotification object:window];
        [center addObserver:self selector:@selector(windowDidChangeBackingProperties:) name:NSWindowDidChangeBackingPropertiesNotification object:window];
        [center addObserver:self selector:@selector(windowDidChangeScreenProfile:) name:NSWindowDidChangeScreenProfileNotification object:window];
        [center addObserver:self selector:@selector(windowDidChangeScreen:) name:NSWindowDidChangeScreenNotification object:window];
        [center addObserver:self selector:@selector(windowWillEnterFullScreen:) name:NSWindowWillEnterFullScreenNotification object:window];
        [center addObserver:self selector:@selector(windowDidEnterFullScreen:) name:NSWindowDidEnterFullScreenNotification object:window];
        [center addObserver:self selector:@selector(windowWillExitFullScreen:) name:NSWindowWillExitFullScreenNotification object:window];
        [center addObserver:self selector:@selector(windowDidExitFullScreen:) name:NSWindowDidExitFullScreenNotification object:window];
        [center addObserver:self selector:@selector(windowDidFailToEnterFullScreen:) name:@"NSWindowDidFailToEnterFullScreenNotification" object:window];
        [center addObserver:self selector:@selector(windowDidFailToExitFullScreen:) name:@"NSWindowDidFailToExitFullScreenNotification" object:window];
        [center addObserver:self selector:@selector(windowDidChangeOcclusionState:) name:NSWindowDidChangeOcclusionStateNotification object:window];
    } else {
        [window setDelegate:self];
    }

    /* Haven't found a delegate / notification that triggers when the window is
     * ordered out (is not visible any more). You can be ordered out without
     * minimizing, so DidMiniaturize doesn't work. (e.g. -[NSWindow orderOut:])
     */
    [window addObserver:self
             forKeyPath:@"visible"
                options:NSKeyValueObservingOptionNew
                context:NULL];

    [window setNextResponder:self];
    [window setAcceptsMouseMovedEvents:YES];

    [view setNextResponder:self];

    [view setAcceptsTouchEvents:YES];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    if (!observingVisible) {
        return;
    }

    if (object == _data.nswindow && [keyPath isEqualToString:@"visible"]) {
        int newVisibility = [[change objectForKey:@"new"] intValue];
        if (newVisibility) {
            SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_SHOWN, 0, 0);
        } else if (![_data.nswindow isMiniaturized]) {
            SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_HIDDEN, 0, 0);
        }
    }
}

- (void)pauseVisibleObservation
{
    observingVisible = NO;
    wasVisible = [_data.nswindow isVisible];
}

- (void)resumeVisibleObservation
{
    BOOL isVisible = [_data.nswindow isVisible];
    observingVisible = YES;
    if (wasVisible != isVisible) {
        if (isVisible) {
            SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_SHOWN, 0, 0);
        } else if (![_data.nswindow isMiniaturized]) {
            SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_HIDDEN, 0, 0);
        }

        wasVisible = isVisible;
    }
}

- (BOOL)setFullscreenSpace:(BOOL)state
{
    SDL_Window *window = _data.window;
    NSWindow *nswindow = _data.nswindow;
    SDL_CocoaVideoData *videodata = ((__bridge SDL_CocoaWindowData *)window->driverdata).videodata;

    if (!videodata.allow_spaces) {
        return NO; /* Spaces are forcibly disabled. */
    } else if (state && window->fullscreen_exclusive) {
        return NO; /* we only allow you to make a Space on fullscreen desktop windows. */
    } else if (!state && window->last_fullscreen_exclusive_display) {
        return NO; /* we only handle leaving the Space on windows that were previously fullscreen desktop. */
    } else if (state == isFullscreenSpace) {
        return YES; /* already there. */
    }

    if (inFullscreenTransition) {
        if (state) {
            [self addPendingWindowOperation:PENDING_OPERATION_ENTER_FULLSCREEN];
        } else {
            [self addPendingWindowOperation:PENDING_OPERATION_LEAVE_FULLSCREEN];
        }
        return YES;
    }
    inFullscreenTransition = YES;

    /* you need to be FullScreenPrimary, or toggleFullScreen doesn't work. Unset it again in windowDidExitFullScreen. */
    [nswindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    [nswindow performSelectorOnMainThread:@selector(toggleFullScreen:) withObject:nswindow waitUntilDone:NO];
    return YES;
}

- (BOOL)isInFullscreenSpace
{
    return isFullscreenSpace;
}

- (BOOL)isInFullscreenSpaceTransition
{
    return inFullscreenTransition;
}

- (void)clearPendingWindowOperation:(PendingWindowOperation)operation
{
    pendingWindowOperation &= ~operation;
}

- (void)addPendingWindowOperation:(PendingWindowOperation)operation
{
    pendingWindowOperation |= operation;
}

- (BOOL)windowOperationIsPending:(PendingWindowOperation)operation
{
    return !!(pendingWindowOperation & operation);
}

- (BOOL)hasPendingWindowOperation
{
    return pendingWindowOperation != PENDING_OPERATION_NONE ||
           isMiniaturizing || inFullscreenTransition;
}

- (void)close
{
    NSNotificationCenter *center;
    NSWindow *window = _data.nswindow;
    NSView *view = [window contentView];

    center = [NSNotificationCenter defaultCenter];

    if ([window delegate] != self) {
        [center removeObserver:self name:NSWindowDidExposeNotification object:window];
        [center removeObserver:self name:NSWindowWillMoveNotification object:window];
        [center removeObserver:self name:NSWindowDidMoveNotification object:window];
        [center removeObserver:self name:NSWindowDidResizeNotification object:window];
        [center removeObserver:self name:NSWindowWillMiniaturizeNotification object:window];
        [center removeObserver:self name:NSWindowDidMiniaturizeNotification object:window];
        [center removeObserver:self name:NSWindowDidDeminiaturizeNotification object:window];
        [center removeObserver:self name:NSWindowDidBecomeKeyNotification object:window];
        [center removeObserver:self name:NSWindowDidResignKeyNotification object:window];
        [center removeObserver:self name:NSWindowDidChangeBackingPropertiesNotification object:window];
        [center removeObserver:self name:NSWindowDidChangeScreenProfileNotification object:window];
        [center removeObserver:self name:NSWindowDidChangeScreenNotification object:window];
        [center removeObserver:self name:NSWindowWillEnterFullScreenNotification object:window];
        [center removeObserver:self name:NSWindowDidEnterFullScreenNotification object:window];
        [center removeObserver:self name:NSWindowWillExitFullScreenNotification object:window];
        [center removeObserver:self name:NSWindowDidExitFullScreenNotification object:window];
        [center removeObserver:self name:@"NSWindowDidFailToEnterFullScreenNotification" object:window];
        [center removeObserver:self name:@"NSWindowDidFailToExitFullScreenNotification" object:window];
        [center removeObserver:self name:NSWindowDidChangeOcclusionStateNotification object:window];
    } else {
        [window setDelegate:nil];
    }

    [window removeObserver:self forKeyPath:@"visible"];

    if ([window nextResponder] == self) {
        [window setNextResponder:nil];
    }
    if ([view nextResponder] == self) {
        [view setNextResponder:nil];
    }
}

- (BOOL)isMoving
{
    return isMoving;
}

- (BOOL)isMovingOrFocusClickPending
{
    return isMoving || (focusClickPending != 0);
}

- (void)setFocusClickPending:(NSInteger)button
{
    focusClickPending |= (1 << button);
}

- (void)clearFocusClickPending:(NSInteger)button
{
    if (focusClickPending & (1 << button)) {
        focusClickPending &= ~(1 << button);
        if (focusClickPending == 0) {
            [self onMovingOrFocusClickPendingStateCleared];
        }
    }
}

- (void)updateIgnoreMouseState:(NSEvent *)theEvent
{
    SDL_Window *window = _data.window;
    SDL_Surface *shape = (SDL_Surface *)SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_SHAPE_POINTER, NULL);
    BOOL ignoresMouseEvents = NO;

    if (shape) {
        NSPoint point = [theEvent locationInWindow];
		NSRect windowRect = [[_data.nswindow contentView] frame];
		if (NSMouseInRect(point, windowRect, NO)) {
			int x = (int)SDL_roundf((point.x / (window->w - 1)) * (shape->w - 1));
			int y = (int)SDL_roundf(((window->h - point.y) / (window->h - 1)) * (shape->h - 1));
			Uint8 a;

			if (SDL_ReadSurfacePixel(shape, x, y, NULL, NULL, NULL, &a) < 0 || a == SDL_ALPHA_TRANSPARENT) {
				ignoresMouseEvents = YES;
			}
		}
    }
    _data.nswindow.ignoresMouseEvents = ignoresMouseEvents;
}

- (void)setPendingMoveX:(float)x Y:(float)y
{
    pendingWindowWarpX = x;
    pendingWindowWarpY = y;
}

- (void)windowDidFinishMoving
{
    if (isMoving) {
        isMoving = NO;
        [self onMovingOrFocusClickPendingStateCleared];
    }
}

- (void)onMovingOrFocusClickPendingStateCleared
{
    if (![self isMovingOrFocusClickPending]) {
        SDL_Mouse *mouse = SDL_GetMouse();
        if (pendingWindowWarpX != FLT_MAX && pendingWindowWarpY != FLT_MAX) {
            mouse->WarpMouseGlobal(pendingWindowWarpX, pendingWindowWarpY);
            pendingWindowWarpX = pendingWindowWarpY = FLT_MAX;
        }
        if (mouse->relative_mode && !mouse->relative_mode_warp && mouse->focus == _data.window) {
            /* Move the cursor to the nearest point in the window */
            {
                float x, y;
                CGPoint cgpoint;

                SDL_GetMouseState(&x, &y);
                cgpoint.x = _data.window->x + x;
                cgpoint.y = _data.window->y + y;

                Cocoa_HandleMouseWarp(cgpoint.x, cgpoint.y);

                DLog("Returning cursor to (%g, %g)", cgpoint.x, cgpoint.y);
                CGDisplayMoveCursorToPoint(kCGDirectMainDisplay, cgpoint);
            }

            mouse->SetRelativeMouseMode(SDL_TRUE);
        } else {
            Cocoa_UpdateClipCursor(_data.window);
        }
    }
}

- (BOOL)windowShouldClose:(id)sender
{
    SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
    return NO;
}

- (void)windowDidExpose:(NSNotification *)aNotification
{
    Cocoa_SendExposedEventIfVisible(_data.window);
}

- (void)windowDidChangeOcclusionState:(NSNotification *)aNotification
{
    if ([_data.nswindow occlusionState] & NSWindowOcclusionStateVisible) {
        SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_EXPOSED, 0, 0);
    } else {
        SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_OCCLUDED, 0, 0);
    }
}

- (void)windowWillMove:(NSNotification *)aNotification
{
    if ([_data.nswindow isKindOfClass:[SDLWindow class]]) {
        pendingWindowWarpX = pendingWindowWarpY = FLT_MAX;
        isMoving = YES;
    }
}

- (void)windowDidMove:(NSNotification *)aNotification
{
    int x, y;
    SDL_Window *window = _data.window;
    NSWindow *nswindow = _data.nswindow;
    BOOL fullscreen = (window->flags & SDL_WINDOW_FULLSCREEN) ? YES : NO;
    NSRect rect = [nswindow contentRectForFrameRect:[nswindow frame]];
    ConvertNSRect(fullscreen, &rect);

    if (inFullscreenTransition || b_inModeTransition) {
        /* We'll take care of this at the end of the transition */
        return;
    }

    x = (int)rect.origin.x;
    y = (int)rect.origin.y;

    ScheduleContextUpdates(_data);

    /* Get the parent-relative coordinates for child windows. */
    SDL_GlobalToRelativeForWindow(window, x, y, &x, &y);
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, x, y);
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
    SDL_Window *window = _data.window;

    /* XXX: Calling [isZoomed] calls this function, and calling [isZoomed]
     *      from within this function will recurse until the stack overflows,
     *      so a recursion guard is required.
     */
    if (!_data.checking_zoom) {
        _data.checking_zoom = YES;
        if ([_data.nswindow isZoomed] && !_data.was_zoomed && _data.send_floating_size) {
            NSRect rect;

            _data.send_floating_size = NO;
            rect.origin.x = window->floating.x;
            rect.origin.y = window->floating.y;
            rect.size.width = window->floating.w;
            rect.size.height = window->floating.h;
            ConvertNSRect(SDL_FALSE, &rect);

            frameSize = rect.size;
        }
        _data.checking_zoom = NO;
    }

    return frameSize;
}

- (void)windowDidResize:(NSNotification *)aNotification
{
    SDL_Window *window;
    NSWindow *nswindow;
    NSRect rect;
    int x, y, w, h;
    BOOL zoomed;
    BOOL fullscreen;

    if (inFullscreenTransition || b_inModeTransition) {
        /* We'll take care of this at the end of the transition */
        return;
    }

    if (focusClickPending) {
        focusClickPending = 0;
        [self onMovingOrFocusClickPendingStateCleared];
    }
    window = _data.window;
    nswindow = _data.nswindow;
    rect = [nswindow contentRectForFrameRect:[nswindow frame]];
    fullscreen = (window->flags & SDL_WINDOW_FULLSCREEN) ? YES : NO;
    ConvertNSRect(fullscreen, &rect);
    x = (int)rect.origin.x;
    y = (int)rect.origin.y;
    w = (int)rect.size.width;
    h = (int)rect.size.height;

    ScheduleContextUpdates(_data);

    /* isZoomed always returns true if the window is not resizable
     * and fullscreen windows are considered zoomed.
     */
    if ((window->flags & SDL_WINDOW_RESIZABLE) && Cocoa_IsZoomed(window) &&
        !(window->flags & SDL_WINDOW_FULLSCREEN) && ![self isInFullscreenSpace]) {
        zoomed = YES;
    } else {
        zoomed = NO;
    }
    if (!zoomed) {
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESTORED, 0, 0);
    } else if (zoomed) {
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MAXIMIZED, 0, 0);
        if ([self windowOperationIsPending:PENDING_OPERATION_MINIMIZE]) {
            [nswindow miniaturize:nil];
        }
    }

    /* The window can move during a resize event, such as when maximizing
       or resizing from a corner */
    SDL_GlobalToRelativeForWindow(window, x, y, &x, &y);
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, x, y);
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, w, h);
}

- (void)windowWillMiniaturize:(NSNotification *)aNotification
{
    isMiniaturizing = YES;
    Cocoa_WaitForMiniaturizable(_data.window);
}

- (void)windowDidMiniaturize:(NSNotification *)aNotification
{
    if (focusClickPending) {
        focusClickPending = 0;
        [self onMovingOrFocusClickPendingStateCleared];
    }
    isMiniaturizing = NO;
    [self clearPendingWindowOperation:PENDING_OPERATION_MINIMIZE];
    SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
}

- (void)windowDidDeminiaturize:(NSNotification *)aNotification
{
    /* Always send restored before maximized. */
    SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_RESTORED, 0, 0);

    /* isZoomed always returns true if the window is not resizable. */
    if ((_data.window->flags & SDL_WINDOW_RESIZABLE) && Cocoa_IsZoomed(_data.window)) {
        SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_MAXIMIZED, 0, 0);
    }

    if ([self windowOperationIsPending:PENDING_OPERATION_ENTER_FULLSCREEN]) {
        SDL_UpdateFullscreenMode(_data.window, SDL_TRUE, SDL_TRUE);
    }
}

- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
    SDL_Window *window = _data.window;
    SDL_Mouse *mouse = SDL_GetMouse();

    /* We're going to get keyboard events, since we're key. */
    /* This needs to be done before restoring the relative mouse mode. */
    Cocoa_SetKeyboardFocus(_data.keyboard_focus ? _data.keyboard_focus : window);

    if (mouse->relative_mode && !mouse->relative_mode_warp && ![self isMovingOrFocusClickPending]) {
        mouse->SetRelativeMouseMode(SDL_TRUE);
    }

    /* If we just gained focus we need the updated mouse position */
    if (!mouse->relative_mode) {
        NSPoint point;
        float x, y;

        point = [_data.nswindow mouseLocationOutsideOfEventStream];
        x = point.x;
        y = (window->h - point.y);

        if (x >= 0.0f && x < (float)window->w && y >= 0.0f && y < (float)window->h) {
            SDL_SendMouseMotion(0, window, mouse->mouseID, 0, x, y);
        }
    }

    /* Check to see if someone updated the clipboard */
    Cocoa_CheckClipboardUpdate(_data.videodata);

    if (isFullscreenSpace && !window->fullscreen_exclusive) {
        [NSMenu setMenuBarVisible:NO];
    }
    {
        const unsigned int newflags = [NSEvent modifierFlags] & NSEventModifierFlagCapsLock;
        _data.videodata.modifierFlags = (_data.videodata.modifierFlags & ~NSEventModifierFlagCapsLock) | newflags;
        SDL_ToggleModState(SDL_KMOD_CAPS, newflags ? SDL_TRUE : SDL_FALSE);
    }
}

- (void)windowDidResignKey:(NSNotification *)aNotification
{
    SDL_Mouse *mouse = SDL_GetMouse();
    if (mouse->relative_mode && !mouse->relative_mode_warp) {
        mouse->SetRelativeMouseMode(SDL_FALSE);
    }

    /* Some other window will get mouse events, since we're not key. */
    if (SDL_GetMouseFocus() == _data.window) {
        SDL_SetMouseFocus(NULL);
    }

    /* Some other window will get keyboard events, since we're not key. */
    if (SDL_GetKeyboardFocus() == _data.window) {
        SDL_SetKeyboardFocus(NULL);
    }

    if (isFullscreenSpace) {
        [NSMenu setMenuBarVisible:YES];
    }
}

- (void)windowDidChangeBackingProperties:(NSNotification *)aNotification
{
    NSNumber *oldscale = [[aNotification userInfo] objectForKey:NSBackingPropertyOldScaleFactorKey];

    if (inFullscreenTransition) {
        return;
    }

    if ([oldscale doubleValue] != [_data.nswindow backingScaleFactor]) {
        /* Force a resize event when the backing scale factor changes. */
        _data.window->w = 0;
        _data.window->h = 0;
        [self windowDidResize:aNotification];
    }
}

- (void)windowDidChangeScreenProfile:(NSNotification *)aNotification
{
    SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_ICCPROF_CHANGED, 0, 0);
}

- (void)windowDidChangeScreen:(NSNotification *)aNotification
{
    /*printf("WINDOWDIDCHANGESCREEN\n");*/

#ifdef SDL_VIDEO_OPENGL

    if (_data && _data.nscontexts) {
        for (SDLOpenGLContext *context in _data.nscontexts) {
            [context movedToNewScreen];
        }
    }

#endif /* SDL_VIDEO_OPENGL */
}

- (void)windowWillEnterFullScreen:(NSNotification *)aNotification
{
    SDL_Window *window = _data.window;
    const NSUInteger flags = NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskTitled;

    /* For some reason, the fullscreen window won't get any mouse button events
     * without the NSWindowStyleMaskTitled flag being set when entering fullscreen,
     * so it's needed even if the window is borderless.
     */
    SetWindowStyle(window, flags);

    _data.was_zoomed = !!(window->flags & SDL_WINDOW_MAXIMIZED);

    isFullscreenSpace = YES;
    inFullscreenTransition = YES;
}

- (void)windowDidFailToEnterFullScreen:(NSNotification *)aNotification
{
    SDL_Window *window = _data.window;

    if (window->is_destroying) {
        return;
    }

    SetWindowStyle(window, GetWindowStyle(window));

    [self clearPendingWindowOperation:PENDING_OPERATION_ENTER_FULLSCREEN];
    isFullscreenSpace = NO;
    inFullscreenTransition = NO;

    [self windowDidExitFullScreen:nil];
}

- (void)windowDidEnterFullScreen:(NSNotification *)aNotification
{
    SDL_Window *window = _data.window;

    inFullscreenTransition = NO;
    [self clearPendingWindowOperation:PENDING_OPERATION_ENTER_FULLSCREEN];

    if ([self windowOperationIsPending:PENDING_OPERATION_LEAVE_FULLSCREEN]) {
        [self setFullscreenSpace:NO];
    } else {
        if (window->fullscreen_exclusive) {
            [NSMenu setMenuBarVisible:NO];
        }

        /* Don't recurse back into UpdateFullscreenMode() if this was hit in
         * a blocking transition, as the caller is already waiting in
         * UpdateFullscreenMode().
         */
        if (!_data.in_blocking_transition) {
            SDL_UpdateFullscreenMode(window, SDL_TRUE, SDL_FALSE);
        }
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_ENTER_FULLSCREEN, 0, 0);

        /* Force the size change event in case it was delivered earlier
           while the window was still animating into place.
         */
        window->w = 0;
        window->h = 0;
        [self windowDidMove:aNotification];
        [self windowDidResize:aNotification];
    }
}

- (void)windowWillExitFullScreen:(NSNotification *)aNotification
{
    SDL_Window *window = _data.window;

    /* If the windowed mode borders were toggled on while in a fullscreen space,
     * NSWindowStyleMaskTitled has to be cleared here, or the window can end up
     * in a weird, semi-decorated state upon returning to windowed mode.
     */
    if (_data.border_toggled && !(window->flags & SDL_WINDOW_BORDERLESS)) {
        const NSUInteger flags = NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

        SetWindowStyle(window, flags);
        _data.border_toggled = SDL_FALSE;
    }

    isFullscreenSpace = NO;
    inFullscreenTransition = YES;
}

- (void)windowDidFailToExitFullScreen:(NSNotification *)aNotification
{
    SDL_Window *window = _data.window;
    const NSUInteger flags = NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

    if (window->is_destroying) {
        return;
    }

    SetWindowStyle(window, flags);

    isFullscreenSpace = YES;
    inFullscreenTransition = NO;

    [self windowDidEnterFullScreen:nil];
}

- (void)windowDidExitFullScreen:(NSNotification *)aNotification
{
    SDL_Window *window = _data.window;
    NSWindow *nswindow = _data.nswindow;

    inFullscreenTransition = NO;

    /* As of macOS 10.15, the window decorations can go missing sometimes after
       certain fullscreen-desktop->exlusive-fullscreen->windowed mode flows
       sometimes. Making sure the style mask always uses the windowed mode style
       when returning to windowed mode from a space (instead of using a pending
       fullscreen mode style mask) seems to work around that issue.
     */
    SetWindowStyle(window, GetWindowWindowedStyle(window));

    /* Don't recurse back into UpdateFullscreenMode() if this was hit in
     * a blocking transition, as the caller is already waiting in
     * UpdateFullscreenMode().
     */
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_LEAVE_FULLSCREEN, 0, 0);
    if (!_data.in_blocking_transition) {
        SDL_UpdateFullscreenMode(window, SDL_FALSE, SDL_FALSE);
    }

    if (window->flags & SDL_WINDOW_ALWAYS_ON_TOP) {
        [nswindow setLevel:NSFloatingWindowLevel];
    } else {
        [nswindow setLevel:kCGNormalWindowLevel];
    }

    [self clearPendingWindowOperation:PENDING_OPERATION_LEAVE_FULLSCREEN];

    if ([self windowOperationIsPending:PENDING_OPERATION_ENTER_FULLSCREEN]) {
        [self setFullscreenSpace:YES];
    } else if ([self windowOperationIsPending:PENDING_OPERATION_MINIMIZE]) {
        /* There's some state that isn't quite back to normal when
         * windowDidExitFullScreen triggers. For example, the minimize button on
         * the title bar doesn't actually enable for another 200 milliseconds or
         * so on this MacBook. Camp here and wait for that to happen before
         * going on, in case we're exiting fullscreen to minimize, which need
         * that window state to be normal before it will work.
         */
        Cocoa_WaitForMiniaturizable(_data.window);
        [self addPendingWindowOperation:PENDING_OPERATION_ENTER_FULLSCREEN];
        [nswindow miniaturize:nil];
    } else {
        /* Adjust the fullscreen toggle button and readd menu now that we're here. */
        if (window->flags & SDL_WINDOW_RESIZABLE) {
            /* resizable windows are Spaces-friendly: they get the "go fullscreen" toggle button on their titlebar. */
            [nswindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        } else {
            [nswindow setCollectionBehavior:NSWindowCollectionBehaviorManaged];
        }
        [NSMenu setMenuBarVisible:YES];

        /* Restore windowed size and position in case it changed while fullscreen */
        NSRect rect;
        rect.origin.x = _data.was_zoomed ? window->windowed.x : window->floating.x;
        rect.origin.y = _data.was_zoomed ? window->windowed.y : window->floating.y;
        rect.size.width = _data.was_zoomed ? window->windowed.w : window->floating.w;
        rect.size.height = _data.was_zoomed ? window->windowed.h : window->floating.h;
        ConvertNSRect(NO, &rect);

        _data.send_floating_position = NO;
        _data.send_floating_size = NO;
        [nswindow setContentSize:rect.size];
        [nswindow setFrameOrigin:rect.origin];

        /* Force the size change event in case it was delivered earlier
         * while the window was still animating into place.
         */
        window->w = 0;
        window->h = 0;
        [self windowDidMove:aNotification];
        [self windowDidResize:aNotification];

        _data.was_zoomed = SDL_FALSE;

        /* FIXME: Why does the window get hidden? */
        if (!(window->flags & SDL_WINDOW_HIDDEN)) {
            Cocoa_ShowWindow(SDL_GetVideoDevice(), window);
        }
    }
}

- (NSApplicationPresentationOptions)window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
{
    if (_data.window->fullscreen_exclusive) {
        return NSApplicationPresentationFullScreen | NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar;
    } else {
        return proposedOptions;
    }
}

/* We'll respond to key events by mostly doing nothing so we don't beep.
 * We could handle key messages here, but we lose some in the NSApp dispatch,
 * where they get converted to action messages, etc.
 */
- (void)flagsChanged:(NSEvent *)theEvent
{
    /*Cocoa_HandleKeyEvent(SDL_GetVideoDevice(), theEvent);*/

    /* Catch capslock in here as a special case:
       https://developer.apple.com/library/archive/qa/qa1519/_index.html
       Note that technote's check of keyCode doesn't work. At least on the
       10.15 beta, capslock comes through here as keycode 255, but it's safe
       to send duplicate key events; SDL filters them out quickly in
       SDL_SendKeyboardKey(). */

    /* Also note that SDL_SendKeyboardKey expects all capslock events to be
       keypresses; it won't toggle the mod state if you send a keyrelease.  */
    const SDL_bool osenabled = ([theEvent modifierFlags] & NSEventModifierFlagCapsLock) ? SDL_TRUE : SDL_FALSE;
    const SDL_bool sdlenabled = (SDL_GetModState() & SDL_KMOD_CAPS) ? SDL_TRUE : SDL_FALSE;
    if (osenabled ^ sdlenabled) {
        SDL_SendKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_CAPSLOCK);
        SDL_SendKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_CAPSLOCK);
    }
}
- (void)keyDown:(NSEvent *)theEvent
{
    /*Cocoa_HandleKeyEvent(SDL_GetVideoDevice(), theEvent);*/
}
- (void)keyUp:(NSEvent *)theEvent
{
    /*Cocoa_HandleKeyEvent(SDL_GetVideoDevice(), theEvent);*/
}

/* We'll respond to selectors by doing nothing so we don't beep.
 * The escape key gets converted to a "cancel" selector, etc.
 */
- (void)doCommandBySelector:(SEL)aSelector
{
    /*NSLog(@"doCommandBySelector: %@\n", NSStringFromSelector(aSelector));*/
}

- (BOOL)processHitTest:(NSEvent *)theEvent
{
    SDL_assert(isDragAreaRunning == [_data.nswindow isMovableByWindowBackground]);

    if (_data.window->hit_test) { /* if no hit-test, skip this. */
        const NSPoint location = [theEvent locationInWindow];
        const SDL_Point point = { (int)location.x, _data.window->h - (((int)location.y) - 1) };
        const SDL_HitTestResult rc = _data.window->hit_test(_data.window, &point, _data.window->hit_test_data);
        if (rc == SDL_HITTEST_DRAGGABLE) {
            if (!isDragAreaRunning) {
                isDragAreaRunning = YES;
                [_data.nswindow setMovableByWindowBackground:YES];
            }
            return YES; /* dragging! */
        }
    }

    if (isDragAreaRunning) {
        isDragAreaRunning = NO;
        [_data.nswindow setMovableByWindowBackground:NO];
        return YES; /* was dragging, drop event. */
    }

    return NO; /* not a special area, carry on. */
}

static int Cocoa_SendMouseButtonClicks(SDL_Mouse *mouse, NSEvent *theEvent, SDL_Window *window, const Uint8 state, const Uint8 button)
{
    const SDL_MouseID mouseID = mouse->mouseID;
    const int clicks = (int)[theEvent clickCount];
    SDL_Window *focus = SDL_GetKeyboardFocus();
    int rc;

    // macOS will send non-left clicks to background windows without raising them, so we need to
    //  temporarily adjust the mouse position when this happens, as `mouse` will be tracking
    //  the position in the currently-focused window. We don't (currently) send a mousemove
    //  event for the background window, this just makes sure the button is reported at the
    //  correct position in its own event.
    if (focus && ([theEvent window] == ((__bridge SDL_CocoaWindowData *)focus->driverdata).nswindow)) {
        rc = SDL_SendMouseButtonClicks(Cocoa_GetEventTimestamp([theEvent timestamp]), window, mouseID, state, button, clicks);
    } else {
        const int orig_x = mouse->x;
        const int orig_y = mouse->y;
        const NSPoint point = [theEvent locationInWindow];
        mouse->x = (int)point.x;
        mouse->y = (int)(window->h - point.y);
        rc = SDL_SendMouseButtonClicks(Cocoa_GetEventTimestamp([theEvent timestamp]), window, mouseID, state, button, clicks);
        mouse->x = orig_x;
        mouse->y = orig_y;
    }

    return rc;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    SDL_Mouse *mouse = SDL_GetMouse();
    int button;

    if (!mouse) {
        return;
    }

    /* Ignore events that aren't inside the client area (i.e. title bar.) */
    if ([theEvent window]) {
        NSRect windowRect = [[[theEvent window] contentView] frame];
        if (!NSMouseInRect([theEvent locationInWindow], windowRect, NO)) {
            return;
        }
    }

    if ([self processHitTest:theEvent]) {
        SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_HIT_TEST, 0, 0);
        return; /* dragging, drop event. */
    }

    switch ([theEvent buttonNumber]) {
    case 0:
        if (([theEvent modifierFlags] & NSEventModifierFlagControl) &&
            GetHintCtrlClickEmulateRightClick()) {
            wasCtrlLeft = YES;
            button = SDL_BUTTON_RIGHT;
        } else {
            wasCtrlLeft = NO;
            button = SDL_BUTTON_LEFT;
        }
        break;
    case 1:
        button = SDL_BUTTON_RIGHT;
        break;
    case 2:
        button = SDL_BUTTON_MIDDLE;
        break;
    default:
        button = (int)[theEvent buttonNumber] + 1;
        break;
    }

    Cocoa_SendMouseButtonClicks(mouse, theEvent, _data.window, SDL_PRESSED, button);
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
    SDL_Mouse *mouse = SDL_GetMouse();
    int button;

    if (!mouse) {
        return;
    }

    if ([self processHitTest:theEvent]) {
        SDL_SendWindowEvent(_data.window, SDL_EVENT_WINDOW_HIT_TEST, 0, 0);
        return; /* stopped dragging, drop event. */
    }

    switch ([theEvent buttonNumber]) {
    case 0:
        if (wasCtrlLeft) {
            button = SDL_BUTTON_RIGHT;
            wasCtrlLeft = NO;
        } else {
            button = SDL_BUTTON_LEFT;
        }
        break;
    case 1:
        button = SDL_BUTTON_RIGHT;
        break;
    case 2:
        button = SDL_BUTTON_MIDDLE;
        break;
    default:
        button = (int)[theEvent buttonNumber] + 1;
        break;
    }

    Cocoa_SendMouseButtonClicks(mouse, theEvent, _data.window, SDL_RELEASED, button);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_MouseID mouseID;
    NSPoint point;
    float x, y;
    SDL_Window *window;

    if (!mouse) {
        return;
    }

    mouseID = mouse->mouseID;
    window = _data.window;

    if (window->flags & SDL_WINDOW_TRANSPARENT) {
        [self updateIgnoreMouseState:theEvent];
    }

    if ([self processHitTest:theEvent]) {
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_HIT_TEST, 0, 0);
        return; /* dragging, drop event. */
    }

    if (mouse->relative_mode) {
        return;
    }

    point = [theEvent locationInWindow];
    x = point.x;
    y = (window->h - point.y);

    if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_13_2) {
        /* Mouse grab is taken care of by the confinement rect */
    } else {
        CGPoint cgpoint;
        if (ShouldAdjustCoordinatesForGrab(window) &&
            AdjustCoordinatesForGrab(window, window->x + x, window->y + y, &cgpoint)) {
            Cocoa_HandleMouseWarp(cgpoint.x, cgpoint.y);
            CGDisplayMoveCursorToPoint(kCGDirectMainDisplay, cgpoint);
            CGAssociateMouseAndMouseCursorPosition(YES);
        }
    }

    SDL_SendMouseMotion(Cocoa_GetEventTimestamp([theEvent timestamp]), window, mouseID, 0, x, y);
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    [self mouseMoved:theEvent];
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    [self mouseMoved:theEvent];
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
    [self mouseMoved:theEvent];
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    Cocoa_HandleMouseWheel(_data.window, theEvent);
}

- (BOOL)isTouchFromTrackpad:(NSEvent *)theEvent
{
    SDL_Window *window = _data.window;
    SDL_CocoaVideoData *videodata = ((__bridge SDL_CocoaWindowData *)window->driverdata).videodata;

    /* if this a MacBook trackpad, we'll make input look like a synthesized
       event. This is backwards from reality, but better matches user
       expectations. You can make it look like a generic touch device instead
       with the SDL_HINT_TRACKPAD_IS_TOUCH_ONLY hint. */
    BOOL istrackpad = NO;
    if (!videodata.trackpad_is_touch_only) {
        @try {
            istrackpad = ([theEvent subtype] == NSEventSubtypeMouseEvent);
        }
        @catch (NSException *e) {
            /* if NSEvent type doesn't have subtype, such as NSEventTypeBeginGesture on
             * macOS 10.5 to 10.10, then NSInternalInconsistencyException is thrown.
             * This still prints a message to terminal so catching it's not an ideal solution.
             *
             * *** Assertion failure in -[NSEvent subtype]
             */
        }
    }
    return istrackpad;
}

- (void)touchesBeganWithEvent:(NSEvent *)theEvent
{
    NSSet *touches;
    SDL_TouchID touchID;
    int existingTouchCount;
    const BOOL istrackpad = [self isTouchFromTrackpad:theEvent];

    touches = [theEvent touchesMatchingPhase:NSTouchPhaseAny inView:nil];
    touchID = istrackpad ? SDL_MOUSE_TOUCHID : (SDL_TouchID)(intptr_t)[[touches anyObject] device];
    existingTouchCount = 0;

    for (NSTouch *touch in touches) {
        if ([touch phase] != NSTouchPhaseBegan) {
            existingTouchCount++;
        }
    }
    if (existingTouchCount == 0) {
        int numFingers = SDL_GetNumTouchFingers(touchID);
        DLog("Reset Lost Fingers: %d", numFingers);
        for (--numFingers; numFingers >= 0; --numFingers) {
            SDL_Finger *finger = SDL_GetTouchFinger(touchID, numFingers);
            /* trackpad touches have no window. If we really wanted one we could
             * use the window that has mouse or keyboard focus.
             * Sending a null window currently also prevents synthetic mouse
             * events from being generated from touch events.
             */
            SDL_Window *window = NULL;
            SDL_SendTouch(Cocoa_GetEventTimestamp([theEvent timestamp]), touchID, finger->id, window, SDL_FALSE, 0, 0, 0);
        }
    }

    DLog("Began Fingers: %lu .. existing: %d", (unsigned long)[touches count], existingTouchCount);
    [self handleTouches:NSTouchPhaseBegan withEvent:theEvent];
}

- (void)touchesMovedWithEvent:(NSEvent *)theEvent
{
    [self handleTouches:NSTouchPhaseMoved withEvent:theEvent];
}

- (void)touchesEndedWithEvent:(NSEvent *)theEvent
{
    [self handleTouches:NSTouchPhaseEnded withEvent:theEvent];
}

- (void)touchesCancelledWithEvent:(NSEvent *)theEvent
{
    [self handleTouches:NSTouchPhaseCancelled withEvent:theEvent];
}

- (void)handleTouches:(NSTouchPhase)phase withEvent:(NSEvent *)theEvent
{
    NSSet *touches = [theEvent touchesMatchingPhase:phase inView:nil];
    const BOOL istrackpad = [self isTouchFromTrackpad:theEvent];
    SDL_FingerID fingerId;
    float x, y;

    for (NSTouch *touch in touches) {
        const SDL_TouchID touchId = istrackpad ? SDL_MOUSE_TOUCHID : (SDL_TouchID)(uintptr_t)[touch device];
        SDL_TouchDeviceType devtype = SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE;

        /* trackpad touches have no window. If we really wanted one we could
         * use the window that has mouse or keyboard focus.
         * Sending a null window currently also prevents synthetic mouse events
         * from being generated from touch events.
         */
        SDL_Window *window = NULL;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101202 /* Added in the 10.12.2 SDK. */
        if ([touch respondsToSelector:@selector(type)]) {
            /* TODO: Before implementing direct touch support here, we need to
             * figure out whether the OS generates mouse events from them on its
             * own. If it does, we should prevent SendTouch from generating
             * synthetic mouse events for these touches itself (while also
             * sending a window.) It will also need to use normalized window-
             * relative coordinates via [touch locationInView:].
             */
            if ([touch type] == NSTouchTypeDirect) {
                continue;
            }
        }
#endif

        if (SDL_AddTouch(touchId, devtype, "") < 0) {
            return;
        }

        fingerId = (SDL_FingerID)(uintptr_t)[touch identity];
        x = [touch normalizedPosition].x;
        y = [touch normalizedPosition].y;
        /* Make the origin the upper left instead of the lower left */
        y = 1.0f - y;

        switch (phase) {
        case NSTouchPhaseBegan:
            SDL_SendTouch(Cocoa_GetEventTimestamp([theEvent timestamp]), touchId, fingerId, window, SDL_TRUE, x, y, 1.0f);
            break;
        case NSTouchPhaseEnded:
        case NSTouchPhaseCancelled:
            SDL_SendTouch(Cocoa_GetEventTimestamp([theEvent timestamp]), touchId, fingerId, window, SDL_FALSE, x, y, 1.0f);
            break;
        case NSTouchPhaseMoved:
            SDL_SendTouchMotion(Cocoa_GetEventTimestamp([theEvent timestamp]), touchId, fingerId, window, x, y, 1.0f);
            break;
        default:
            break;
        }
    }
}

@end

@interface SDLView : NSView
{
    SDL_Window *_sdlWindow;
}

- (void)setSDLWindow:(SDL_Window *)window;

/* The default implementation doesn't pass rightMouseDown to responder chain */
- (void)rightMouseDown:(NSEvent *)theEvent;
- (BOOL)mouseDownCanMoveWindow;
- (void)drawRect:(NSRect)dirtyRect;
- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent;
- (BOOL)wantsUpdateLayer;
- (void)updateLayer;
@end

@implementation SDLView

- (void)setSDLWindow:(SDL_Window *)window
{
    _sdlWindow = window;
}

/* this is used on older macOS revisions, and newer ones which emulate old
   NSOpenGLContext behaviour while still using a layer under the hood. 10.8 and
   later use updateLayer, up until 10.14.2 or so, which uses drawRect without
   a GraphicsContext and with a layer active instead (for OpenGL contexts). */
- (void)drawRect:(NSRect)dirtyRect
{
    /* Force the graphics context to clear to black so we don't get a flash of
       white until the app is ready to draw. In practice on modern macOS, this
       only gets called for window creation and other extraordinary events. */
    BOOL transparent = (_sdlWindow->flags & SDL_WINDOW_TRANSPARENT) != 0;
    if ([NSGraphicsContext currentContext]) {
        NSColor *fillColor = transparent ? [NSColor clearColor] : [NSColor blackColor];
        [fillColor setFill];
        NSRectFill(dirtyRect);
    } else if (self.layer) {
        CFStringRef color = transparent ? kCGColorClear : kCGColorBlack;
        self.layer.backgroundColor = CGColorGetConstantColor(color);
    }

    Cocoa_SendExposedEventIfVisible(_sdlWindow);
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

/* This is also called when a Metal layer is active. */
- (void)updateLayer
{
    /* Force the graphics context to clear to black so we don't get a flash of
       white until the app is ready to draw. In practice on modern macOS, this
       only gets called for window creation and other extraordinary events. */
    BOOL transparent = (_sdlWindow->flags & SDL_WINDOW_TRANSPARENT) != 0;
    CFStringRef color = transparent ? kCGColorClear : kCGColorBlack;
    self.layer.backgroundColor = CGColorGetConstantColor(color);
    ScheduleContextUpdates((__bridge SDL_CocoaWindowData *)_sdlWindow->driverdata);
    Cocoa_SendExposedEventIfVisible(_sdlWindow);
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    [[self nextResponder] rightMouseDown:theEvent];
}

- (BOOL)mouseDownCanMoveWindow
{
    /* Always say YES, but this doesn't do anything until we call
       -[NSWindow setMovableByWindowBackground:YES], which we ninja-toggle
       during mouse events when we're using a drag area. */
    return YES;
}

- (void)resetCursorRects
{
    SDL_Mouse *mouse;
    [super resetCursorRects];
    mouse = SDL_GetMouse();

    if (mouse->cursor_shown && mouse->cur_cursor && !mouse->relative_mode) {
        [self addCursorRect:[self bounds]
                     cursor:(__bridge NSCursor *)mouse->cur_cursor->driverdata];
    } else {
        [self addCursorRect:[self bounds]
                     cursor:[NSCursor invisibleCursor]];
    }
}

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
    if (SDL_GetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH)) {
        return SDL_GetHintBoolean(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, SDL_FALSE);
    } else {
        return SDL_GetHintBoolean("SDL_MAC_MOUSE_FOCUS_CLICKTHROUGH", SDL_FALSE);
    }
}

@end

static int SetupWindowData(SDL_VideoDevice *_this, SDL_Window *window, NSWindow *nswindow, NSView *nsview)
{
    @autoreleasepool {
        SDL_CocoaVideoData *videodata = (__bridge SDL_CocoaVideoData *)_this->driverdata;
        SDL_CocoaWindowData *data;

        /* Allocate the window data */
        data = [[SDL_CocoaWindowData alloc] init];
        if (!data) {
            return SDL_OutOfMemory();
        }
        data.window = window;
        data.nswindow = nswindow;
        data.videodata = videodata;
        data.window_number = nswindow.windowNumber;
        data.nscontexts = [[NSMutableArray alloc] init];
        data.sdlContentView = nsview;

        /* Create an event listener for the window */
        data.listener = [[Cocoa_WindowListener alloc] init];

        /* Fill in the SDL window with the window data */
        {
            int x, y;
            NSRect rect = [nswindow contentRectForFrameRect:[nswindow frame]];
            BOOL fullscreen = (window->flags & SDL_WINDOW_FULLSCREEN) ? YES : NO;
            ConvertNSRect(fullscreen, &rect);
            SDL_GlobalToRelativeForWindow(window, (int)rect.origin.x, (int)rect.origin.y, &x, &y);
            window->x = x;
            window->y = y;
            window->w = (int)rect.size.width;
            window->h = (int)rect.size.height;
        }

        /* Set up the listener after we create the view */
        [data.listener listen:data];

        if ([nswindow isVisible]) {
            window->flags &= ~SDL_WINDOW_HIDDEN;
        } else {
            window->flags |= SDL_WINDOW_HIDDEN;
        }

        {
            unsigned long style = [nswindow styleMask];

            /* NSWindowStyleMaskBorderless is zero, and it's possible to be
                Resizeable _and_ borderless, so we can't do a simple bitwise AND
                of NSWindowStyleMaskBorderless here. */
            if ((style & ~(NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)) == NSWindowStyleMaskBorderless) {
                window->flags |= SDL_WINDOW_BORDERLESS;
            } else {
                window->flags &= ~SDL_WINDOW_BORDERLESS;
            }
            if (style & NSWindowStyleMaskResizable) {
                window->flags |= SDL_WINDOW_RESIZABLE;
            } else {
                window->flags &= ~SDL_WINDOW_RESIZABLE;
            }
        }

        /* isZoomed always returns true if the window is not resizable */
        if ((window->flags & SDL_WINDOW_RESIZABLE) && Cocoa_IsZoomed(window)) {
            window->flags |= SDL_WINDOW_MAXIMIZED;
        } else {
            window->flags &= ~SDL_WINDOW_MAXIMIZED;
        }

        if ([nswindow isMiniaturized]) {
            window->flags |= SDL_WINDOW_MINIMIZED;
        } else {
            window->flags &= ~SDL_WINDOW_MINIMIZED;
        }

        if (!SDL_WINDOW_IS_POPUP(window)) {
            if ([nswindow isKeyWindow]) {
                window->flags |= SDL_WINDOW_INPUT_FOCUS;
                Cocoa_SetKeyboardFocus(data.window);
            }
        } else {
            NSWindow *nsparent = ((__bridge SDL_CocoaWindowData *)window->parent->driverdata).nswindow;
            [nsparent addChildWindow:nswindow ordered:NSWindowAbove];

            if (window->flags & SDL_WINDOW_TOOLTIP) {
                [nswindow setIgnoresMouseEvents:YES];
            } else if (window->flags & SDL_WINDOW_POPUP_MENU) {
                if (window->parent == SDL_GetKeyboardFocus()) {
                    Cocoa_SetKeyboardFocus(window);
                }
            }

            /* FIXME: Should not need to call addChildWindow then orderOut.
               Attaching a hidden child window to a hidden parent window will cause the child window
               to show when the parent does. We therefore shouldn't attach the child window here as we're
               going to do so when the child window is explicitly shown later but skipping the addChildWindow
               entirely causes the child window to not get key focus correctly the first time it's shown. Adding
               then immediately ordering out (removing) the window does work. */
            if (window->flags & SDL_WINDOW_HIDDEN) {
                [nswindow orderOut:nil];
            }
        }

        if (nswindow.isOpaque) {
            window->flags &= ~SDL_WINDOW_TRANSPARENT;
        } else {
            window->flags |= SDL_WINDOW_TRANSPARENT;
        }

        /* SDL_CocoaWindowData will be holding a strong reference to the NSWindow, and
         * it will also call [NSWindow close] in DestroyWindow before releasing the
         * NSWindow, so the extra release provided by releasedWhenClosed isn't
         * necessary. */
        nswindow.releasedWhenClosed = NO;

        /* Prevents the window's "window device" from being destroyed when it is
         * hidden. See http://www.mikeash.com/pyblog/nsopenglcontext-and-one-shot.html
         */
        [nswindow setOneShot:NO];

        if (window->flags & SDL_WINDOW_EXTERNAL) {
            /* Query the title from the existing window */
            NSString *title = [nswindow title];
            if (title) {
                window->title = SDL_strdup([title UTF8String]);
            }
        }

        SDL_PropertiesID props = SDL_GetWindowProperties(window);
        SDL_SetProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, (__bridge void *)data.nswindow);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_COCOA_METAL_VIEW_TAG_NUMBER, SDL_METALVIEW_TAG);

        /* All done! */
        window->driverdata = (SDL_WindowData *)CFBridgingRetain(data);
        return 0;
    }
}

int Cocoa_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    @autoreleasepool {
        SDL_CocoaVideoData *videodata = (__bridge SDL_CocoaVideoData *)_this->driverdata;
        const void *data = SDL_GetProperty(create_props, "sdl2-compat.external_window", NULL);
        NSWindow *nswindow = nil;
        NSView *nsview = nil;

        if (data) {
            if ([(__bridge id)data isKindOfClass:[NSWindow class]]) {
                nswindow = (__bridge NSWindow *)data;
            } else if ([(__bridge id)data isKindOfClass:[NSView class]]) {
                nsview = (__bridge NSView *)data;
            } else {
                SDL_assert(false);
            }
        } else {
            nswindow = (__bridge NSWindow *)SDL_GetProperty(create_props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, NULL);
            nsview = (__bridge NSView *)SDL_GetProperty(create_props, SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER, NULL);
        }
        if (nswindow && !nsview) {
            nsview = [nswindow contentView];
        }
        if (nsview && !nswindow) {
            nswindow = [nsview window];
        }
        if (nswindow) {
            window->flags |= SDL_WINDOW_EXTERNAL;
        } else {
            int x, y;
            NSScreen *screen;
            NSRect rect, screenRect;
            BOOL fullscreen;
            NSUInteger style;
            SDLView *contentView;

            SDL_RelativeToGlobalForWindow(window, window->x, window->y, &x, &y);
            rect.origin.x = x;
            rect.origin.y = y;
            rect.size.width = window->w;
            rect.size.height = window->h;
            fullscreen = (window->flags & SDL_WINDOW_FULLSCREEN) ? YES : NO;
            ConvertNSRect(fullscreen, &rect);

            style = GetWindowStyle(window);

            /* Figure out which screen to place this window */
            screen = ScreenForRect(&rect);
            screenRect = [screen frame];
            rect.origin.x -= screenRect.origin.x;
            rect.origin.y -= screenRect.origin.y;

            /* Constrain the popup */
            if (SDL_WINDOW_IS_POPUP(window)) {
                if (rect.origin.x + rect.size.width > screenRect.origin.x + screenRect.size.width) {
                    rect.origin.x -= (rect.origin.x + rect.size.width) - (screenRect.origin.x + screenRect.size.width);
                }
                if (rect.origin.y + rect.size.height > screenRect.origin.y + screenRect.size.height) {
                    rect.origin.y -= (rect.origin.y + rect.size.height) - (screenRect.origin.y + screenRect.size.height);
                }
                rect.origin.x = SDL_max(rect.origin.x, screenRect.origin.x);
                rect.origin.y = SDL_max(rect.origin.y, screenRect.origin.y);
            }

            @try {
                nswindow = [[SDLWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:NO screen:screen];
            }
            @catch (NSException *e) {
                return SDL_SetError("%s", [[e reason] UTF8String]);
            }

            [nswindow setColorSpace:[NSColorSpace sRGBColorSpace]];

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101200 /* Added in the 10.12.0 SDK. */
            /* By default, don't allow users to make our window tabbed in 10.12 or later */
            if ([nswindow respondsToSelector:@selector(setTabbingMode:)]) {
                [nswindow setTabbingMode:NSWindowTabbingModeDisallowed];
            }
#endif

            if (videodata.allow_spaces) {
                /* we put fullscreen desktop windows in their own Space, without a toggle button or menubar, later */
                if (window->flags & SDL_WINDOW_RESIZABLE) {
                    /* resizable windows are Spaces-friendly: they get the "go fullscreen" toggle button on their titlebar. */
                    [nswindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
                }
            }

            /* Create a default view for this window */
            rect = [nswindow contentRectForFrameRect:[nswindow frame]];
            contentView = [[SDLView alloc] initWithFrame:rect];
            [contentView setSDLWindow:window];
            nsview = contentView;
        }

        if (window->flags & SDL_WINDOW_ALWAYS_ON_TOP) {
            [nswindow setLevel:NSFloatingWindowLevel];
        }

        if (window->flags & SDL_WINDOW_TRANSPARENT) {
            nswindow.opaque = NO;
            nswindow.hasShadow = NO;
            nswindow.backgroundColor = [NSColor clearColor];
        }

/* We still support OpenGL as long as Apple offers it, deprecated or not, so disable deprecation warnings about it. */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        /* Note: as of the macOS 10.15 SDK, this defaults to YES instead of NO when
         * the NSHighResolutionCapable boolean is set in Info.plist. */
        BOOL highdpi = (window->flags & SDL_WINDOW_HIGH_PIXEL_DENSITY) ? YES : NO;
        [nsview setWantsBestResolutionOpenGLSurface:highdpi];
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef SDL_VIDEO_OPENGL_ES2
#ifdef SDL_VIDEO_OPENGL_EGL
        if ((window->flags & SDL_WINDOW_OPENGL) &&
            _this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES) {
            [nsview setWantsLayer:TRUE];
            if ((window->flags & SDL_WINDOW_HIGH_PIXEL_DENSITY) &&
                [nswindow.screen respondsToSelector:@selector(backingScaleFactor)]) {
                nsview.layer.contentsScale = nswindow.screen.backingScaleFactor;
            } else {
                nsview.layer.contentsScale = 1;
            }
        }
#endif /* SDL_VIDEO_OPENGL_EGL */
#endif /* SDL_VIDEO_OPENGL_ES2 */
        [nswindow setContentView:nsview];

        if (SetupWindowData(_this, window, nswindow, nsview) < 0) {
            return -1;
        }

        if (!(window->flags & SDL_WINDOW_OPENGL)) {
            return 0;
        }

        /* The rest of this macro mess is for OpenGL or OpenGL ES windows */
#ifdef SDL_VIDEO_OPENGL_ES2
        if (_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES) {
#ifdef SDL_VIDEO_OPENGL_EGL
            if (Cocoa_GLES_SetupWindow(_this, window) < 0) {
                Cocoa_DestroyWindow(_this, window);
                return -1;
            }
            return 0;
#else
            return SDL_SetError("Could not create GLES window surface (EGL support not configured)");
#endif /* SDL_VIDEO_OPENGL_EGL */
        }
#endif /* SDL_VIDEO_OPENGL_ES2 */
        return 0;
    }
}

void Cocoa_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        const char *title = window->title ? window->title : "";
        NSWindow *nswindow = ((__bridge SDL_CocoaWindowData *)window->driverdata).nswindow;
        NSString *string = [[NSString alloc] initWithUTF8String:title];
        [nswindow setTitle:string];
    }
}

int Cocoa_SetWindowIcon(SDL_VideoDevice *_this, SDL_Window *window, SDL_Surface *icon)
{
    @autoreleasepool {
        NSImage *nsimage = Cocoa_CreateImage(icon);

        if (nsimage) {
            [NSApp setApplicationIconImage:nsimage];

            return 0;
        }

        return SDL_SetError("Unable to set the window's icon");
    }
}

int Cocoa_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *windata = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSWindow *nswindow = windata.nswindow;
        NSRect rect = [nswindow contentRectForFrameRect:[nswindow frame]];
        BOOL fullscreen = (window->flags & SDL_WINDOW_FULLSCREEN) ? YES : NO;
        int x, y;

        if ([windata.listener windowOperationIsPending:(PENDING_OPERATION_ENTER_FULLSCREEN | PENDING_OPERATION_LEAVE_FULLSCREEN)] ||
            [windata.listener isInFullscreenSpaceTransition]) {
            Cocoa_SyncWindow(_this, window);
        }

        if (!(window->flags & SDL_WINDOW_MAXIMIZED)) {
            if (fullscreen) {
                SDL_VideoDisplay *display = SDL_GetVideoDisplayForFullscreenWindow(window);
                SDL_Rect r;
                SDL_GetDisplayBounds(display->id, &r);

                rect.origin.x = r.x;
                rect.origin.y = r.y;
            } else {
                SDL_RelativeToGlobalForWindow(window, window->floating.x, window->floating.y, &x, &y);
                rect.origin.x = x;
                rect.origin.y = y;
            }
            ConvertNSRect(fullscreen, &rect);

            /* Position and constrain the popup */
            if (SDL_WINDOW_IS_POPUP(window)) {
                NSRect screenRect = [ScreenForRect(&rect) frame];

                if (rect.origin.x + rect.size.width > screenRect.origin.x + screenRect.size.width) {
                    rect.origin.x -= (rect.origin.x + rect.size.width) - (screenRect.origin.x + screenRect.size.width);
                }
                if (rect.origin.y + rect.size.height > screenRect.origin.y + screenRect.size.height) {
                    rect.origin.y -= (rect.origin.y + rect.size.height) - (screenRect.origin.y + screenRect.size.height);
                }
                rect.origin.x = SDL_max(rect.origin.x, screenRect.origin.x);
                rect.origin.y = SDL_max(rect.origin.y, screenRect.origin.y);
            }

            [nswindow setFrameOrigin:rect.origin];

            ScheduleContextUpdates(windata);
        } else {
            windata.send_floating_position = SDL_TRUE;
        }
    }
    return 0;
}

void Cocoa_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *windata = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSWindow *nswindow = windata.nswindow;
        NSRect rect = [nswindow contentRectForFrameRect:[nswindow frame]];

        rect.size.width = window->floating.w;
        rect.size.height = window->floating.h;

        if ([windata.listener windowOperationIsPending:(PENDING_OPERATION_ENTER_FULLSCREEN | PENDING_OPERATION_LEAVE_FULLSCREEN)] ||
            [windata.listener isInFullscreenSpaceTransition]) {
            Cocoa_SyncWindow(_this, window);
        }

        /* isZoomed always returns true if the window is not resizable */
        if (!(window->flags & SDL_WINDOW_RESIZABLE) || !Cocoa_IsZoomed(window)) {
            if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
                [nswindow setFrame:[nswindow frameRectForContentRect:rect] display:YES];
                ScheduleContextUpdates(windata);
            } else if (windata.was_zoomed) {
                windata.send_floating_size = SDL_TRUE;
            }
        } else {
            windata.send_floating_size = SDL_TRUE;
        }
    }
}

void Cocoa_SetWindowMinimumSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *windata = (__bridge SDL_CocoaWindowData *)window->driverdata;

        NSSize minSize;
        minSize.width = window->min_w;
        minSize.height = window->min_h;

        [windata.nswindow setContentMinSize:minSize];
    }
}

void Cocoa_SetWindowMaximumSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *windata = (__bridge SDL_CocoaWindowData *)window->driverdata;

        NSSize maxSize;
        maxSize.width = window->max_w;
        maxSize.height = window->max_h;

        [windata.nswindow setContentMaxSize:maxSize];
    }
}

void Cocoa_GetWindowSizeInPixels(SDL_VideoDevice *_this, SDL_Window *window, int *w, int *h)
{
    @autoreleasepool {
        SDL_CocoaWindowData *windata = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSView *contentView = windata.sdlContentView;
        NSRect viewport = [contentView bounds];

        if (window->flags & SDL_WINDOW_HIGH_PIXEL_DENSITY) {
            /* This gives us the correct viewport for a Retina-enabled view. */
            viewport = [contentView convertRectToBacking:viewport];
        }

        *w = viewport.size.width;
        *h = viewport.size.height;
    }
}

void Cocoa_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *windowData = ((__bridge SDL_CocoaWindowData *)window->driverdata);
        NSWindow *nswindow = windowData.nswindow;
        SDL_bool bActivate = SDL_GetHintBoolean(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, SDL_TRUE);

        if (![nswindow isMiniaturized]) {
            [windowData.listener pauseVisibleObservation];
            if (SDL_WINDOW_IS_POPUP(window)) {
                NSWindow *nsparent = ((__bridge SDL_CocoaWindowData *)window->parent->driverdata).nswindow;
                [nsparent addChildWindow:nswindow ordered:NSWindowAbove];
            } else {
                if (bActivate) {
                    [nswindow makeKeyAndOrderFront:nil];
                } else {
                    /* Order this window below the key window if we're not activating it */
                    if ([NSApp keyWindow]) {
                        [nswindow orderWindow:NSWindowBelow relativeTo:[[NSApp keyWindow] windowNumber]];
                    }
                }
            }
            [nswindow setIsVisible:YES];
            [windowData.listener resumeVisibleObservation];
        }
    }
}

void Cocoa_HideWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        NSWindow *nswindow = ((__bridge SDL_CocoaWindowData *)window->driverdata).nswindow;

        /* orderOut has no effect on miniaturized windows, so close must be used to remove
         * the window from the desktop and window list in this case.
         *
         * SDL holds a strong reference to the window (oneShot/releasedWhenClosed are 'NO'),
         * and calling 'close' doesn't send a 'windowShouldClose' message, so it's safe to
         * use for this purpose as nothing is implicitly released.
         */
        if (![nswindow isMiniaturized]) {
            [nswindow orderOut:nil];
        } else {
            [nswindow close];
        }

        /* Transfer keyboard focus back to the parent */
        if (window->flags & SDL_WINDOW_POPUP_MENU) {
            if (window == SDL_GetKeyboardFocus()) {
                SDL_Window *new_focus = window->parent;

                /* Find the highest level window that isn't being hidden or destroyed. */
                while (new_focus->parent != NULL && (new_focus->is_hiding || new_focus->is_destroying)) {
                    new_focus = new_focus->parent;
                }

                Cocoa_SetKeyboardFocus(new_focus);
            }
        }
    }
}

void Cocoa_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *windowData = ((__bridge SDL_CocoaWindowData *)window->driverdata);
        NSWindow *nswindow = windowData.nswindow;
        SDL_bool bActivate = SDL_GetHintBoolean(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, SDL_TRUE);

        /* makeKeyAndOrderFront: has the side-effect of deminiaturizing and showing
         a minimized or hidden window, so check for that before showing it.
         */
        [windowData.listener pauseVisibleObservation];
        if (![nswindow isMiniaturized] && [nswindow isVisible]) {
            if (SDL_WINDOW_IS_POPUP(window)) {
                NSWindow *nsparent = ((__bridge SDL_CocoaWindowData *)window->parent->driverdata).nswindow;
                [nsparent addChildWindow:nswindow ordered:NSWindowAbove];
                if (bActivate) {
                    [nswindow makeKeyWindow];
                }
            } else {
                if (bActivate) {
                    [NSApp activateIgnoringOtherApps:YES];
                    [nswindow makeKeyAndOrderFront:nil];
                } else {
                    [nswindow orderFront:nil];
                }
            }
        }
        [windowData.listener resumeVisibleObservation];
    }
}

void Cocoa_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *windata = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSWindow *nswindow = windata.nswindow;

        if ([windata.listener windowOperationIsPending:(PENDING_OPERATION_ENTER_FULLSCREEN | PENDING_OPERATION_LEAVE_FULLSCREEN)] ||
            [windata.listener isInFullscreenSpaceTransition]) {
            Cocoa_SyncWindow(_this, window);
        }

        if (!(window->flags & SDL_WINDOW_FULLSCREEN) &&
            ![windata.listener isInFullscreenSpaceTransition] &&
            ![windata.listener isInFullscreenSpace]) {
            [nswindow zoom:nil];
            ScheduleContextUpdates(windata);
        }
    }
}

void Cocoa_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSWindow *nswindow = data.nswindow;

        [data.listener addPendingWindowOperation:PENDING_OPERATION_MINIMIZE];
        if ([data.listener isInFullscreenSpace] || (window->flags & SDL_WINDOW_FULLSCREEN)) {
            [data.listener addPendingWindowOperation:PENDING_OPERATION_LEAVE_FULLSCREEN];
            SDL_UpdateFullscreenMode(window, SDL_FALSE, SDL_TRUE);
        } else if ([data.listener isInFullscreenSpaceTransition]) {
            [data.listener addPendingWindowOperation:PENDING_OPERATION_LEAVE_FULLSCREEN];
        } else {
            [nswindow miniaturize:nil];
        }
    }
}

void Cocoa_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSWindow *nswindow = data.nswindow;

        if ([data.listener windowOperationIsPending:(PENDING_OPERATION_ENTER_FULLSCREEN | PENDING_OPERATION_LEAVE_FULLSCREEN)] ||
            [data.listener isInFullscreenSpaceTransition]) {
            Cocoa_SyncWindow(_this, window);
        }

        [data.listener clearPendingWindowOperation:(PENDING_OPERATION_MINIMIZE)];

        if (!(window->flags & SDL_WINDOW_FULLSCREEN) &&
            ![data.listener isInFullscreenSpaceTransition] &&
            ![data.listener isInFullscreenSpace]) {
            if ([nswindow isMiniaturized]) {
                [nswindow deminiaturize:nil];
            } else if ((window->flags & SDL_WINDOW_RESIZABLE) && Cocoa_IsZoomed(window)) {
                NSRect rect;

                /* Update the floating coordinates */
                rect.origin.x = window->floating.x;
                rect.origin.y = window->floating.y;

                /* The floating size will be set in windowWillResize */
                [nswindow zoom:nil];

                rect.size.width = window->floating.w;
                rect.size.height = window->floating.h;

                ConvertNSRect(SDL_FALSE, &rect);

                if (data.send_floating_position) {
                    data.send_floating_position = SDL_FALSE;
                    [nswindow setFrameOrigin:rect.origin];
                    ScheduleContextUpdates(data);
                }
            }
        }
    }
}

void Cocoa_SetWindowBordered(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool bordered)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

        /* If the window is in or transitioning to/from fullscreen, this will be set on leave. */
        if (!(window->flags & SDL_WINDOW_FULLSCREEN) && ![data.listener isInFullscreenSpaceTransition]) {
            if (SetWindowStyle(window, GetWindowStyle(window))) {
                if (bordered) {
                    Cocoa_SetWindowTitle(_this, window); /* this got blanked out. */
                }
            }
        } else {
            data.border_toggled = SDL_TRUE;
        }
    }
}

void Cocoa_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool resizable)
{
    @autoreleasepool {
        /* Don't set this if we're in or transitioning to/from a space!
         * The window will get permanently stuck if resizable is false.
         * -flibit
         */
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        Cocoa_WindowListener *listener = data.listener;
        NSWindow *nswindow = data.nswindow;
        SDL_CocoaVideoData *videodata = data.videodata;
        if (![listener isInFullscreenSpace] && ![listener isInFullscreenSpaceTransition]) {
            SetWindowStyle(window, GetWindowStyle(window));
        }
        if (videodata.allow_spaces) {
            if (resizable) {
                /* resizable windows are Spaces-friendly: they get the "go fullscreen" toggle button on their titlebar. */
                [nswindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
            } else {
                [nswindow setCollectionBehavior:NSWindowCollectionBehaviorManaged];
            }
        }
    }
}

void Cocoa_SetWindowAlwaysOnTop(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool on_top)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSWindow *nswindow = data.nswindow;

        /* If the window is in or transitioning to/from fullscreen, this will be set on leave. */
        if (!(window->flags & SDL_WINDOW_FULLSCREEN) && ![data.listener isInFullscreenSpaceTransition]) {
            if (on_top) {
                [nswindow setLevel:NSFloatingWindowLevel];
            } else {
                [nswindow setLevel:kCGNormalWindowLevel];
            }
        }
    }
}

int Cocoa_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSWindow *nswindow = data.nswindow;
        NSRect rect;

        /* The view responder chain gets messed with during setStyleMask */
        if ([data.sdlContentView nextResponder] == data.listener) {
            [data.sdlContentView setNextResponder:nil];
        }

        if (fullscreen) {
            SDL_Rect bounds;

            if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
                data.was_zoomed = !!(window->flags & SDL_WINDOW_MAXIMIZED);
            }

            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_ENTER_FULLSCREEN, 0, 0);
            Cocoa_GetDisplayBounds(_this, display, &bounds);
            rect.origin.x = bounds.x;
            rect.origin.y = bounds.y;
            rect.size.width = bounds.w;
            rect.size.height = bounds.h;
            ConvertNSRect(fullscreen, &rect);

            /* Hack to fix origin on macOS 10.4
               This is no longer needed as of macOS 10.15, according to bug 4822.
             */
            if (SDL_floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_14) {
                NSRect screenRect = [[nswindow screen] frame];
                if (screenRect.size.height >= 1.0f) {
                    rect.origin.y += (screenRect.size.height - rect.size.height);
                }
            }

            [nswindow setStyleMask:NSWindowStyleMaskBorderless];
        } else {
            NSRect frameRect;

            SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_LEAVE_FULLSCREEN, 0, 0);

            rect.origin.x = data.was_zoomed ? window->windowed.x : window->floating.x;
            rect.origin.y = data.was_zoomed ? window->windowed.y : window->floating.y;
            rect.size.width = data.was_zoomed ? window->windowed.w : window->floating.w;
            rect.size.height = data.was_zoomed ? window->windowed.h : window->floating.h;

            ConvertNSRect(fullscreen, &rect);

            /* The window is not meant to be fullscreen, but its flags might have a
             * fullscreen bit set if it's scheduled to go fullscreen immediately
             * after. Always using the windowed mode style here works around bugs in
             * macOS 10.15 where the window doesn't properly restore the windowed
             * mode decorations after exiting fullscreen-desktop, when the window
             * was created as fullscreen-desktop. */
            [nswindow setStyleMask:GetWindowWindowedStyle(window)];

            /* Hack to restore window decorations on macOS 10.10 */
            frameRect = [nswindow frame];
            [nswindow setFrame:NSMakeRect(frameRect.origin.x, frameRect.origin.y, frameRect.size.width + 1, frameRect.size.height) display:NO];
            [nswindow setFrame:frameRect display:NO];
        }

        /* The view responder chain gets messed with during setStyleMask */
        if ([data.sdlContentView nextResponder] != data.listener) {
            [data.sdlContentView setNextResponder:data.listener];
        }

        [nswindow setContentSize:rect.size];
        [nswindow setFrameOrigin:rect.origin];

        /* When the window style changes the title is cleared */
        if (!fullscreen) {
            Cocoa_SetWindowTitle(_this, window);

            data.was_zoomed = NO;

            if ([data.listener windowOperationIsPending:PENDING_OPERATION_MINIMIZE]) {
                Cocoa_WaitForMiniaturizable(window);
                [data.listener addPendingWindowOperation:PENDING_OPERATION_ENTER_FULLSCREEN];
                [nswindow miniaturize:nil];
            }
        }

        if (SDL_ShouldAllowTopmost() && fullscreen) {
            /* OpenGL is rendering to the window, so make it visible! */
            [nswindow setLevel:kCGMainMenuWindowLevel + 1];
        } else if (window->flags & SDL_WINDOW_ALWAYS_ON_TOP) {
            [nswindow setLevel:NSFloatingWindowLevel];
        } else {
            [nswindow setLevel:kCGNormalWindowLevel];
        }

        if ([nswindow isVisible] || fullscreen) {
            [data.listener pauseVisibleObservation];
            [nswindow makeKeyAndOrderFront:nil];
            [data.listener resumeVisibleObservation];
        }

        ScheduleContextUpdates(data);
    }

    return 0;
}

void *Cocoa_GetWindowICCProfile(SDL_VideoDevice *_this, SDL_Window *window, size_t *size)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        NSWindow *nswindow = data.nswindow;
        NSScreen *screen = [nswindow screen];
        NSData *iccProfileData = nil;
        void *retIccProfileData = NULL;

        if (screen == nil) {
            SDL_SetError("Could not get screen of window.");
            return NULL;
        }

        if ([screen colorSpace] == nil) {
            SDL_SetError("Could not get colorspace information of screen.");
            return NULL;
        }

        iccProfileData = [[screen colorSpace] ICCProfileData];
        if (iccProfileData == nil) {
            SDL_SetError("Could not get ICC profile data.");
            return NULL;
        }

        retIccProfileData = SDL_malloc([iccProfileData length]);
        if (!retIccProfileData) {
            return NULL;
        }

        [iccProfileData getBytes:retIccProfileData length:[iccProfileData length]];
        *size = [iccProfileData length];
        return retIccProfileData;
    }
}

SDL_DisplayID Cocoa_GetDisplayForWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        NSScreen *screen;
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

        /* Not recognized via CHECK_WINDOW_MAGIC */
        if (data == nil) {
            /* Don't set the error here, it hides other errors and is ignored anyway */
            /*return SDL_SetError("Window data not set");*/
            return 0;
        }

        /* NSWindow.screen may be nil when the window is off-screen. */
        screen = data.nswindow.screen;

        if (screen != nil) {
            CGDirectDisplayID displayid;
            int i;

            /* https://developer.apple.com/documentation/appkit/nsscreen/1388360-devicedescription?language=objc */
            displayid = [[screen.deviceDescription objectForKey:@"NSScreenNumber"] unsignedIntValue];

            for (i = 0; i < _this->num_displays; i++) {
                SDL_DisplayData *displaydata = _this->displays[i]->driverdata;
                if (displaydata != NULL && displaydata->display == displayid) {
                    return _this->displays[i]->id;
                }
            }
        }

        /* The higher level code will use other logic to find the display */
        return 0;
    }
}

void Cocoa_SetWindowMouseRect(SDL_VideoDevice *_this, SDL_Window *window)
{
    Cocoa_UpdateClipCursor(window);
}

void Cocoa_SetWindowMouseGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

        Cocoa_UpdateClipCursor(window);

        if (data && (window->flags & SDL_WINDOW_FULLSCREEN) != 0) {
            if (SDL_ShouldAllowTopmost() && (window->flags & SDL_WINDOW_INPUT_FOCUS) && ![data.listener isInFullscreenSpace]) {
                /* OpenGL is rendering to the window, so make it visible! */
                /* Doing this in 10.11 while in a Space breaks things (bug #3152) */
                [data.nswindow setLevel:kCGMainMenuWindowLevel + 1];
            } else if (window->flags & SDL_WINDOW_ALWAYS_ON_TOP) {
                [data.nswindow setLevel:NSFloatingWindowLevel];
            } else {
                [data.nswindow setLevel:kCGNormalWindowLevel];
            }
        }
    }
}

void Cocoa_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (SDL_CocoaWindowData *)CFBridgingRelease(window->driverdata);

        if (data) {
#ifdef SDL_VIDEO_OPENGL

            NSArray *contexts;

#endif /* SDL_VIDEO_OPENGL */
            SDL_Window *topmost = GetTopmostWindow(window);
            SDL_CocoaWindowData *topmost_data = (__bridge SDL_CocoaWindowData *)topmost->driverdata;

            /* Reset the input focus of the root window if this window is still set as keyboard focus.
             * SDL_DestroyWindow will have already taken care of reassigning focus if this is the SDL
             * keyboard focus, this ensures that an inactive window with this window set as input focus
             * does not try to reference it the next time it gains focus.
             */
            if (topmost_data.keyboard_focus == window) {
                SDL_Window *new_focus = window;
                while(new_focus->parent && (new_focus->is_hiding || new_focus->is_destroying)) {
                    new_focus = new_focus->parent;
                }

                topmost_data.keyboard_focus = new_focus;
            }

            if ([data.listener isInFullscreenSpace]) {
                [NSMenu setMenuBarVisible:YES];
            }
            [data.listener close];
            data.listener = nil;
            if (data.created) {
                /* Release the content view to avoid further updateLayer callbacks */
                [data.nswindow setContentView:nil];
                [data.nswindow close];
            }

#ifdef SDL_VIDEO_OPENGL

            contexts = [data.nscontexts copy];
            for (SDLOpenGLContext *context in contexts) {
                /* Calling setWindow:NULL causes the context to remove itself from the context list. */
                [context setWindow:NULL];
            }

#endif /* SDL_VIDEO_OPENGL */
        }
        window->driverdata = NULL;
    }
}

SDL_bool Cocoa_IsWindowInFullscreenSpace(SDL_Window *window)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

        if ([data.listener isInFullscreenSpace]) {
            return SDL_TRUE;
        } else {
            return SDL_FALSE;
        }
    }
}

SDL_bool Cocoa_SetWindowFullscreenSpace(SDL_Window *window, SDL_bool state, SDL_bool blocking)
{
    @autoreleasepool {
        SDL_bool succeeded = SDL_FALSE;
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

        data.in_blocking_transition = blocking;
        if ([data.listener setFullscreenSpace:(state ? YES : NO)]) {
            if (blocking) {
                const int maxattempts = 3;
                int attempt = 0;
                while (++attempt <= maxattempts) {
                    /* Wait for the transition to complete, so application changes
                     take effect properly (e.g. setting the window size, etc.)
                     */
                    const int limit = 10000;
                    int count = 0;
                    while ([data.listener isInFullscreenSpaceTransition]) {
                        if (++count == limit) {
                            /* Uh oh, transition isn't completing. Should we assert? */
                            break;
                        }
                        SDL_Delay(1);
                        SDL_PumpEvents();
                    }
                    if ([data.listener isInFullscreenSpace] == (state ? YES : NO)) {
                        break;
                    }
                    /* Try again, the last attempt was interrupted by user gestures */
                    if (![data.listener setFullscreenSpace:(state ? YES : NO)]) {
                        break; /* ??? */
                    }
                }
            }

            /* Return TRUE to prevent non-space fullscreen logic from running */
            succeeded = SDL_TRUE;
        }

        data.in_blocking_transition = NO;
        return succeeded;
    }
}

int Cocoa_SetWindowHitTest(SDL_Window *window, SDL_bool enabled)
{
    return 0; /* just succeed, the real work is done elsewhere. */
}

void Cocoa_AcceptDragAndDrop(SDL_Window *window, SDL_bool accept)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        if (accept) {
            [data.nswindow registerForDraggedTypes:[NSArray arrayWithObject:(NSString *)kUTTypeFileURL]];
        } else {
            [data.nswindow unregisterDraggedTypes];
        }
    }
}

int Cocoa_FlashWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_FlashOperation operation)
{
    @autoreleasepool {
        /* Note that this is app-wide and not window-specific! */
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;

        if (data.flash_request) {
            [NSApp cancelUserAttentionRequest:data.flash_request];
            data.flash_request = 0;
        }

        switch (operation) {
        case SDL_FLASH_CANCEL:
            /* Canceled above */
            break;
        case SDL_FLASH_BRIEFLY:
            data.flash_request = [NSApp requestUserAttention:NSInformationalRequest];
            break;
        case SDL_FLASH_UNTIL_FOCUSED:
            data.flash_request = [NSApp requestUserAttention:NSCriticalRequest];
            break;
        default:
            return SDL_Unsupported();
        }
        return 0;
    }
}

int Cocoa_SetWindowFocusable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool focusable)
{
    return 0; /* just succeed, the real work is done elsewhere. */
}

int Cocoa_SetWindowOpacity(SDL_VideoDevice *_this, SDL_Window *window, float opacity)
{
    @autoreleasepool {
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        [data.nswindow setAlphaValue:opacity];
        return 0;
    }
}

int Cocoa_SyncWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    int ret = 0;

    @autoreleasepool {
        /* The timeout needs to be high enough that animated fullscreen
         * spaces transitions won't cause it to time out.
         */
        Uint64 timeout = SDL_GetTicksNS() + SDL_MS_TO_NS(2000);
        SDL_CocoaWindowData *data = (__bridge SDL_CocoaWindowData *)window->driverdata;
        while (SDL_TRUE) {
            SDL_PumpEvents();

            if (SDL_GetTicksNS() >= timeout) {
                ret = 1;
                break;
            }
            if (![data.listener hasPendingWindowOperation]) {
                break;
            }

            SDL_Delay(10);
        }
    }

    return ret;
}

#endif /* SDL_VIDEO_DRIVER_COCOA */
