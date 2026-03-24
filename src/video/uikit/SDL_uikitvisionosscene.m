/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_PLATFORM_VISIONOS

#import "SDL_uikitvisionosscene.h"
#include "SDL_uikitevents.h"
#include "SDL_uikitwindow.h"
#include "../../events/SDL_events_c.h"


@interface SDL_UIKitVisionOSScene ()

@property (nonatomic, assign, readwrite) BOOL isPresented;
@property (nonatomic, strong) UISceneSession *sceneSession;
@property (nonatomic, assign) CGSize contentSize;
@property (nonatomic, assign) CGFloat curvature;
@property (nonatomic, strong) UISceneSession *mainSceneSession;

@end

// Returns the ObjC class name for the immersive scene
static void SDL_GetSceneConfig(NSString * __autoreleasing *outClassName)
{
    *outClassName = @"SDL_ImmersiveHostingSceneDelegate";
}

// Helper function to get the Swift hosting delegate class for the given mode
static Class SDL_GetHostingDelegateClass()
{
    NSString *className = nil;
    SDL_GetSceneConfig(&className);

    // The @objc(...) attribute on the Swift class gives it a stable ObjC name
    Class delegateClass = NSClassFromString(className);
    if (delegateClass) {
        return delegateClass;
    }

    SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                "SDL_UIKitVisionOSScene: Could not find %s delegate class", [className UTF8String]);
    return nil;
}

// Helper function to get the shared delegate instance
static id SDL_GetSharedDelegate()
{
    Class delegateClass = SDL_GetHostingDelegateClass();
    if (!delegateClass) {
        return nil;
    }

    SEL sharedSelector = NSSelectorFromString(@"shared");
    if (![delegateClass respondsToSelector:sharedSelector]) {
        return nil;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    return [delegateClass performSelector:sharedSelector];
#pragma clang diagnostic pop
}

@implementation SDL_UIKitVisionOSScene

- (instancetype)initWithWindowScene:(UIWindowScene *)windowScene
                   curvature:(CGFloat)curvature
                        size:(CGSize)size
            mainSceneSession:(UISceneSession *)mainSceneSession
{
    self = [super init];
    if (!self) {
        return nil;
    }

    _isPresented = NO;
    _contentSize = size;
    _curvature = curvature;
    _mainSceneSession = mainSceneSession;

    SDL_Log("SDL_UIKitVisionOSScene: Initialized with size %.0fx%.0f, curvature %.2f",
            size.width, size.height, curvature);

    return self;
}

- (void)present
{
    if (_isPresented) {
        SDL_Log("SDL_UIKitVisionOSScene: Already presented");
        return;
    }

    SDL_Log("SDL_UIKitVisionOSScene: Presenting via UIHostingSceneDelegate bridging");

    Class delegateClass = SDL_GetHostingDelegateClass();
    if (!delegateClass) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                    "SDL_UIKitVisionOSScene: Delegate class not found");
        return;
    }

    // Get the shared Swift scene delegate for configuration
    id sharedDelegate = SDL_GetSharedDelegate();
    if (!sharedDelegate) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                    "SDL_UIKitVisionOSScene: Failed to get shared delegate");
        return;
    }

    // Configure the delegate with curvature and Metal device
    SEL configureSelector = NSSelectorFromString(@"configureWithSize:curvature:");
    if ([sharedDelegate respondsToSelector:configureSelector]) {
        NSMethodSignature *signature = [sharedDelegate methodSignatureForSelector:configureSelector];
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
        [invocation setSelector:configureSelector];
        [invocation setTarget:sharedDelegate];

        [invocation setArgument:&_contentSize atIndex:2];
        float curvatureFloat = (float)_curvature;
        [invocation setArgument:&curvatureFloat atIndex:3];
        [invocation invoke];

        SDL_Log("SDL_UIKitVisionOSScene: Configured with size %gx%g and curvature %.2f", _contentSize.width, _contentSize.height, curvatureFloat);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                   "SDL_UIKitVisionOSScene: Delegate doesn't respond to configure");
    }

    // Set the static scene-connected callback on the delegate CLASS (not instance).
    // SwiftUI creates a new delegate instance for each scene, so instance properties
    // set on 'shared' won't be seen by the new instance. Static properties work.
    __weak typeof(self) weakSelf = self;
    SEL callbackSelector = NSSelectorFromString(@"setSceneConnectedCallback:");
    if ([delegateClass respondsToSelector:callbackSelector]) {
        void (^callback)(UISceneSession *) = ^(UISceneSession *session) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (strongSelf) {
                strongSelf.sceneSession = session;
                strongSelf.isPresented = YES;
                SDL_Log("SDL_UIKitVisionOSScene: Scene connected, session: %p", session);
            }
        };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
        [delegateClass performSelector:callbackSelector withObject:callback];
#pragma clang diagnostic pop
    }

    if (_mainSceneSession) {
        SDL_Log("SDL_UIKitVisionOSScene: Keeping main scene session alive");
    }

    // Activate using the UIHostingSceneDelegate bridging API
    SEL activateSelector = NSSelectorFromString(@"activateSceneWithErrorHandler:");
    if (![delegateClass respondsToSelector:activateSelector]) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "SDL_UIKitVisionOSScene: Delegate doesn't respond to activateSceneWithErrorHandler:");
        return;
    }

    void (^errorBlock)(NSError *) = ^(NSError *error) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                    "SDL_UIKitVisionOSScene: Activation failed: %s (code: %ld)",
                    [[error localizedDescription] UTF8String],
                    (long)error.code);
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            strongSelf.isPresented = NO;
        }
    };

    NSMethodSignature *signature = [delegateClass methodSignatureForSelector:activateSelector];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:activateSelector];
    [invocation setTarget:delegateClass];
    [invocation setArgument:&errorBlock atIndex:2];
    [invocation invoke];

    SDL_Log("SDL_UIKitVisionOSScene: Activation call completed");
}

- (void)dismiss
{
    if (!_isPresented) {
        SDL_Log("SDL_UIKitVisionOSScene: Not presented, trying static dismiss as fallback");
    }

    SDL_Log("SDL_UIKitVisionOSScene: Dismissing");

    // Use the static dismiss method which tracks the session
    // across SwiftUI-created delegate instances
    Class delegateClass = SDL_GetHostingDelegateClass();
    if (delegateClass) {
        SEL dismissSelector = NSSelectorFromString(@"dismissScene");
        if ([delegateClass respondsToSelector:dismissSelector]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
            [delegateClass performSelector:dismissSelector];
#pragma clang diagnostic pop
            SDL_Log("SDL_UIKitVisionOSScene: Called static dismiss");
        }
    }

    // Also try our local session reference as a fallback
    if (_sceneSession) {
        UISceneDestructionRequestOptions *options = [[UISceneDestructionRequestOptions alloc] init];
        [[UIApplication sharedApplication] requestSceneSessionDestruction:_sceneSession
                                                                  options:options
                                                             errorHandler:^(NSError *error) {
            if (error) {
                SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                           "SDL_UIKitVisionOSScene: Failed to dismiss scene: %s",
                           [[error localizedDescription] UTF8String]);
            } else {
                SDL_Log("SDL_UIKitVisionOSScene: Scene destroyed via local session");
            }
        }];
        _sceneSession = nil;
    }

    _isPresented = NO;
}

- (id<MTLTexture>)getDisplayTexture:(id<MTLCommandBuffer>)commandBuffer
                              width:(int)width
                             height:(int)height
                        pixelFormat:(MTLPixelFormat)pixelFormat
{
    id sharedDelegate = SDL_GetSharedDelegate();
    if (!sharedDelegate) {
        return nil;
    }

    SEL getDisplayTextureSelector = NSSelectorFromString(@"getDisplayTexture:width:height:pixelFormat:");
    if ([sharedDelegate respondsToSelector:getDisplayTextureSelector]) {
        NSMethodSignature *signature = [sharedDelegate methodSignatureForSelector:getDisplayTextureSelector];
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
        [invocation setSelector:getDisplayTextureSelector];
        [invocation setTarget:sharedDelegate];

        long arg_width = width;
        long arg_height = height;
        [invocation setArgument:&commandBuffer atIndex:2];
        [invocation setArgument:&arg_width atIndex:3];
        [invocation setArgument:&arg_height atIndex:4];
        [invocation setArgument:&pixelFormat atIndex:5];
        [invocation invoke];
        __unsafe_unretained id temp = nil;
        [invocation getReturnValue:&temp];
        id<MTLTexture> texture = temp;
        return texture;
    }
    return nil;
}

- (void)setSize:(CGSize)size
{
    id sharedDelegate = SDL_GetSharedDelegate();
    if (!sharedDelegate) {
        return;
    }

    SEL updateSelector = NSSelectorFromString(@"updateSize:");
    if ([sharedDelegate respondsToSelector:updateSelector]) {
        NSMethodSignature *signature = [sharedDelegate methodSignatureForSelector:updateSelector];
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
        [invocation setSelector:updateSelector];
        [invocation setTarget:sharedDelegate];

        [invocation setArgument:&size atIndex:2];
        [invocation invoke];
    }
    _contentSize = size;
}

- (void)setCurvature:(CGFloat)curvature
{
    _curvature = curvature;

    id sharedDelegate = SDL_GetSharedDelegate();
    if (!sharedDelegate) {
        return;
    }

    SEL updateSelector = NSSelectorFromString(@"updateCurvature:");
    if ([sharedDelegate respondsToSelector:updateSelector]) {
        NSMethodSignature *signature = [sharedDelegate methodSignatureForSelector:updateSelector];
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
        [invocation setSelector:updateSelector];
        [invocation setTarget:sharedDelegate];

        float curvatureFloat = (float)curvature;
        [invocation setArgument:&curvatureFloat atIndex:2];
        [invocation invoke];
    }
}

- (void)setSceneSession:(UISceneSession *)session
{
    _sceneSession = session;
    SDL_Log("SDL_UIKitVisionOSScene: Session set: %p", session);
}

@end


// Called from Swift scene delegates when window curvature changes
void SDL_VisionOS_SendCurvatureChanged(CGFloat curvature)
{
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (window) {
        SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
        data.curvature = curvature;
        SDL_SetFloatProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_CREATE_VISIONOS_CURVATURE_FLOAT, curvature);
    }
}

// Called from Swift scene delegates when visionOS delivers a touch event
void SDL_VisionOS_SendImmersiveTouch(NSTimeInterval timestamp, SDL_FingerID fingerID, Uint32 eventType, CGPoint location)
{
    const SDL_TouchID directTouchId = 1;
    SDL_Window **windows = SDL_GetWindows(NULL);
    if (windows) {
        for (int i = 0; windows[i]; ++i) {
            SDL_Window *window = windows[i];
            if (SDL_UIKit_IsImmersiveWindow(window)) {
                float pressure;
                if (eventType == SDL_EVENT_FINGER_DOWN || eventType == SDL_EVENT_FINGER_MOTION) {
                    pressure = 1.0f;
                } else {
                    pressure = 0.0f;
                }
                float x = location.x / window->w;
                float y = location.y / window->h;
                if (eventType == SDL_EVENT_FINGER_MOTION) {
                    SDL_SendTouchMotion(UIKit_GetEventTimestamp(timestamp), directTouchId, fingerID, window, x, y, pressure);
                } else {
                    SDL_SendTouch(UIKit_GetEventTimestamp(timestamp), directTouchId, fingerID, window, (SDL_EventType)eventType, x, y, pressure);
                }
                break;
            }
        }
        SDL_free(windows);
    }
}

// Called from Swift to enter immersive mode
void SDL_VisionOS_EnterImmersiveMode()
{
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (!window) {
        return;
    }

    SDL_UIKitWindowData *windowData = (__bridge SDL_UIKitWindowData *)window->internal;

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "VISIONOS: Failed to create Metal device");
        SDL_SetError("Failed to create Metal device for visionOS rendering");
        return false;
    }

    SDL_UIKitVisionOSScene *scene =
        [[SDL_UIKitVisionOSScene alloc] initWithWindowScene:windowData.uiwindow.windowScene
                                                  curvature:windowData.curvature
                                                       size:CGSizeMake(window->w, window->h)
                                           mainSceneSession:windowData.uiwindow.windowScene.session];

    if (!scene) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "VISIONOS: Failed to create scene");
    }

    windowData.visionOSScene = scene;
    SDL_Log("VISIONOS: Scene created successfully");

    SDL_Log("VISIONOS: Auto-presenting scene now");
    windowData.uiwindow.hidden = YES;
    [scene present];
}

// Called from Swift to leave immersive mode
void SDL_VisionOS_LeaveImmersiveMode()
{
    SDL_Window *window = SDL_GetToplevelForKeyboardFocus();
    if (!window) {
        return;
    }

    SDL_UIKitWindowData *windowData = (__bridge SDL_UIKitWindowData *)window->internal;
    [windowData.visionOSScene dismiss];
    windowData.visionOSScene = nil;
    windowData.uiwindow.hidden = NO;
    [windowData.viewcontroller addOrnaments];
}

bool SDL_UIKit_IsImmersiveWindow(SDL_Window *window)
{
    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;

    return data && data.visionOSScene;
}

id<MTLTexture> SDL_UIKit_GetImmersiveDisplayTexture(SDL_Window *window, id<MTLCommandBuffer> commandBuffer, int width, int height, MTLPixelFormat pixelFormat)
{
    SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
    if (!data || !data.visionOSScene) {
        return;
    }

    return [data.visionOSScene getDisplayTexture:commandBuffer
                                           width:width
                                          height:height
                                     pixelFormat:pixelFormat];
}

#endif /* SDL_PLATFORM_VISIONOS */
