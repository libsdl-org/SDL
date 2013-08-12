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

    UIViewController *splashViewController = [[SDL_splashviewcontroller alloc] init];
    launch_window.rootViewController = splashViewController;
    [launch_window addSubview:splashViewController.view];
    [launch_window makeKeyAndVisible];

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
