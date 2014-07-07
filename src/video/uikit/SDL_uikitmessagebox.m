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

#if SDL_VIDEO_DRIVER_UIKIT

#include "SDL.h"
#include "SDL_uikitvideo.h"


/* Display a UIKit message box */

static SDL_bool s_showingMessageBox = SDL_FALSE;

@interface UIKit_UIAlertViewDelegate : NSObject <UIAlertViewDelegate> {
@private
    int *clickedButtonIndex;
}

- (id)initWithButtonIndex:(int *)_buttonIndex;
- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex;

@end

@implementation UIKit_UIAlertViewDelegate

- (id)initWithButtonIndex:(int *)buttonIndex
{
    self = [self init];
    if (self == nil) {
        return nil;
    }
    self->clickedButtonIndex = buttonIndex;

    return self;
}

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex;
{
    *clickedButtonIndex = (int)buttonIndex;
}

@end /* UIKit_UIAlertViewDelegate */


SDL_bool
UIKit_ShowingMessageBox()
{
    return s_showingMessageBox;
}

int
UIKit_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    int clicked;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    UIAlertView* alert = [[UIAlertView alloc] init];

    alert.title = [NSString stringWithUTF8String:messageboxdata->title];
    alert.message = [NSString stringWithUTF8String:messageboxdata->message];
    alert.delegate = [[UIKit_UIAlertViewDelegate alloc] initWithButtonIndex:&clicked];

    const SDL_MessageBoxButtonData *buttons = messageboxdata->buttons;
    int i;
    for (i = 0; i < messageboxdata->numbuttons; ++i) {
        [alert addButtonWithTitle:[[NSString alloc] initWithUTF8String:buttons[i].text]];
    }

    /* Set up for showing the alert */
    clicked = messageboxdata->numbuttons;

    [alert show];

    /* Run the main event loop until the alert has finished */
    /* Note that this needs to be done on the main thread */
    s_showingMessageBox = SDL_TRUE;
    while (clicked == messageboxdata->numbuttons) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
    }
    s_showingMessageBox = SDL_FALSE;

    *buttonid = messageboxdata->buttons[clicked].buttonid;

    [alert.delegate release];
    [alert release];

    [pool release];

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
