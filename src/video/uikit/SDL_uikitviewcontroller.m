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

#ifdef SDL_VIDEO_DRIVER_UIKIT

#include "../SDL_sysvideo.h"
#include "../../events/SDL_events_c.h"

#include "SDL_uikitviewcontroller.h"
#include "SDL_uikitmessagebox.h"
#include "SDL_uikitevents.h"
#include "SDL_uikitvideo.h"
#include "SDL_uikitmodes.h"
#include "SDL_uikitwindow.h"
#include "SDL_uikitopengles.h"

#ifdef SDL_PLATFORM_TVOS
static void SDLCALL SDL_AppleTVControllerUIHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *viewcontroller = (__bridge SDL_uikitviewcontroller *)userdata;
        viewcontroller.controllerUserInteractionEnabled = hint && (*hint != '0');
    }
}
#endif

#ifndef SDL_PLATFORM_TVOS
static void SDLCALL SDL_HideHomeIndicatorHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *viewcontroller = (__bridge SDL_uikitviewcontroller *)userdata;
        viewcontroller.homeIndicatorHidden = (hint && *hint) ? SDL_atoi(hint) : -1;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
        if ([viewcontroller respondsToSelector:@selector(setNeedsUpdateOfHomeIndicatorAutoHidden)]) {
            [viewcontroller setNeedsUpdateOfHomeIndicatorAutoHidden];
            [viewcontroller setNeedsUpdateOfScreenEdgesDeferringSystemGestures];
        }
#pragma clang diagnostic pop
    }
}
#endif

@implementation SDLUITextField : UITextField
- (BOOL)canPerformAction:(SEL)action withSender:(id)sender
{
    if (action == @selector(paste:)) {
        return NO;
    }

    return [super canPerformAction:action withSender:sender];
}
@end

@implementation SDL_uikitviewcontroller
{
    CADisplayLink *displayLink;
    int animationInterval;
    void (*animationCallback)(void *);
    void *animationCallbackParam;

#ifdef SDL_IPHONE_KEYBOARD
    SDLUITextField *textField;
    BOOL hardwareKeyboard;
    BOOL showingKeyboard;
    BOOL hidingKeyboard;
    BOOL rotatingOrientation;
    NSString *committedText;
    NSString *obligateForBackspace;
#endif
}

@synthesize window;

- (instancetype)initWithSDLWindow:(SDL_Window *)_window
{
    if (self = [super initWithNibName:nil bundle:nil]) {
        self.window = _window;

#ifdef SDL_IPHONE_KEYBOARD
        [self initKeyboard];
        hardwareKeyboard = NO;
        showingKeyboard = NO;
        hidingKeyboard = NO;
        rotatingOrientation = NO;
#endif

#ifdef SDL_PLATFORM_TVOS
        SDL_AddHintCallback(SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS,
                            SDL_AppleTVControllerUIHintChanged,
                            (__bridge void *)self);
#endif

#ifndef SDL_PLATFORM_TVOS
        SDL_AddHintCallback(SDL_HINT_IOS_HIDE_HOME_INDICATOR,
                            SDL_HideHomeIndicatorHintChanged,
                            (__bridge void *)self);
#endif
    }
    return self;
}

- (void)dealloc
{
#ifdef SDL_IPHONE_KEYBOARD
    [self deinitKeyboard];
#endif

#ifdef SDL_PLATFORM_TVOS
    SDL_DelHintCallback(SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS,
                        SDL_AppleTVControllerUIHintChanged,
                        (__bridge void *)self);
#endif

#ifndef SDL_PLATFORM_TVOS
    SDL_DelHintCallback(SDL_HINT_IOS_HIDE_HOME_INDICATOR,
                        SDL_HideHomeIndicatorHintChanged,
                        (__bridge void *)self);
#endif
}

- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection
{
    SDL_SetSystemTheme(UIKit_GetSystemTheme());
}

- (void)setAnimationCallback:(int)interval
                    callback:(void (*)(void *))callback
               callbackParam:(void *)callbackParam
{
    [self stopAnimation];

    animationInterval = interval;
    animationCallback = callback;
    animationCallbackParam = callbackParam;

    if (animationCallback) {
        [self startAnimation];
    }
}

- (void)startAnimation
{
    displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(doLoop:)];

#ifdef SDL_PLATFORM_VISIONOS
    displayLink.preferredFramesPerSecond = 90 / animationInterval;      //TODO: Get frame max frame rate on visionOS
#elif defined(__IPHONE_10_3)
    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->driverdata;

    if ([displayLink respondsToSelector:@selector(preferredFramesPerSecond)] && data != nil && data.uiwindow != nil && [data.uiwindow.screen respondsToSelector:@selector(maximumFramesPerSecond)]) {
        displayLink.preferredFramesPerSecond = data.uiwindow.screen.maximumFramesPerSecond / animationInterval;
    } else
#endif
    {
#if __IPHONE_OS_VERSION_MIN_REQUIRED < 100300
        [displayLink setFrameInterval:animationInterval];
#endif
    }

    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)stopAnimation
{
    [displayLink invalidate];
    displayLink = nil;
}

- (void)doLoop:(CADisplayLink *)sender
{
    /* Don't run the game loop while a messagebox is up */
    if (!UIKit_ShowingMessageBox()) {
        /* See the comment in the function definition. */
#if defined(SDL_VIDEO_OPENGL_ES) || defined(SDL_VIDEO_OPENGL_ES2)
        UIKit_GL_RestoreCurrentContext();
#endif

        animationCallback(animationCallbackParam);
    }
}

- (void)loadView
{
    /* Do nothing. */
}

- (void)viewDidLayoutSubviews
{
    const CGSize size = self.view.bounds.size;
    int w = (int)size.width;
    int h = (int)size.height;

    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, w, h);
}

#ifndef SDL_PLATFORM_TVOS
- (NSUInteger)supportedInterfaceOrientations
{
    return UIKit_GetSupportedOrientations(window);
}

- (BOOL)prefersStatusBarHidden
{
    BOOL hidden = (window->flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS)) != 0;
    return hidden;
}

- (BOOL)prefersHomeIndicatorAutoHidden
{
    BOOL hidden = NO;
    if (self.homeIndicatorHidden == 1) {
        hidden = YES;
    }
    return hidden;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
    if (self.homeIndicatorHidden >= 0) {
        if (self.homeIndicatorHidden == 2) {
            return UIRectEdgeAll;
        } else {
            return UIRectEdgeNone;
        }
    }

    /* By default, fullscreen and borderless windows get all screen gestures */
    if ((window->flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS)) != 0) {
        return UIRectEdgeAll;
    } else {
        return UIRectEdgeNone;
    }
}

- (BOOL)prefersPointerLocked
{
    return SDL_GCMouseRelativeMode() ? YES : NO;
}

#endif /* !SDL_PLATFORM_TVOS */

/*
 ---- Keyboard related functionality below this line ----
 */
#ifdef SDL_IPHONE_KEYBOARD

@synthesize textInputRect;
@synthesize keyboardHeight;
@synthesize keyboardVisible;

/* Set ourselves up as a UITextFieldDelegate */
- (void)initKeyboard
{
    obligateForBackspace = @"                                                                "; /* 64 space */
    textField = [[SDLUITextField alloc] initWithFrame:CGRectZero];
    textField.delegate = self;
    /* placeholder so there is something to delete! */
    textField.text = obligateForBackspace;
    committedText = textField.text;

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

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#ifndef SDL_PLATFORM_TVOS
    [center addObserver:self
               selector:@selector(keyboardWillShow:)
                   name:UIKeyboardWillShowNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(keyboardDidShow:)
                   name:UIKeyboardDidShowNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(keyboardWillHide:)
                   name:UIKeyboardWillHideNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(keyboardDidHide:)
                   name:UIKeyboardDidHideNotification
                 object:nil];
#endif
    [center addObserver:self
               selector:@selector(textFieldTextDidChange:)
                   name:UITextFieldTextDidChangeNotification
                 object:nil];
}

- (NSArray *)keyCommands
{
    NSMutableArray *commands = [[NSMutableArray alloc] init];
    [commands addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:kNilOptions action:@selector(handleCommand:)]];
    [commands addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:kNilOptions action:@selector(handleCommand:)]];
    [commands addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:kNilOptions action:@selector(handleCommand:)]];
    [commands addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:kNilOptions action:@selector(handleCommand:)]];
    [commands addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:kNilOptions action:@selector(handleCommand:)]];
    return [NSArray arrayWithArray:commands];
}

- (void)handleCommand:(UIKeyCommand *)keyCommand
{
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    NSString *input = keyCommand.input;

    if (input == UIKeyInputUpArrow) {
        scancode = SDL_SCANCODE_UP;
    } else if (input == UIKeyInputDownArrow) {
        scancode = SDL_SCANCODE_DOWN;
    } else if (input == UIKeyInputLeftArrow) {
        scancode = SDL_SCANCODE_LEFT;
    } else if (input == UIKeyInputRightArrow) {
        scancode = SDL_SCANCODE_RIGHT;
    } else if (input == UIKeyInputEscape) {
        scancode = SDL_SCANCODE_ESCAPE;
    }

    if (scancode != SDL_SCANCODE_UNKNOWN) {
        SDL_SendKeyboardKeyAutoRelease(0, scancode);
    }
}

- (void)setView:(UIView *)view
{
    [super setView:view];

    [view addSubview:textField];

    if (keyboardVisible) {
        [self showKeyboard];
    }
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    rotatingOrientation = YES;
    [coordinator
        animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        }
        completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
          self->rotatingOrientation = NO;
        }];
}

- (void)deinitKeyboard
{
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#ifndef SDL_PLATFORM_TVOS
    [center removeObserver:self
                      name:UIKeyboardWillShowNotification
                    object:nil];
    [center removeObserver:self
                      name:UIKeyboardDidShowNotification
                    object:nil];
    [center removeObserver:self
                      name:UIKeyboardWillHideNotification
                    object:nil];
    [center removeObserver:self
                      name:UIKeyboardDidHideNotification
                    object:nil];
#endif
    [center removeObserver:self
                      name:UITextFieldTextDidChangeNotification
                    object:nil];
}

/* reveal onscreen virtual keyboard */
- (void)showKeyboard
{
    if (keyboardVisible) {
        return;
    }

    keyboardVisible = YES;
    if (textField.window) {
        showingKeyboard = YES;
        [textField becomeFirstResponder];
    }
}

/* hide onscreen virtual keyboard */
- (void)hideKeyboard
{
    if (!keyboardVisible) {
        return;
    }

    keyboardVisible = NO;
    if (textField.window) {
        hidingKeyboard = YES;
        [textField resignFirstResponder];
    }
}

- (void)keyboardWillShow:(NSNotification *)notification
{
    BOOL shouldStartTextInput = NO;

    if (!SDL_TextInputActive() && !hidingKeyboard && !rotatingOrientation) {
        shouldStartTextInput = YES;
    }

    showingKeyboard = YES;
#ifndef SDL_PLATFORM_TVOS
    CGRect kbrect = [[notification userInfo][UIKeyboardFrameEndUserInfoKey] CGRectValue];

    /* The keyboard rect is in the coordinate space of the screen/window, but we
     * want its height in the coordinate space of the view. */
    kbrect = [self.view convertRect:kbrect fromView:nil];

    [self setKeyboardHeight:(int)kbrect.size.height];
#endif

    if (shouldStartTextInput) {
        SDL_StartTextInput();
    }
}

- (void)keyboardDidShow:(NSNotification *)notification
{
    showingKeyboard = NO;
}

- (void)keyboardWillHide:(NSNotification *)notification
{
    BOOL shouldStopTextInput = NO;

    if (SDL_TextInputActive() && !showingKeyboard && !rotatingOrientation) {
        shouldStopTextInput = YES;
    }

    hidingKeyboard = YES;
    [self setKeyboardHeight:0];

    if (shouldStopTextInput) {
        SDL_StopTextInput();
    }
}

- (void)keyboardDidHide:(NSNotification *)notification
{
    hidingKeyboard = NO;
}

- (void)textFieldTextDidChange:(NSNotification *)notification
{
    if (textField.markedTextRange == nil) {
        NSUInteger compareLength = SDL_min(textField.text.length, committedText.length);
        NSUInteger matchLength;

        /* Backspace over characters that are no longer in the string */
        for (matchLength = 0; matchLength < compareLength; ++matchLength) {
            if ([committedText characterAtIndex:matchLength] != [textField.text characterAtIndex:matchLength]) {
                break;
            }
        }
        if (matchLength < committedText.length) {
            size_t deleteLength = SDL_utf8strlen([[committedText substringFromIndex:matchLength] UTF8String]);
            while (deleteLength > 0) {
                /* Send distinct down and up events for each backspace action */
                SDL_SendVirtualKeyboardKey(0, SDL_PRESSED, SDL_SCANCODE_BACKSPACE);
                SDL_SendVirtualKeyboardKey(0, SDL_RELEASED, SDL_SCANCODE_BACKSPACE);
                --deleteLength;
            }
        }

        if (matchLength < textField.text.length) {
            NSString *pendingText = [textField.text substringFromIndex:matchLength];
            if (!SDL_HardwareKeyboardKeyPressed()) {
                /* Go through all the characters in the string we've been sent and
                 * convert them to key presses */
                NSUInteger i;
                for (i = 0; i < pendingText.length; i++) {
                    SDL_SendKeyboardUnicodeKey(0, [pendingText characterAtIndex:i]);
                }
            }
            SDL_SendKeyboardText([pendingText UTF8String]);
        }
        committedText = textField.text;
    }
}

- (void)updateKeyboard
{
    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *) window->driverdata;

    CGAffineTransform t = self.view.transform;
    CGPoint offset = CGPointMake(0.0, 0.0);
#ifdef SDL_PLATFORM_VISIONOS
    CGRect frame = UIKit_ComputeViewFrame(window);
#else
    CGRect frame = UIKit_ComputeViewFrame(window, data.uiwindow.screen);
#endif

    if (self.keyboardHeight) {
        int rectbottom = self.textInputRect.y + self.textInputRect.h;
        int keybottom = self.view.bounds.size.height - self.keyboardHeight;
        if (keybottom < rectbottom) {
            offset.y = keybottom - rectbottom;
        }
    }

    /* Apply this view's transform (except any translation) to the offset, in
     * order to orient it correctly relative to the frame's coordinate space. */
    t.tx = 0.0;
    t.ty = 0.0;
    offset = CGPointApplyAffineTransform(offset, t);

    /* Apply the updated offset to the view's frame. */
    frame.origin.x += offset.x;
    frame.origin.y += offset.y;

    self.view.frame = frame;
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
    if (textField.markedTextRange == nil) {
        if (textField.text.length < 16) {
            textField.text = obligateForBackspace;
            committedText = textField.text;
        }
    }
    return YES;
}

/* Terminates the editing session */
- (BOOL)textFieldShouldReturn:(UITextField *)_textField
{
    SDL_SendKeyboardKeyAutoRelease(0, SDL_SCANCODE_RETURN);
    if (keyboardVisible &&
        SDL_GetHintBoolean(SDL_HINT_RETURN_KEY_HIDES_IME, SDL_FALSE)) {
        SDL_StopTextInput();
    }
    return YES;
}

#endif

@end

/* iPhone keyboard addition functions */
#ifdef SDL_IPHONE_KEYBOARD

static SDL_uikitviewcontroller *GetWindowViewController(SDL_Window *window)
{
    if (!window || !window->driverdata) {
        SDL_SetError("Invalid window");
        return nil;
    }

    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->driverdata;

    return data.viewcontroller;
}

SDL_bool UIKit_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    return SDL_TRUE;
}

void UIKit_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *vc = GetWindowViewController(window);
        [vc showKeyboard];
    }
}

void UIKit_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *vc = GetWindowViewController(window);
        [vc hideKeyboard];
    }
}

SDL_bool UIKit_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *vc = GetWindowViewController(window);
        if (vc != nil) {
            return vc.keyboardVisible;
        }
        return SDL_FALSE;
    }
}

int UIKit_SetTextInputRect(SDL_VideoDevice *_this, const SDL_Rect *rect)
{
    @autoreleasepool {
        SDL_uikitviewcontroller *vc = GetWindowViewController(SDL_GetKeyboardFocus());
        if (vc != nil) {
            vc.textInputRect = *rect;

            if (vc.keyboardVisible) {
                [vc updateKeyboard];
            }
        }
    }
    return 0;
}

#endif /* SDL_IPHONE_KEYBOARD */

#endif /* SDL_VIDEO_DRIVER_UIKIT */
