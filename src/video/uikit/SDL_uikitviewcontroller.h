/*
 Simple DirectMedia Layer
 Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#import <UIKit/UIKit.h>

#include "../SDL_sysvideo.h"

#include "SDL_touch.h"

#if SDL_IPHONE_KEYBOARD
@interface SDL_uikitviewcontroller : UIViewController <UITextFieldDelegate>
#else
@interface SDL_uikitviewcontroller : UIViewController
#endif

@property (nonatomic, assign) SDL_Window *window;

- (instancetype)initWithSDLWindow:(SDL_Window *)_window;

- (void)setAnimationCallback:(int)interval
                    callback:(void (*)(void*))callback
               callbackParam:(void*)callbackParam;

- (void)startAnimation;
- (void)stopAnimation;

- (void)doLoop:(CADisplayLink*)sender;

- (void)loadView;
- (void)viewDidLayoutSubviews;
- (NSUInteger)supportedInterfaceOrientations;
- (BOOL)prefersStatusBarHidden;

#if SDL_IPHONE_KEYBOARD
- (void)showKeyboard;
- (void)hideKeyboard;
- (void)initKeyboard;
- (void)deinitKeyboard;

- (void)keyboardWillShow:(NSNotification *)notification;
- (void)keyboardWillHide:(NSNotification *)notification;

- (void)updateKeyboard;

@property (nonatomic, assign, getter=isKeyboardVisible) BOOL keyboardVisible;
@property (nonatomic, assign) SDL_Rect textInputRect;
@property (nonatomic, assign) int keyboardHeight;
#endif

@end

#if SDL_IPHONE_KEYBOARD
SDL_bool UIKit_HasScreenKeyboardSupport(_THIS);
void UIKit_ShowScreenKeyboard(_THIS, SDL_Window *window);
void UIKit_HideScreenKeyboard(_THIS, SDL_Window *window);
SDL_bool UIKit_IsScreenKeyboardShown(_THIS, SDL_Window *window);
void UIKit_SetTextInputRect(_THIS, SDL_Rect *rect);
#endif
