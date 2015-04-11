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

#import "SDL_uikitappdelegate.h"
#import "SDL_uikitmodes.h"
#import "SDL_uikitwindow.h"

#include "../../events/SDL_events_c.h"

#ifdef main
#undef main
#endif

static int forward_argc;
static char **forward_argv;
static int exit_status;

int main(int argc, char **argv)
{
    int i;

    /* store arguments */
    forward_argc = argc;
    forward_argv = (char **)malloc((argc+1) * sizeof(char *));
    for (i = 0; i < argc; i++) {
        forward_argv[i] = malloc( (strlen(argv[i])+1) * sizeof(char));
        strcpy(forward_argv[i], argv[i]);
    }
    forward_argv[i] = NULL;

    /* Give over control to run loop, SDLUIKitDelegate will handle most things from here */
    @autoreleasepool {
        UIApplicationMain(argc, argv, nil, [SDLUIKitDelegate getAppDelegateClassName]);
    }

    /* free the memory we used to hold copies of argc and argv */
    for (i = 0; i < forward_argc; i++) {
        free(forward_argv[i]);
    }
    free(forward_argv);

    return exit_status;
}

static void
SDL_IdleTimerDisabledChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    BOOL disable = (hint && *hint != '0');
    [UIApplication sharedApplication].idleTimerDisabled = disable;
}

/* Load a launch image using the old UILaunchImageFile-era naming rules. */
static UIImage *
SDL_LoadLaunchImageNamed(NSString *name, int screenh)
{
    UIInterfaceOrientation curorient = [UIApplication sharedApplication].statusBarOrientation;
    UIUserInterfaceIdiom idiom = [UIDevice currentDevice].userInterfaceIdiom;
    UIImage *image = nil;

    if (idiom == UIUserInterfaceIdiomPhone && screenh == 568) {
        /* The image name for the iPhone 5 uses its height as a suffix. */
        image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-568h", name]];
    } else if (idiom == UIUserInterfaceIdiomPad) {
        /* iPad apps can launch in any orientation. */
        if (UIInterfaceOrientationIsLandscape(curorient)) {
            if (curorient == UIInterfaceOrientationLandscapeLeft) {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-LandscapeLeft", name]];
            } else {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-LandscapeRight", name]];
            }
            if (!image) {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-Landscape", name]];
            }
        } else {
            if (curorient == UIInterfaceOrientationPortraitUpsideDown) {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-PortraitUpsideDown", name]];
            }
            if (!image) {
                image = [UIImage imageNamed:[NSString stringWithFormat:@"%@-Portrait", name]];
            }
        }
    }

    if (!image) {
        image = [UIImage imageNamed:name];
    }

    return image;
}

@implementation SDLLaunchScreenController {
    UIInterfaceOrientationMask supportedOrientations;
}

- (instancetype)init
{
    if (!(self = [super initWithNibName:nil bundle:nil])) {
        return nil;
    }

    NSBundle *bundle = [NSBundle mainBundle];
    NSString *screenname = [bundle objectForInfoDictionaryKey:@"UILaunchStoryboardName"];

    /* Normally we don't want to rotate from the initial orientation. */
    supportedOrientations = (1 << [UIApplication sharedApplication].statusBarOrientation);

    /* Launch screens were added in iOS 8. Otherwise we use launch images. */
    if (screenname && UIKit_IsSystemVersionAtLeast(8.0)) {
        @try {
            self.view = [bundle loadNibNamed:screenname owner:self options:nil][0];
        }
        @catch (NSException *exception) {
            /* iOS displays a blank screen rather than falling back to an image,
             * if a launch screen name is specified but it fails to load. */
            return nil;
        }
    }

    if (!self.view) {
        NSArray *launchimages = [bundle objectForInfoDictionaryKey:@"UILaunchImages"];
        UIInterfaceOrientation curorient = [UIApplication sharedApplication].statusBarOrientation;
        NSString *imagename = nil;
        UIImage *image = nil;

        int screenw = (int)([UIScreen mainScreen].bounds.size.width + 0.5);
        int screenh = (int)([UIScreen mainScreen].bounds.size.height + 0.5);

        /* We always want portrait-oriented size, to match UILaunchImageSize. */
        if (screenw > screenh) {
            int width = screenw;
            screenw = screenh;
            screenh = width;
        }

        /* Xcode 5 introduced a dictionary of launch images in Info.plist. */
        if (launchimages) {
            for (NSDictionary *dict in launchimages) {
                UIInterfaceOrientationMask orientmask = UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;
                NSString *minversion   = dict[@"UILaunchImageMinimumOSVersion"];
                NSString *sizestring   = dict[@"UILaunchImageSize"];
                NSString *orientstring = dict[@"UILaunchImageOrientation"];

                /* Ignore this image if the current version is too low. */
                if (minversion && !UIKit_IsSystemVersionAtLeast(minversion.doubleValue)) {
                    continue;
                }

                /* Ignore this image if the size doesn't match. */
                if (sizestring) {
                    CGSize size = CGSizeFromString(sizestring);
                    if ((int)(size.width + 0.5) != screenw || (int)(size.height + 0.5) != screenh) {
                        continue;
                    }
                }

                if (orientstring) {
                    if ([orientstring isEqualToString:@"PortraitUpsideDown"]) {
                        orientmask = UIInterfaceOrientationMaskPortraitUpsideDown;
                    } else if ([orientstring isEqualToString:@"Landscape"]) {
                        orientmask = UIInterfaceOrientationMaskLandscape;
                    } else if ([orientstring isEqualToString:@"LandscapeLeft"]) {
                        orientmask = UIInterfaceOrientationMaskLandscapeLeft;
                    } else if ([orientstring isEqualToString:@"LandscapeRight"]) {
                        orientmask = UIInterfaceOrientationMaskLandscapeRight;
                    }
                }

                /* Ignore this image if the orientation doesn't match. */
                if ((orientmask & (1 << curorient)) == 0) {
                    continue;
                }

                imagename = dict[@"UILaunchImageName"];
            }

            if (imagename) {
                image = [UIImage imageNamed:imagename];
            }
        } else {
            imagename = [bundle objectForInfoDictionaryKey:@"UILaunchImageFile"];

            if (imagename) {
                image = SDL_LoadLaunchImageNamed(imagename, screenh);
            }

            if (!image) {
                image = SDL_LoadLaunchImageNamed(@"Default", screenh);
            }
        }

        if (image) {
            if (image.size.width > image.size.height) {
                supportedOrientations = UIInterfaceOrientationMaskLandscape;
            } else {
                supportedOrientations = UIInterfaceOrientationMaskPortrait;
            }

            self.view = [[UIImageView alloc] initWithImage:image];
        }
    }

    return self;
}

- (void)loadView
{
    /* Do nothing. */
}

- (NSUInteger)supportedInterfaceOrientations
{
    return supportedOrientations;
}

@end

@implementation SDLUIKitDelegate {
    UIWindow *launchWindow;
}

/* convenience method */
+ (id)sharedAppDelegate
{
    /* the delegate is set in UIApplicationMain(), which is guaranteed to be
     * called before this method */
    return [UIApplication sharedApplication].delegate;
}

+ (NSString *)getAppDelegateClassName
{
    /* subclassing notice: when you subclass this appdelegate, make sure to add
     * a category to override this method and return the actual name of the
     * delegate */
    return @"SDLUIKitDelegate";
}

- (void)hideLaunchScreen
{
    UIWindow *window = launchWindow;

    if (!window || window.hidden) {
        return;
    }

    launchWindow = nil;

    /* Do a nice animated fade-out (roughly matches the real launch behavior.) */
    [UIView animateWithDuration:0.2 animations:^{
        window.alpha = 0.0;
    } completion:^(BOOL finished) {
        window.hidden = YES;
    }];
}

- (void)postFinishLaunch
{
    /* Hide the launch screen the next time the run loop is run. SDL apps will
     * have a chance to load resources while the launch screen is still up. */
    [self performSelector:@selector(hideLaunchScreen) withObject:nil afterDelay:0.0];

    /* run the user's application, passing argc and argv */
    SDL_iPhoneSetEventPump(SDL_TRUE);
    exit_status = SDL_main(forward_argc, forward_argv);
    SDL_iPhoneSetEventPump(SDL_FALSE);

    if (launchWindow) {
        launchWindow.hidden = YES;
        launchWindow = nil;
    }

    /* exit, passing the return status from the user's application */
    /* We don't actually exit to support applications that do setup in their
     * main function and then allow the Cocoa event loop to run. */
    /* exit(exit_status); */
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    NSBundle *bundle = [NSBundle mainBundle];
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

#if SDL_IPHONE_LAUNCHSCREEN
    /* The normal launch screen is displayed until didFinishLaunching returns,
     * but SDL_main is called after that happens and there may be a noticeable
     * delay between the start of SDL_main and when the first real frame is
     * displayed (e.g. if resources are loaded before SDL_GL_SwapWindow is
     * called), so we show the launch screen programmatically until the first
     * time events are pumped. */
    UIViewController *viewcontroller = [[SDLLaunchScreenController alloc] init];

    if (viewcontroller.view) {
        launchWindow = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];

        /* We don't want the launch window immediately hidden when a real SDL
         * window is shown - we fade it out ourselves when we're ready. */
        launchWindow.windowLevel = UIWindowLevelNormal + 1.0;

        /* Show the window but don't make it key. Events should always go to
         * other windows when possible. */
        launchWindow.hidden = NO;

        launchWindow.rootViewController = viewcontroller;
    }
#endif

    /* Set working directory to resource path */
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[bundle resourcePath]];

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

- (void)application:(UIApplication *)application didChangeStatusBarOrientation:(UIInterfaceOrientation)oldStatusBarOrientation
{
    BOOL isLandscape = UIInterfaceOrientationIsLandscape(application.statusBarOrientation);
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this && _this->num_displays > 0) {
        SDL_DisplayMode *desktopmode = &_this->displays[0].desktop_mode;
        SDL_DisplayMode *currentmode = &_this->displays[0].current_mode;

        /* The desktop display mode should be kept in sync with the screen
         * orientation so that updating a window's fullscreen state to
         * SDL_WINDOW_FULLSCREEN_DESKTOP keeps the window dimensions in the
         * correct orientation. */
        if (isLandscape != (desktopmode->w > desktopmode->h)) {
            int height = desktopmode->w;
            desktopmode->w = desktopmode->h;
            desktopmode->h = height;
        }

        /* Same deal with the current mode + SDL_GetCurrentDisplayMode. */
        if (isLandscape != (currentmode->w > currentmode->h)) {
            int height = currentmode->w;
            currentmode->w = currentmode->h;
            currentmode->h = height;
        }
    }
}

- (void)applicationWillResignActive:(UIApplication*)application
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

- (void)applicationDidEnterBackground:(UIApplication*)application
{
    SDL_SendAppEvent(SDL_APP_DIDENTERBACKGROUND);
}

- (void)applicationWillEnterForeground:(UIApplication*)application
{
    SDL_SendAppEvent(SDL_APP_WILLENTERFOREGROUND);
}

- (void)applicationDidBecomeActive:(UIApplication*)application
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
    NSURL *fileURL = url.filePathURL;
    if (fileURL != nil) {
        SDL_SendDropFile([fileURL.path UTF8String]);
    } else {
        SDL_SendDropFile([url.absoluteString UTF8String]);
    }
    return YES;
}

@end

#endif /* SDL_VIDEO_DRIVER_UIKIT */

/* vi: set ts=4 sw=4 expandtab: */
