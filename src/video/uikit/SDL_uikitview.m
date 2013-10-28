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

#if SDL_VIDEO_DRIVER_UIKIT

#include "SDL_uikitview.h"

#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"

#if SDL_IPHONE_KEYBOARD
#include "keyinfotable.h"
#endif
#include "SDL_uikitappdelegate.h"
#include "SDL_uikitmodes.h"
#include "SDL_uikitwindow.h"

@implementation SDL_uikitview

- (void)dealloc
{
    [super dealloc];
}

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame: frame];

#if SDL_IPHONE_KEYBOARD
    [self initializeKeyboard];
#endif

    self.multipleTouchEnabled = YES;

    touchId = 1;
    SDL_AddTouch(touchId, "");

    return self;

}

- (CGPoint)touchLocation:(UITouch *)touch shouldNormalize:(BOOL)normalize
{
    CGPoint point = [touch locationInView: self];

    /* Get the display scale and apply that to the input coordinates */
    SDL_Window *window = self->viewcontroller.window;
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayModeData *displaymodedata = (SDL_DisplayModeData *) display->current_mode.driverdata;

    if (normalize) {
        CGRect bounds = [self bounds];
        point.x /= bounds.size.width;
        point.y /= bounds.size.height;
    } else {
        point.x *= displaymodedata->scale;
        point.y *= displaymodedata->scale;
    }
    return point;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    NSEnumerator *enumerator = [touches objectEnumerator];
    UITouch *touch = (UITouch*)[enumerator nextObject];

    while (touch) {
        if (!leftFingerDown) {
            CGPoint locationInView = [self touchLocation:touch shouldNormalize:NO];

            /* send moved event */
            SDL_SendMouseMotion(NULL, SDL_TOUCH_MOUSEID, 0, locationInView.x, locationInView.y);

            /* send mouse down event */
            SDL_SendMouseButton(NULL, SDL_TOUCH_MOUSEID, SDL_PRESSED, SDL_BUTTON_LEFT);

            leftFingerDown = touch;
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
#ifdef IPHONE_TOUCH_EFFICIENT_DANGEROUS
        /* FIXME: TODO: Using touch as the fingerId is potentially dangerous
         * It is also much more efficient than storing the UITouch pointer
         * and comparing it to the incoming event.
         */
        SDL_SendTouch(touchId, (SDL_FingerID)((size_t)touch),
                      SDL_TRUE, locationInView.x, locationInView.y, 1.0f);
#else
        int i;
        for(i = 0; i < MAX_SIMULTANEOUS_TOUCHES; i++) {
            if (finger[i] == NULL) {
                finger[i] = touch;
                SDL_SendTouch(touchId, i,
                              SDL_TRUE, locationInView.x, locationInView.y, 1.0f);
                break;
            }
        }
#endif
        touch = (UITouch*)[enumerator nextObject];
    }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    NSEnumerator *enumerator = [touches objectEnumerator];
    UITouch *touch = (UITouch*)[enumerator nextObject];

    while(touch) {
        if (touch == leftFingerDown) {
            /* send mouse up */
            SDL_SendMouseButton(NULL, SDL_TOUCH_MOUSEID, SDL_RELEASED, SDL_BUTTON_LEFT);
            leftFingerDown = nil;
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
#ifdef IPHONE_TOUCH_EFFICIENT_DANGEROUS
        SDL_SendTouch(touchId, (long)touch,
                      SDL_FALSE, locationInView.x, locationInView.y, 1.0f);
#else
        int i;
        for (i = 0; i < MAX_SIMULTANEOUS_TOUCHES; i++) {
            if (finger[i] == touch) {
                SDL_SendTouch(touchId, i,
                              SDL_FALSE, locationInView.x, locationInView.y, 1.0f);
                finger[i] = NULL;
                break;
            }
        }
#endif
        touch = (UITouch*)[enumerator nextObject];
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    /*
        this can happen if the user puts more than 5 touches on the screen
        at once, or perhaps in other circumstances.  Usually (it seems)
        all active touches are canceled.
    */
    [self touchesEnded: touches withEvent: event];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    NSEnumerator *enumerator = [touches objectEnumerator];
    UITouch *touch = (UITouch*)[enumerator nextObject];

    while (touch) {
        if (touch == leftFingerDown) {
            CGPoint locationInView = [self touchLocation:touch shouldNormalize:NO];

            /* send moved event */
            SDL_SendMouseMotion(NULL, SDL_TOUCH_MOUSEID, 0, locationInView.x, locationInView.y);
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
#ifdef IPHONE_TOUCH_EFFICIENT_DANGEROUS
        SDL_SendTouchMotion(touchId, (long)touch,
                            locationInView.x, locationInView.y, 1.0f);
#else
        int i;
        for (i = 0; i < MAX_SIMULTANEOUS_TOUCHES; i++) {
            if (finger[i] == touch) {
                SDL_SendTouchMotion(touchId, i,
                                    locationInView.x, locationInView.y, 1.0f);
                break;
            }
        }
#endif
        touch = (UITouch*)[enumerator nextObject];
    }
}

/*
    ---- Keyboard related functionality below this line ----
*/
#if SDL_IPHONE_KEYBOARD

/* Is the iPhone virtual keyboard visible onscreen? */
- (BOOL)keyboardVisible
{
    return keyboardVisible;
}

/* Set ourselves up as a UITextFieldDelegate */
- (void)initializeKeyboard
{
    textField = [[UITextField alloc] initWithFrame: CGRectZero];
    textField.delegate = self;
    /* placeholder so there is something to delete! */
    textField.text = @" ";

    /* set UITextInputTrait properties, mostly to defaults */
    textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    textField.autocorrectionType = UITextAutocorrectionTypeNo;
    textField.enablesReturnKeyAutomatically = NO;
    textField.keyboardAppearance = UIKeyboardAppearanceDefault;
    textField.keyboardType = UIKeyboardTypeDefault;
    textField.returnKeyType = UIReturnKeyDefault;
    textField.secureTextEntry = NO;

    textField.hidden = YES;
    keyboardVisible = NO;
    /* add the UITextField (hidden) to our view */
    [self addSubview: textField];
    [textField release];
}

/* reveal onscreen virtual keyboard */
- (void)showKeyboard
{
    keyboardVisible = YES;
    [textField becomeFirstResponder];
}

/* hide onscreen virtual keyboard */
- (void)hideKeyboard
{
    keyboardVisible = NO;
    [textField resignFirstResponder];
}

/* UITextFieldDelegate method.  Invoked when user types something. */
- (BOOL)textField:(UITextField *)_textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
    if ([string length] == 0) {
        /* it wants to replace text with nothing, ie a delete */
        SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_BACKSPACE);
        SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_BACKSPACE);
    }
    else {
        /* go through all the characters in the string we've been sent
           and convert them to key presses */
        int i;
        for (i = 0; i < [string length]; i++) {

            unichar c = [string characterAtIndex: i];

            Uint16 mod = 0;
            SDL_Scancode code;

            if (c < 127) {
                /* figure out the SDL_Scancode and SDL_keymod for this unichar */
                code = unicharToUIKeyInfoTable[c].code;
                mod  = unicharToUIKeyInfoTable[c].mod;
            }
            else {
                /* we only deal with ASCII right now */
                code = SDL_SCANCODE_UNKNOWN;
                mod = 0;
            }

            if (mod & KMOD_SHIFT) {
                /* If character uses shift, press shift down */
                SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_LSHIFT);
            }
            /* send a keydown and keyup even for the character */
            SDL_SendKeyboardKey(SDL_PRESSED, code);
            SDL_SendKeyboardKey(SDL_RELEASED, code);
            if (mod & KMOD_SHIFT) {
                /* If character uses shift, press shift back up */
                SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_LSHIFT);
            }
        }
        SDL_SendKeyboardText([string UTF8String]);
    }
    return NO; /* don't allow the edit! (keep placeholder text there) */
}

/* Terminates the editing session */
- (BOOL)textFieldShouldReturn:(UITextField*)_textField
{
    SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_RETURN);
    SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_RETURN);
    SDL_StopTextInput();
    return YES;
}

#endif

@end

/* iPhone keyboard addition functions */
#if SDL_IPHONE_KEYBOARD

static SDL_uikitview * getWindowView(SDL_Window * window)
{
    if (window == NULL) {
        SDL_SetError("Window does not exist");
        return nil;
    }

    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    SDL_uikitview *view = data != NULL ? data->view : nil;

    if (view == nil) {
        SDL_SetError("Window has no view");
    }

    return view;
}

SDL_bool UIKit_HasScreenKeyboardSupport(_THIS)
{
    return SDL_TRUE;
}

void UIKit_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
    SDL_uikitview *view = getWindowView(window);
    if (view != nil) {
        [view showKeyboard];
    }
}

void UIKit_HideScreenKeyboard(_THIS, SDL_Window *window)
{
    SDL_uikitview *view = getWindowView(window);
    if (view != nil) {
        [view hideKeyboard];
    }
}

SDL_bool UIKit_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    SDL_uikitview *view = getWindowView(window);
    if (view == nil) {
        return 0;
    }

    return view.keyboardVisible;
}

#endif /* SDL_IPHONE_KEYBOARD */

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
