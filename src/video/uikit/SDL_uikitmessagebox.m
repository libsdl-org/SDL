/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

@interface SDLAlertViewDelegate : NSObject <UIAlertViewDelegate>

@property (nonatomic, assign) int clickedIndex;

@end

@implementation SDLAlertViewDelegate

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    _clickedIndex = (int)buttonIndex;
}

@end


SDL_bool
UIKit_ShowingMessageBox()
{
    return s_showingMessageBox;
}

int
UIKit_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    int i;
    const SDL_MessageBoxButtonData *buttons = messageboxdata->buttons;

    @autoreleasepool {
        UIAlertView *alert = [[UIAlertView alloc] init];
        SDLAlertViewDelegate *delegate = [[SDLAlertViewDelegate alloc] init];

        alert.delegate = delegate;
        alert.title = @(messageboxdata->title);
        alert.message = @(messageboxdata->message);

        for (i = 0; i < messageboxdata->numbuttons; ++i) {
            [alert addButtonWithTitle:@(buttons[i].text)];
        }

        /* Set up for showing the alert */
        delegate.clickedIndex = messageboxdata->numbuttons;

        [alert show];

        /* Run the main event loop until the alert has finished */
        /* Note that this needs to be done on the main thread */
        s_showingMessageBox = SDL_TRUE;
        while (delegate.clickedIndex == messageboxdata->numbuttons) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
        }
        s_showingMessageBox = SDL_FALSE;

        *buttonid = messageboxdata->buttons[delegate.clickedIndex].buttonid;

        alert.delegate = nil;
    }

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
