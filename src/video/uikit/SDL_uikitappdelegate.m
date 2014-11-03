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

#include "../SDL_sysvideo.h"
#include "SDL_assert.h"
#include "SDL_hints.h"
#include "SDL_system.h"
#include "SDL_main.h"

#include "SDL_uikitappdelegate.h"
#include "SDL_uikitmodes.h"
#include "../../events/SDL_events_c.h"

#ifdef main
#undef main
#endif

static int forward_argc;
static char **forward_argv;
static int exit_status;
static UIWindow *launch_window;

int main(int argc, char **argv)
{
    int i;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* store arguments */
    forward_argc = argc;
    forward_argv = (char **)malloc((argc+1) * sizeof(char *));
    for (i = 0; i < argc; i++) {
        forward_argv[i] = malloc( (strlen(argv[i])+1) * sizeof(char));
        strcpy(forward_argv[i], argv[i]);
    }
    forward_argv[i] = NULL;

    /* Give over control to run loop, SDLUIKitDelegate will handle most things from here */
    UIApplicationMain(argc, argv, NULL, [SDLUIKitDelegate getAppDelegateClassName]);

    /* free the memory we used to hold copies of argc and argv */
    for (i = 0; i < forward_argc; i++) {
        free(forward_argv[i]);
    }
    free(forward_argv);

    [pool release];
    return exit_status;
}

static void
SDL_IdleTimerDisabledChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    BOOL disable = (hint && *hint != '0');
    [UIApplication sharedApplication].idleTimerDisabled = disable;
}


@interface SDL_launchscreenviewcontroller : UIViewController {
	
}

@end

@implementation SDL_launchscreenviewcontroller

- (id)init
{
    self = [super init];
    if (self == nil) {
        return nil;
    }

    NSString* launch_screen_name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UILaunchStoryboardName"];

    if(launch_screen_name) {
        // TODO: If the NIB is not in the bundle, this will throw an exception. We might consider a pre-emptive check, but returning a useless viewcontroller isn't helpful and the check should be outside.
        UIView* launch_screen = [[[NSBundle mainBundle] loadNibNamed:launch_screen_name owner:self options:nil] objectAtIndex:0];
        CGSize size = [UIScreen mainScreen].bounds.size;
        
        CGRect bounds = CGRectMake(0, 0, size.width, size.height);
        
        [launch_screen setFrame:bounds];
        [self setView:launch_screen];
        [launch_screen release];
    }

    


    return self;
}

- (NSUInteger)supportedInterfaceOrientations
{
    NSUInteger orientationMask = UIInterfaceOrientationMaskAll;
    
    /* Don't allow upside-down orientation on the phone, so answering calls is in the natural orientation */
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        orientationMask &= ~UIInterfaceOrientationMaskPortraitUpsideDown;
    }
    return orientationMask;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orient
{
    NSUInteger orientationMask = [self supportedInterfaceOrientations];
    return (orientationMask & (1 << orient));
}

@end


@interface SDL_splashviewcontroller : UIViewController {
    UIImageView *splash;
    UIImage *splashPortrait;
    UIImage *splashLandscape;
}

- (void)updateSplashImage:(UIInterfaceOrientation)interfaceOrientation;
@end

@implementation SDL_splashviewcontroller

- (id)init
{
    self = [super init];
    if (self == nil) {
        return nil;
    }

    self->splash = [[UIImageView alloc] init];
    [self setView:self->splash];

    CGSize size = [UIScreen mainScreen].bounds.size;
    float height = SDL_max(size.width, size.height);
    /* FIXME: Some where around iOS 7, UILaunchImages in the Info.plist was introduced which explicitly maps image names to devices and orientations.
     This gets rid of the hardcoded magic file names and allows more control for OS version, orientation, retina, and device.
     But this existing code needs to be modified to look in the Info.plist for each key and act appropriately for the correct iOS version.
     But iOS 8 superscedes this process and introduces the LaunchScreen NIB which uses autolayout to handle all orientations and devices.
     Since we now have a LaunchScreen solution, this may never get fixed, 
     but this note is here for anybody trying to debug their program on iOS 7 and doesn't understand why their Info.plist isn't working.
     */
    self->splashPortrait = [UIImage imageNamed:[NSString stringWithFormat:@"Default-%dh.png", (int)height]];
    if (!self->splashPortrait) {
        self->splashPortrait = [UIImage imageNamed:@"Default.png"];
    }
    self->splashLandscape = [UIImage imageNamed:@"Default-Landscape.png"];
    if (!self->splashLandscape && self->splashPortrait) {
        self->splashLandscape = [[UIImage alloc] initWithCGImage: self->splashPortrait.CGImage
                                                           scale: 1.0
                                                     orientation: UIImageOrientationRight];
    }
    if (self->splashPortrait) {
        [self->splashPortrait retain];
    }
    if (self->splashLandscape) {
        [self->splashLandscape retain];
    }

    [self updateSplashImage:[[UIApplication sharedApplication] statusBarOrientation]];

    return self;
}

- (NSUInteger)supportedInterfaceOrientations
{
    NSUInteger orientationMask = UIInterfaceOrientationMaskAll;

    /* Don't allow upside-down orientation on the phone, so answering calls is in the natural orientation */
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        orientationMask &= ~UIInterfaceOrientationMaskPortraitUpsideDown;
    }
    return orientationMask;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orient
{
    NSUInteger orientationMask = [self supportedInterfaceOrientations];
    return (orientationMask & (1 << orient));
}

- (void)willAnimateRotationToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation duration:(NSTimeInterval)duration
{
    [self updateSplashImage:interfaceOrientation];
}

- (void)updateSplashImage:(UIInterfaceOrientation)interfaceOrientation
{
    UIImage *image;

    if (UIInterfaceOrientationIsLandscape(interfaceOrientation)) {
        image = self->splashLandscape;
    } else {
        image = self->splashPortrait;
    }
    if (image)
    {
        splash.image = image;
    }
}

@end


@implementation SDLUIKitDelegate

/* convenience method */
+ (id) sharedAppDelegate
{
    /* the delegate is set in UIApplicationMain(), which is garaunteed to be called before this method */
    return [[UIApplication sharedApplication] delegate];
}

+ (NSString *)getAppDelegateClassName
{
    /* subclassing notice: when you subclass this appdelegate, make sure to add a category to override
       this method and return the actual name of the delegate */
    return @"SDLUIKitDelegate";
}

- (id)init
{
    self = [super init];
    return self;
}

- (void)postFinishLaunch
{
    /* run the user's application, passing argc and argv */
    SDL_iPhoneSetEventPump(SDL_TRUE);
    exit_status = SDL_main(forward_argc, forward_argv);
    SDL_iPhoneSetEventPump(SDL_FALSE);

    /* If we showed a splash image, clean it up */
    if (launch_window) {
        [launch_window release];
        launch_window = NULL;
    }

    /* exit, passing the return status from the user's application */
    /* We don't actually exit to support applications that do setup in
     * their main function and then allow the Cocoa event loop to run.
     */
    /* exit(exit_status); */
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    /* Keep the launch image up until we set a video mode */
    launch_window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

    /* iOS 8 introduces LaunchScreen NIBs which use autolayout to handle all devices and orientations with a single NIB instead of multiple launch images.
     This is also the only way to get the App Store badge "Optimized for iPhone 6 and iPhone 6 Plus".
     So if the application is running on iOS 8 or greater AND has specified a LaunchScreen in their Info.plist, we should use the LaunchScreen NIB.
     Otherwise, we should fallback to the legacy behavior of launch screen images.
     */
    NSString* launch_screen_name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UILaunchStoryboardName"];
    if( ([[UIDevice currentDevice].systemVersion intValue] >= 8) && (nil != launch_screen_name) ) {
        // iOS 8.0 and above uses LaunchScreen.xib
        SDL_launchscreenviewcontroller* launch_screen_view_controller = [[SDL_launchscreenviewcontroller alloc] init];
        launch_window.rootViewController = launch_screen_view_controller;
        [launch_window addSubview:launch_screen_view_controller.view];
        [launch_window makeKeyAndVisible];
    } else {
        // Anything less than iOS 8.0

        UIViewController *splashViewController = [[SDL_splashviewcontroller alloc] init];
        launch_window.rootViewController = splashViewController;
        [launch_window addSubview:splashViewController.view];
        [launch_window makeKeyAndVisible];
    }

    /* Set working directory to resource path */
    [[NSFileManager defaultManager] changeCurrentDirectoryPath: [[NSBundle mainBundle] resourcePath]];

    /* register a callback for the idletimer hint */
    SDL_AddHintCallback(SDL_HINT_IDLE_TIMER_DISABLED,
                        SDL_IdleTimerDisabledChanged, NULL);

    SDL_SetMainReady();
    [self performSelector:@selector(postFinishLaunch) withObject:nil afterDelay:0.0];

    return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    SDL_SendAppEvent(SDL_APP_TERMINATING);
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
    SDL_SendAppEvent(SDL_APP_LOWMEMORY);
}

- (void) applicationWillResignActive:(UIApplication*)application
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this) {
        SDL_Window *window;
        for (window = _this->windows; window != nil; window = window->next) {
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_LOST, 0, 0);
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
        }
    }
    SDL_SendAppEvent(SDL_APP_WILLENTERBACKGROUND);
}

- (void) applicationDidEnterBackground:(UIApplication*)application
{
    SDL_SendAppEvent(SDL_APP_DIDENTERBACKGROUND);
}

- (void) applicationWillEnterForeground:(UIApplication*)application
{
    SDL_SendAppEvent(SDL_APP_WILLENTERFOREGROUND);
}

- (void) applicationDidBecomeActive:(UIApplication*)application
{
    SDL_SendAppEvent(SDL_APP_DIDENTERFOREGROUND);

    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this) {
        SDL_Window *window;
        for (window = _this->windows; window != nil; window = window->next) {
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESTORED, 0, 0);
        }
    }
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
    NSURL *fileURL = [url filePathURL];
    if (fileURL != nil) {
        SDL_SendDropFile([[fileURL path] UTF8String]);
    } else {
        SDL_SendDropFile([[url absoluteString] UTF8String]);
    }
    return YES;
}

@end

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
