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

@implementation SDL_uikitview {

    SDL_TouchID touchId;
    UITouch *leftFingerDown;

#if SDL_IPHONE_KEYBOARD
    UITextField *textField;
#endif

}

@synthesize viewcontroller;

- (id)initWithFrame:(CGRect)frame
{
    if (self = [super initWithFrame:frame]) {
#if SDL_IPHONE_KEYBOARD
        [self initKeyboard];
#endif

        self.multipleTouchEnabled = YES;

        touchId = 1;
        SDL_AddTouch(touchId, "");
    }

    return self;

}

- (void)dealloc
{
#if SDL_IPHONE_KEYBOARD
    [self deinitKeyboard];
#endif
}

- (CGPoint)touchLocation:(UITouch *)touch shouldNormalize:(BOOL)normalize
{
    CGPoint point = [touch locationInView:self];

    if (normalize) {
        CGRect bounds = self.bounds;
        point.x /= bounds.size.width;
        point.y /= bounds.size.height;
    }

    return point;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        if (!leftFingerDown) {
            CGPoint locationInView = [self touchLocation:touch shouldNormalize:NO];

            /* send moved event */
            SDL_SendMouseMotion(self.viewcontroller.window, SDL_TOUCH_MOUSEID, 0, locationInView.x, locationInView.y);

            /* send mouse down event */
            SDL_SendMouseButton(self.viewcontroller.window, SDL_TOUCH_MOUSEID, SDL_PRESSED, SDL_BUTTON_LEFT);

            leftFingerDown = touch;
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
        SDL_SendTouch(touchId, (SDL_FingerID)((size_t)touch),
                      SDL_TRUE, locationInView.x, locationInView.y, 1.0f);
    }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        if (touch == leftFingerDown) {
            /* send mouse up */
            SDL_SendMouseButton(self.viewcontroller.window, SDL_TOUCH_MOUSEID, SDL_RELEASED, SDL_BUTTON_LEFT);
            leftFingerDown = nil;
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
        SDL_SendTouch(touchId, (SDL_FingerID)((size_t)touch),
                      SDL_FALSE, locationInView.x, locationInView.y, 1.0f);
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    /*
        this can happen if the user puts more than 5 touches on the screen
        at once, or perhaps in other circumstances.  Usually (it seems)
        all active touches are canceled.
    */
    [self touchesEnded:touches withEvent:event];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        if (touch == leftFingerDown) {
            CGPoint locationInView = [self touchLocation:touch shouldNormalize:NO];

            /* send moved event */
            SDL_SendMouseMotion(self.viewcontroller.window, SDL_TOUCH_MOUSEID, 0, locationInView.x, locationInView.y);
        }

        CGPoint locationInView = [self touchLocation:touch shouldNormalize:YES];
        SDL_SendTouchMotion(touchId, (SDL_FingerID)((size_t)touch),
                            locationInView.x, locationInView.y, 1.0f);
    }
}

/*
    ---- Keyboard related functionality below this line ----
*/
#if SDL_IPHONE_KEYBOARD

@synthesize textInputRect;
@synthesize keyboardHeight;
@synthesize keyboardVisible;

/* Set ourselves up as a UITextFieldDelegate */
- (void)initKeyboard
{
    textField = [[UITextField alloc] initWithFrame:CGRectZero];
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
    [self addSubview:textField];

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
    [center addObserver:self selector:@selector(keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];
}

- (void)deinitKeyboard
{
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self name:UIKeyboardWillShowNotification object:nil];
    [center removeObserver:self name:UIKeyboardWillHideNotification object:nil];
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

- (void)keyboardWillShow:(NSNotification *)notification
{
    CGRect kbrect = [[notification userInfo][UIKeyboardFrameBeginUserInfoKey] CGRectValue];
    int height;

    /* The keyboard rect is in the coordinate space of the screen, but we want
     * its height in the view's coordinate space.
     */
#ifdef __IPHONE_8_0
    if ([self respondsToSelector:@selector(convertRect:fromCoordinateSpace:)]) {
        UIScreen *screen = self.window.screen;
        kbrect = [self convertRect:kbrect fromCoordinateSpace:screen.coordinateSpace];
        height = kbrect.size.height;
    } else
#endif
    {
        /* In iOS 7 and below, the screen's coordinate space is never rotated. */
        if (UIInterfaceOrientationIsLandscape([[UIApplication sharedApplication] statusBarOrientation])) {
            height = kbrect.size.width;
        } else {
            height = kbrect.size.height;
        }
    }

    [self setKeyboardHeight:height];
}

 - (void)keyboardWillHide:(NSNotification *)notification
{
    [self setKeyboardHeight:0];
}

- (void)updateKeyboard
{
    SDL_Rect textrect = self.textInputRect;
    CGAffineTransform t = self.transform;
    CGPoint offset = CGPointMake(0.0, 0.0);

    if (self.keyboardHeight) {
        int rectbottom = textrect.y + textrect.h;
        int kbottom = self.bounds.size.height - self.keyboardHeight;
        if (kbottom < rectbottom) {
            offset.y = kbottom - rectbottom;
        }
    }

    /* Put the offset into the this view transform's coordinate space. */
    t.tx = 0.0;
    t.ty = 0.0;
    offset = CGPointApplyAffineTransform(offset, t);

    t.tx = offset.x;
    t.ty = offset.y;

    /* Move the view by applying the updated transform. */
    self.transform = t;
}

- (void)setKeyboardHeight:(int)height
{
    keyboardVisible = height > 0;
    keyboardHeight = height;
    [self updateKeyboard];
}

/* UITextFieldDelegate method.  Invoked when user types something. */
- (BOOL)textField:(UITextField *)_textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
    NSUInteger len = string.length;

    if (len == 0) {
        /* it wants to replace text with nothing, ie a delete */
        SDL_SendKeyboardKey(SDL_PRESSED, SDL_SCANCODE_BACKSPACE);
        SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_BACKSPACE);
    } else {
        /* go through all the characters in the string we've been sent
           and convert them to key presses */
        for (int i = 0; i < len; i++) {
            unichar c = [string characterAtIndex:i];
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

static SDL_uikitview *
GetWindowView(SDL_Window * window)
{
    if (window == NULL) {
        SDL_SetError("Window does not exist");
        return nil;
    }

    SDL_WindowData *data = (__bridge SDL_WindowData *)window->driverdata;
    SDL_uikitview *view = data != nil ? data.view : nil;

    if (view == nil) {
        SDL_SetError("Window has no view");
    }

    return view;
}

SDL_bool
UIKit_HasScreenKeyboardSupport(_THIS)
{
    return SDL_TRUE;
}

void
UIKit_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitview *view = GetWindowView(window);
        if (view != nil) {
            [view showKeyboard];
        }
    }
}

void
UIKit_HideScreenKeyboard(_THIS, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitview *view = GetWindowView(window);
        if (view != nil) {
            [view hideKeyboard];
        }
    }
}

SDL_bool
UIKit_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitview *view = GetWindowView(window);
        if (view != nil) {
            return view.isKeyboardVisible;
        }
        return 0;
    }
}

void
UIKit_SetTextInputRect(_THIS, SDL_Rect *rect)
{
    if (!rect) {
        SDL_InvalidParamError("rect");
        return;
    }

    @autoreleasepool {
        SDL_uikitview *view = GetWindowView(SDL_GetFocusWindow());
        if (view != nil) {
            view.textInputRect = *rect;

            if (view.keyboardVisible) {
                [view updateKeyboard];
            }
        }
    }
}


#endif /* SDL_IPHONE_KEYBOARD */

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
