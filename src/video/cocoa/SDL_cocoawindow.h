/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#ifndef _SDL_cocoawindow_h
#define _SDL_cocoawindow_h

#import <Cocoa/Cocoa.h>

typedef struct SDL_WindowData SDL_WindowData;

@interface Cocoa_WindowListener : NSResponder <NSWindowDelegate> {
    SDL_WindowData *_data;
    BOOL observingVisible;
    BOOL wasCtrlLeft;
    BOOL wasVisible;
}

-(void) listen:(SDL_WindowData *) data;
-(void) pauseVisibleObservation;
-(void) resumeVisibleObservation;
-(void) close;

/* Window delegate functionality */
-(BOOL) windowShouldClose:(id) sender;
-(void) windowDidExpose:(NSNotification *) aNotification;
-(void) windowDidMove:(NSNotification *) aNotification;
-(void) windowDidResize:(NSNotification *) aNotification;
-(void) windowDidMiniaturize:(NSNotification *) aNotification;
-(void) windowDidDeminiaturize:(NSNotification *) aNotification;
-(void) windowDidBecomeKey:(NSNotification *) aNotification;
-(void) windowDidResignKey:(NSNotification *) aNotification;

/* Window event handling */
-(void) mouseDown:(NSEvent *) theEvent;
-(void) rightMouseDown:(NSEvent *) theEvent;
-(void) otherMouseDown:(NSEvent *) theEvent;
-(void) mouseUp:(NSEvent *) theEvent;
-(void) rightMouseUp:(NSEvent *) theEvent;
-(void) otherMouseUp:(NSEvent *) theEvent;
-(void) mouseMoved:(NSEvent *) theEvent;
-(void) mouseDragged:(NSEvent *) theEvent;
-(void) rightMouseDragged:(NSEvent *) theEvent;
-(void) otherMouseDragged:(NSEvent *) theEvent;
-(void) scrollWheel:(NSEvent *) theEvent;
-(void) touchesBeganWithEvent:(NSEvent *) theEvent;
-(void) touchesMovedWithEvent:(NSEvent *) theEvent;
-(void) touchesEndedWithEvent:(NSEvent *) theEvent;
-(void) touchesCancelledWithEvent:(NSEvent *) theEvent;

/* Touch event handling */
typedef enum {
    COCOA_TOUCH_DOWN,
    COCOA_TOUCH_UP,
    COCOA_TOUCH_MOVE,
    COCOA_TOUCH_CANCELLED
} cocoaTouchType;
-(void) handleTouches:(cocoaTouchType)type withEvent:(NSEvent*) event;

@end
/* *INDENT-ON* */

@class SDLOpenGLContext;

struct SDL_WindowData
{
    SDL_Window *window;
    NSWindow *nswindow;
    NSMutableArray *nscontexts;
    SDL_bool created;
    Cocoa_WindowListener *listener;
    struct SDL_VideoData *videodata;
};

extern int Cocoa_CreateWindow(_THIS, SDL_Window * window);
extern int Cocoa_CreateWindowFrom(_THIS, SDL_Window * window,
                                  const void *data);
extern void Cocoa_SetWindowTitle(_THIS, SDL_Window * window);
extern void Cocoa_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon);
extern void Cocoa_SetWindowPosition(_THIS, SDL_Window * window);
extern void Cocoa_SetWindowSize(_THIS, SDL_Window * window);
extern void Cocoa_SetWindowMinimumSize(_THIS, SDL_Window * window);
extern void Cocoa_SetWindowMaximumSize(_THIS, SDL_Window * window);
extern void Cocoa_ShowWindow(_THIS, SDL_Window * window);
extern void Cocoa_HideWindow(_THIS, SDL_Window * window);
extern void Cocoa_RaiseWindow(_THIS, SDL_Window * window);
extern void Cocoa_MaximizeWindow(_THIS, SDL_Window * window);
extern void Cocoa_MinimizeWindow(_THIS, SDL_Window * window);
extern void Cocoa_RestoreWindow(_THIS, SDL_Window * window);
extern void Cocoa_SetWindowBordered(_THIS, SDL_Window * window, SDL_bool bordered);
extern void Cocoa_SetWindowFullscreen(_THIS, SDL_Window * window, SDL_VideoDisplay * display, SDL_bool fullscreen);
extern int Cocoa_SetWindowGammaRamp(_THIS, SDL_Window * window, const Uint16 * ramp);
extern int Cocoa_GetWindowGammaRamp(_THIS, SDL_Window * window, Uint16 * ramp);
extern void Cocoa_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed);
extern void Cocoa_DestroyWindow(_THIS, SDL_Window * window);
extern SDL_bool Cocoa_GetWindowWMInfo(_THIS, SDL_Window * window,
                                      struct SDL_SysWMinfo *info);

#endif /* _SDL_cocoawindow_h */

/* vi: set ts=4 sw=4 expandtab: */
