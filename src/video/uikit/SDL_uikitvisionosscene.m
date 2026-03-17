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
#include "SDL_uikitvolumetric.h"
#include "SDL_uikitevents.h"
#include "../../events/SDL_events_c.h"

// Called from Swift scene delegates when visionOS resizes a scene
void SDL_VisionOS_SendWindowResized(CGSize size)
{
    SDL_Window **windows = SDL_GetWindows(NULL);
    if (windows) {
        for (int i = 0; windows[i]; ++i) {
            SDL_Window *window = windows[i];
            if (SDL_UIKit_IsVolumetricWindow(window)) {
                SDL_UIKitWindowData *data = (__bridge SDL_UIKitWindowData *)window->internal;
                data.uiwindow.frame = CGRectMake(0, 0, size.width, size.height);
                break;
            }
        }
        SDL_free(windows);
    }
}

// Called from Swift scene delegates when visionOS delivers a touch event
void SDL_VisionOS_SendVolumetricTouch(NSTimeInterval timestamp, SDL_FingerID fingerID, Uint32 eventType, CGPoint location)
{
    const SDL_TouchID directTouchId = 1;
    SDL_Window **windows = SDL_GetWindows(NULL);
    if (windows) {
        for (int i = 0; windows[i]; ++i) {
            SDL_Window *window = windows[i];
            if (SDL_UIKit_IsVolumetricWindow(window)) {
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

// Forward declare the Swift delegate class.
@class SDL_VolumetricHostingSceneDelegate;

@interface SDL_UIKitVisionOSScene ()

@property (nonatomic, assign, readwrite) BOOL isPresented;
@property (nonatomic, assign, readwrite) SDL_VisionOSSceneMode mode;
@property (nonatomic, strong) UISceneSession *sceneSession;
@property (nonatomic, assign) CGSize contentSize;
@property (nonatomic, assign) CGFloat curvature;
@property (nonatomic, strong) id<MTLDevice> metalDevice;
@property (nonatomic, strong) UISceneSession *mainSceneSession;

@end

// Returns the ObjC class name and scene ID for the given mode
static void SDL_GetSceneConfig(SDL_VisionOSSceneMode mode,
                               NSString * __autoreleasing *outClassName,
                               NSString * __autoreleasing *outActivateSelector,
                               NSString * __autoreleasing *outDismissSelector)
{
    if (mode == SDL_VisionOSSceneModeImmersive) {
        *outClassName = @"SDL_ImmersiveHostingSceneDelegate";
        *outActivateSelector = @"activateImmersiveSceneWithErrorHandler:";
        *outDismissSelector = @"dismissImmersiveScene";
    } else {
        *outClassName = @"SDL_VolumetricHostingSceneDelegate";
        *outActivateSelector = @"activateVolumetricSceneWithErrorHandler:";
        *outDismissSelector = @"dismissVolumetricScene";
    }
}

static const char *SDL_GetSceneModeName(SDL_VisionOSSceneMode mode)
{
    return (mode == SDL_VisionOSSceneModeImmersive) ? "immersive" : "volumetric";
}

// Helper function to get the Swift hosting delegate class for the given mode
static Class SDL_GetHostingDelegateClass(SDL_VisionOSSceneMode mode)
{
    NSString *className = nil;
    NSString *unused1 = nil;
    NSString *unused2 = nil;
    SDL_GetSceneConfig(mode, &className, &unused1, &unused2);

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
static id SDL_GetSharedDelegate(SDL_VisionOSSceneMode mode)
{
    Class delegateClass = SDL_GetHostingDelegateClass(mode);
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

- (instancetype)initWithMode:(SDL_VisionOSSceneMode)mode
                 windowScene:(UIWindowScene *)windowScene
                 metalDevice:(id<MTLDevice>)device
                   curvature:(CGFloat)curvature
                        size:(CGSize)size
            mainSceneSession:(UISceneSession *)mainSceneSession
{
    self = [super init];
    if (!self) {
        return nil;
    }

    _mode = mode;
    _isPresented = NO;
    _contentSize = size;
    _curvature = curvature;
    _metalDevice = device;
    _mainSceneSession = mainSceneSession;

    SDL_Log("SDL_UIKitVisionOSScene (%s): Initialized with size %.0fx%.0f, curvature %.2f",
            SDL_GetSceneModeName(mode), size.width, size.height, curvature);

    return self;
}

- (void)present
{
    if (_isPresented) {
        SDL_Log("SDL_UIKitVisionOSScene (%s): Already presented", SDL_GetSceneModeName(_mode));
        return;
    }

    const char *modeName = SDL_GetSceneModeName(_mode);
    SDL_Log("SDL_UIKitVisionOSScene (%s): Presenting via UIHostingSceneDelegate bridging", modeName);

    Class delegateClass = SDL_GetHostingDelegateClass(_mode);
    if (!delegateClass) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                    "SDL_UIKitVisionOSScene (%s): Delegate class not found", modeName);
        return;
    }

    // Get the shared Swift scene delegate for configuration
    id sharedDelegate = SDL_GetSharedDelegate(_mode);
    if (!sharedDelegate) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                    "SDL_UIKitVisionOSScene (%s): Failed to get shared delegate", modeName);
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

        SDL_Log("SDL_UIKitVisionOSScene (%s): Configured with size %gx%g and curvature %.2f", modeName, _contentSize.width, _contentSize.height, curvatureFloat);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                   "SDL_UIKitVisionOSScene (%s): Delegate doesn't respond to configure", modeName);
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
                SDL_Log("SDL_UIKitVisionOSScene (%s): Scene connected, session: %p",
                        SDL_GetSceneModeName(strongSelf.mode), session);
            }
        };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
        [delegateClass performSelector:callbackSelector withObject:callback];
#pragma clang diagnostic pop
    }

    if (_mainSceneSession) {
        SDL_Log("SDL_UIKitVisionOSScene (%s): Keeping main scene session alive", modeName);
    }

    // Activate using the UIHostingSceneDelegate bridging API
    NSString *unused1 = nil;
    NSString *activateSelectorName = nil;
    NSString *unused2 = nil;
    SDL_GetSceneConfig(_mode, &unused1, &activateSelectorName, &unused2);

    SEL activateSelector = NSSelectorFromString(activateSelectorName);
    if (![delegateClass respondsToSelector:activateSelector]) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                    "SDL_UIKitVisionOSScene (%s): Delegate doesn't respond to %s",
                    modeName, [activateSelectorName UTF8String]);
        return;
    }

    void (^errorBlock)(NSError *) = ^(NSError *error) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                    "SDL_UIKitVisionOSScene (%s): Activation failed: %s (code: %ld)",
                    SDL_GetSceneModeName(self.mode),
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

    SDL_Log("SDL_UIKitVisionOSScene (%s): Activation call completed", modeName);
}

- (void)dismiss
{
    const char *modeName = SDL_GetSceneModeName(_mode);

    if (!_isPresented) {
        SDL_Log("SDL_UIKitVisionOSScene (%s): Not presented, trying static dismiss as fallback", modeName);
    }

    SDL_Log("SDL_UIKitVisionOSScene (%s): Dismissing", modeName);

    // Use the static dismiss method which tracks the session
    // across SwiftUI-created delegate instances
    Class delegateClass = SDL_GetHostingDelegateClass(_mode);
    if (delegateClass) {
        NSString *unused1 = nil;
        NSString *unused2 = nil;
        NSString *dismissSelectorName = nil;
        SDL_GetSceneConfig(_mode, &unused1, &unused2, &dismissSelectorName);

        SEL dismissSelector = NSSelectorFromString(dismissSelectorName);
        if ([delegateClass respondsToSelector:dismissSelector]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
            [delegateClass performSelector:dismissSelector];
#pragma clang diagnostic pop
            SDL_Log("SDL_UIKitVisionOSScene (%s): Called static dismiss", modeName);
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
    id sharedDelegate = SDL_GetSharedDelegate(_mode);
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
    id sharedDelegate = SDL_GetSharedDelegate(_mode);
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

    id sharedDelegate = SDL_GetSharedDelegate(_mode);
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
    SDL_Log("SDL_UIKitVisionOSScene (%s): Session set: %p", SDL_GetSceneModeName(_mode), session);
}

@end

#endif /* SDL_PLATFORM_VISIONOS */
