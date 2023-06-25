/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_COCOA

#include "SDL_cocoavideo.h"
#include "../../events/SDL_clipboardevents_c.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101300
typedef NSString *NSPasteboardType; /* Defined in macOS 10.13+ */
#endif

@interface Cocoa_PasteboardDataProvider : NSObject<NSPasteboardItemDataProvider>
{
    SDL_ClipboardDataCallback m_callback;
    void *m_userdata;
}
@end

@implementation Cocoa_PasteboardDataProvider

- (nullable instancetype)initWith:(SDL_ClipboardDataCallback)callback
                         userData:(void *)userdata
{
    self = [super init];
    if (!self) {
        return self;
    }
    m_callback = callback;
    m_userdata = userdata;
    return self;
}

- (void)pasteboard:(NSPasteboard *)pasteboard
              item:(NSPasteboardItem *)item
provideDataForType:(NSPasteboardType)type
{
    @autoreleasepool {
        size_t size = 0;
        CFStringRef mimeType;
        void *callbackData;
        NSData *data;
        mimeType = UTTypeCopyPreferredTagWithClass((__bridge CFStringRef)type, kUTTagClassMIMEType);
        callbackData = m_callback(&size, [(__bridge NSString *)mimeType UTF8String], m_userdata);
        CFRelease(mimeType);
        if (callbackData == NULL || size == 0) {
            return;
        }
        data = [NSData dataWithBytes: callbackData length: size];
        [item setData: data forType: type];
    }
}

@end


int Cocoa_SetClipboardText(SDL_VideoDevice *_this, const char *text)
{
    @autoreleasepool {
        SDL_CocoaVideoData *data = (__bridge SDL_CocoaVideoData *)_this->driverdata;
        NSPasteboard *pasteboard;
        NSString *format = NSPasteboardTypeString;
        NSString *nsstr = [NSString stringWithUTF8String:text];
        if (nsstr == nil) {
            return SDL_SetError("Couldn't create NSString; is your string data in UTF-8 format?");
        }

        pasteboard = [NSPasteboard generalPasteboard];
        data.clipboard_count = [pasteboard declareTypes:[NSArray arrayWithObject:format] owner:nil];
        [pasteboard setString:nsstr forType:format];

        return 0;
    }
}

char *Cocoa_GetClipboardText(SDL_VideoDevice *_this)
{
    @autoreleasepool {
        NSPasteboard *pasteboard;
        NSString *format = NSPasteboardTypeString;
        NSString *available;
        char *text;

        pasteboard = [NSPasteboard generalPasteboard];
        available = [pasteboard availableTypeFromArray:[NSArray arrayWithObject:format]];
        if ([available isEqualToString:format]) {
            NSString *string;
            const char *utf8;

            string = [pasteboard stringForType:format];
            if (string == nil) {
                utf8 = "";
            } else {
                utf8 = [string UTF8String];
            }
            text = SDL_strdup(utf8 ? utf8 : "");
        } else {
            text = SDL_strdup("");
        }

        return text;
    }
}

SDL_bool Cocoa_HasClipboardText(SDL_VideoDevice *_this)
{
    SDL_bool result = SDL_FALSE;
    char *text = Cocoa_GetClipboardText(_this);
    if (text) {
        result = text[0] != '\0' ? SDL_TRUE : SDL_FALSE;
        SDL_free(text);
    }
    return result;
}

void Cocoa_CheckClipboardUpdate(SDL_CocoaVideoData *data)
{
    @autoreleasepool {
        NSPasteboard *pasteboard;
        NSInteger count;

        pasteboard = [NSPasteboard generalPasteboard];
        count = [pasteboard changeCount];
        if (count != data.clipboard_count) {
            if (data.clipboard_count) {
                SDL_SendClipboardUpdate();
            }
            data.clipboard_count = count;
        }
    }
}

void *Cocoa_GetClipboardData(SDL_VideoDevice *_this, size_t *len, const char *mime_type)
{
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSData *itemData;
        void *data;
        *len = 0;
        for (NSPasteboardItem *item in [pasteboard pasteboardItems]) {
            CFStringRef mimeType = CFStringCreateWithCString(NULL, mime_type, kCFStringEncodingUTF8);
            CFStringRef utiType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeType, NULL);
            CFRelease(mimeType);
            itemData = [item dataForType: (__bridge NSString *)utiType];
            CFRelease(utiType);
            if (itemData != nil) {
                *len = (size_t)[itemData length];
                data = SDL_malloc(*len);
                [itemData getBytes: data length: *len];
                return data;
            }
        }
        return nil;
    }
}

SDL_bool Cocoa_HasClipboardData(SDL_VideoDevice *_this, const char *mime_type)
{

    SDL_bool result = SDL_FALSE;
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        CFStringRef mimeType = CFStringCreateWithCString(NULL, mime_type, kCFStringEncodingUTF8);
        CFStringRef utiType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeType, NULL);
        CFRelease(mimeType);
        if ([pasteboard canReadItemWithDataConformingToTypes: @[(__bridge NSString *)utiType]]) {
            result = SDL_TRUE;
        }
        CFRelease(utiType);
    }
    return result;

}

int Cocoa_SetClipboardData(SDL_VideoDevice *_this, SDL_ClipboardDataCallback callback, size_t mime_count,
                           const char **mime_types, void *userdata)
{
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSPasteboardItem *newItem = [NSPasteboardItem new];
        NSMutableArray *utiTypes = [NSMutableArray new];
        Cocoa_PasteboardDataProvider *provider = [[Cocoa_PasteboardDataProvider alloc] initWith: callback userData: userdata];
        BOOL itemResult = FALSE;
        BOOL writeResult = FALSE;
        SDL_CocoaVideoData *data = (__bridge SDL_CocoaVideoData *)_this->driverdata;

        for (int i = 0; i < mime_count; i++) {
            CFStringRef mimeType = CFStringCreateWithCString(NULL, mime_types[i], kCFStringEncodingUTF8);
            CFStringRef utiType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeType, NULL);
            CFRelease(mimeType);

            [utiTypes addObject: (__bridge NSString *)utiType];
            CFRelease(utiType);
        }
        itemResult = [newItem setDataProvider: provider forTypes: utiTypes];
        if (itemResult == FALSE) {
            return SDL_SetError("Unable to set clipboard item data");
        }

        [pasteboard clearContents];
        writeResult = [pasteboard writeObjects: @[newItem]];
        if (writeResult == FALSE) {
            return SDL_SetError("Unable to set clipboard data");
        }
        data.clipboard_count = [pasteboard changeCount];
    }
    return 0;
}

#endif /* SDL_VIDEO_DRIVER_COCOA */
