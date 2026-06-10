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

#include "../SDL_notification_c.h"

// tvOS doesn't support the notification features SDL cares about.
#ifndef SDL_PLATFORM_TVOS

#include "../../events/SDL_notificationevents_c.h"
#include "../../video/SDL_surface_c.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UNNotification.h>
#import <UserNotifications/UNNotificationAction.h>
#import <UserNotifications/UNNotificationAttachment.h>
#import <UserNotifications/UNNotificationCategory.h>
#import <UserNotifications/UNNotificationContent.h>
#import <UserNotifications/UNNotificationRequest.h>
#import <UserNotifications/UNNotificationResponse.h>
#import <UserNotifications/UNNotificationSettings.h>
#import <UserNotifications/UNNotificationSound.h>
#import <UserNotifications/UNUserNotificationCenter.h>

@interface SDLNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation SDLNotificationDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter *)center willPresentNotification:(UNNotification *)notification withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler
    API_AVAILABLE(macos(10.14))
{
    if (@available(macOS 11, iOS 14, *)) {
        completionHandler(UNNotificationPresentationOptionBanner + UNNotificationPresentationOptionSound);
    } else {
        completionHandler(UNNotificationPresentationOptionAlert + UNNotificationPresentationOptionSound);
    }
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center didReceiveNotificationResponse:(UNNotificationResponse *)response withCompletionHandler:(void (^)(void))completionHandler
    API_AVAILABLE(macos(10.14))
{
    NSString *SDL_Identifier = @"SDL_LocalNotification-";
    NSString *identifier = [[[response notification] request] identifier];
    // const char *identifier = [[[[response notification] request] identifier] UTF8String];
    SDL_NotificationID id = 0;

    if ([identifier compare:SDL_Identifier options:0 range:NSMakeRange(0, [SDL_Identifier length])] == 0) {
        id = (SDL_NotificationID)[[identifier substringFromIndex:[SDL_Identifier length]] integerValue];
    }

    if (id) {
        NSString *action_id = [response actionIdentifier];
        if (action_id) {
            if ([action_id isEqualToString:UNNotificationDefaultActionIdentifier]) {
                SDL_SendNotificationAction(id, "default");
            } else {
                if ([action_id length] != 0) {
                    SDL_SendNotificationAction(id, [action_id UTF8String]);
                }
            }
        }
    }

    completionHandler();
}
@end

API_AVAILABLE(macos(10.14))
static UNUserNotificationCenter *center;
static SDLNotificationDelegate *delegate;

static bool ShouldEnableNotifications()
{
#if defined(SDL_PLATFORM_MACOS)
    /* Notifications outside of an app bundle are unsupported, and will crash with an
     * unhandled exception error, deep within a system library.
     *
     * FIXME: These functions are deprecated, find a modern way.
     */
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFURLRef bundleUrl = CFBundleCopyBundleURL(bundle);

    CFStringRef uti;
    if (CFURLCopyResourcePropertyForKey(bundleUrl, kCFURLTypeIdentifierKey, &uti, NULL) &&
        uti && UTTypeConformsTo(uti, kUTTypeApplicationBundle)) {
        return true;
    }

    return false;
#else
    // iOS can always enable notificaions.
    return true;
#endif
}

static NSURL *SaveTempImage(SDL_Surface *image)
{
    @autoreleasepool {
        const UInt32 hash = SDL_murmur3_32(image->pixels, image->pitch * image->h, 0);
        NSString *tempFileName = [NSString stringWithFormat:@"SDL_tmpimage-%u.png", hash];
        NSString *tempFilePath = [NSTemporaryDirectory() stringByAppendingPathComponent:tempFileName];

        if (!SDL_SavePNG(image, [tempFilePath fileSystemRepresentation])) {
            return nil;
        }

        return [NSURL fileURLWithPath:tempFilePath];
    }
}

void Cocoa_RegisterNotificationDelegate(void)
{
    if (!ShouldEnableNotifications()) {
        return;
    }

    if (@available(macOS 10.14, *)) {
        @autoreleasepool {
            if (!center) {
                center = [UNUserNotificationCenter currentNotificationCenter];
            }
            if (!delegate) {
                delegate = [SDLNotificationDelegate new];
                [center setDelegate:delegate];
            }
        }
    }
}

bool SDL_RequestNotificationPermission(void)
{
    @autoreleasepool {
        if (@available(macOS 10.14, *)) {
            // Notifications not initialized (not in a bundle).
            if (!center) {
                return SDL_SetError("macOS notifications not supported outside an application bundle");
            }

            // Check authorization to send notifications, and request it if necessary.
            [center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings *_Nonnull settings) {
              if (settings.authorizationStatus == UNAuthorizationStatusNotDetermined) {
                  UNAuthorizationOptions options = UNAuthorizationOptionAlert + UNAuthorizationOptionSound;
                  [center requestAuthorizationWithOptions:options
                                        completionHandler:^(BOOL granted, NSError *_Nullable error) {}];
              }
            }];

            return true;
        } else {
            SDL_SetError("Notifications require macOS 10.14 or higher");
            return false;
        }
    }

    return false;
}

bool SDL_RemoveNotification(SDL_NotificationID notification)
{
    @autoreleasepool {
        if (@available(macOS 10.14, *)) {
            // Notifications not initialized (not in a bundle).
            if (!center) {
                return SDL_SetError("macOS notifications not supported outside an application bundle");
            }

            NSString *identifier = [NSString stringWithFormat:@"SDL_LocalNotification-%u", notification];
            [center removePendingNotificationRequestsWithIdentifiers:@[ identifier ]];
            [center removeDeliveredNotificationsWithIdentifiers:@[ identifier ]];
        }
    }

    return true;
}

SDL_NotificationID SDL_SYS_ShowNotification(SDL_PropertiesID props)
{
    @autoreleasepool {
        if (@available(macOS 10.14, *)) {
            if (!SDL_RequestNotificationPermission()) {
                return false;
            }

            // Get the notification properties.
            const char *title = SDL_GetStringProperty(props, SDL_PROP_NOTIFICATION_TITLE_STRING, NULL);
            const char *message = SDL_GetStringProperty(props, SDL_PROP_NOTIFICATION_MESSAGE_STRING, "");
            const char *sound = SDL_GetStringProperty(props, SDL_PROP_NOTIFICATION_SOUND_STRING, "default");
            SDL_Surface *image = SDL_GetPointerProperty(props, SDL_PROP_NOTIFICATION_IMAGE_POINTER, NULL);
            const SDL_NotificationID replaces = (SDL_NotificationID)SDL_GetNumberProperty(props, SDL_PROP_NOTIFICATION_REPLACES_NUMBER, 0);
            const SDL_NotificationPriority priority = (SDL_NotificationPriority)SDL_GetNumberProperty(props, SDL_PROP_NOTIFICATION_PRIORITY_NUMBER, SDL_NOTIFICATION_PRIORITY_NORMAL);
            const SDL_NotificationAction *sdlactions = SDL_GetPointerProperty(props, SDL_PROP_NOTIFICATION_ACTIONS_POINTER, NULL);
            const int num_sdlactions = (int)SDL_GetNumberProperty(props, SDL_PROP_NOTIFICATION_ACTION_COUNT_NUMBER, 0);
            const bool transient = SDL_GetBooleanProperty(props, SDL_PROP_NOTIFICATION_TRANSIENT_BOOLEAN, false);

            // Generate a new ID.
            Uint32 new_id;
            if (replaces) {
                new_id = replaces;
            } else if (SecRandomCopyBytes(kSecRandomDefault, sizeof(new_id), &new_id) != errSecSuccess) {
                new_id = (Uint32)SDL_GetTicksNS();
            }

            // Build the action array.
            NSMutableArray *actions = nil;
            if (sdlactions && num_sdlactions) {
                actions = [NSMutableArray array];
                for (int i = 0; i < num_sdlactions; ++i) {
                    if (sdlactions[i].type == SDL_NOTIFICATION_ACTION_TYPE_BUTTON) {
                        UNNotificationAction *action = [UNNotificationAction actionWithIdentifier:[NSString stringWithUTF8String:sdlactions[i].button.action_id]
                                                                                            title:[NSString stringWithUTF8String:sdlactions[i].button.action_label]
                                                                                          options:UNNotificationActionOptionNone];
                        actions[i] = action;
                    }
                }
            }

            // Create the category.
            NSString *category_id = nil;
            if (actions && [actions count]) {
                // Create the notification category.
                category_id = [[NSUUID new] UUIDString];
                UNNotificationCategory *category = [UNNotificationCategory categoryWithIdentifier:category_id
                                                                                          actions:actions
                                                                                intentIdentifiers:@[]
                                                                                          options:UNNotificationCategoryOptionNone];
                NSSet *categories = [NSSet setWithObject:category];
                [center setNotificationCategories:categories];
            }

            // Configure the content.
            UNMutableNotificationContent *content = [UNMutableNotificationContent new];
            content.title = [NSString stringWithUTF8String:title];
            content.body = [NSString stringWithUTF8String:message];
            content.categoryIdentifier = category_id;

            if (SDL_strcmp(sound, "default") == 0) {
                if (priority == SDL_NOTIFICATION_PRIORITY_CRITICAL) {
                    // defaultCriticalSound is only in iOS 12+
                    if (@available(iOS 12, *)) {
                        content.sound = [UNNotificationSound defaultCriticalSound];
                    } else {
                        content.sound = [UNNotificationSound defaultSound];
                    }
                } else {
                    content.sound = [UNNotificationSound defaultSound];
                }
            } else if (SDL_strcmp(sound, "silent") != 0) {
                if (priority == SDL_NOTIFICATION_PRIORITY_CRITICAL) {
                    if (@available(iOS 12, *)) {
                        content.sound = [UNNotificationSound criticalSoundNamed:[NSString stringWithUTF8String:sound]];
                    } else {
                        content.sound = [UNNotificationSound soundNamed:[NSString stringWithUTF8String:sound]];
                    }
                } else {
                    content.sound = [UNNotificationSound soundNamed:[NSString stringWithUTF8String:sound]];
                }
            }

            if (@available(macOS 12, iOS 15, *)) {
                switch (priority) {
                case SDL_NOTIFICATION_PRIORITY_LOW:
                    content.interruptionLevel = UNNotificationInterruptionLevelPassive;
                    break;
                case SDL_NOTIFICATION_PRIORITY_CRITICAL:
                    content.interruptionLevel = UNNotificationInterruptionLevelCritical;
                    break;
                case SDL_NOTIFICATION_PRIORITY_NORMAL:
                case SDL_NOTIFICATION_PRIORITY_HIGH:
                default:
                    content.interruptionLevel = UNNotificationInterruptionLevelActive;
                    break;
                }
            }

            // Notifications load images from file paths, so save it to a temporary location.
            if (image) {
                NSURL *url = SaveTempImage(image);
                if (url) {
                    UNNotificationAttachment *attach = [UNNotificationAttachment attachmentWithIdentifier:@"" URL:url options:nil error:nil];
                    content.attachments = @[ attach ];
                }
            }

            NSString *identifier = [NSString stringWithFormat:@"SDL_LocalNotification-%u", new_id];
            UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:identifier
                                                                                  content:content
                                                                                  trigger:nil];

            [center addNotificationRequest:request
                     withCompletionHandler:^(NSError *_Nullable error) {
                       if (error != nil) {
                           SDL_SetError("Failed to show notification");
                       }
                     }];
            
            /* There is no way to tell if the notification was ignored or wound up
             * in the notification center, so just remove transient notifications
             * after a brief period via a timer.
             */
            if (transient) {
                [NSTimer scheduledTimerWithTimeInterval:7 repeats:NO block:^(NSTimer *timer) {
                    SDL_RemoveNotification(new_id);
                }];
            }

            return new_id;
        } else {
            SDL_SetError("macOS 10.14+ required for notifications");
        }

        return 0;
    }
}
#else

// Notifications on tvOS are just for updating badges, and are of no use here.
SDL_NotificationID SDL_SYS_ShowNotification(SDL_PropertiesID props)
{
    SDL_SetError("Notifications not supported on tvOS");
    return 0;
}

void Cocoa_RegisterNotificationDelegate()
{
}

bool SDL_RequestNotificationPermission(void)
{
    return SDL_SetError("Notifications not supported on tvOS");
}

bool SDL_RemoveNotification(SDL_NotificationID notification)
{
    return SDL_SetError("Notifications not supported on tvOS");
}
#endif

void SDL_CleanupNotifications()
{
    // TODO: Anything to do here?
}
