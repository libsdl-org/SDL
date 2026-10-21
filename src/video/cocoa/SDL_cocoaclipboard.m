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

#ifdef SDL_VIDEO_DRIVER_COCOA

#include "SDL_cocoavideo.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_clipboardevents_c.h"

#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

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
        const void *callbackData;
        NSData *data;
        mimeType = UTTypeCopyPreferredTagWithClass((__bridge CFStringRef)type, kUTTagClassMIMEType);
        callbackData = m_callback(m_userdata, [(__bridge NSString *)mimeType UTF8String], &size);
        CFRelease(mimeType);
        if (callbackData == NULL || size == 0) {
            return;
        }
        data = [NSData dataWithBytes: callbackData length: size];
        [item setData: data forType: type];
    }
}

@end

static void *GetTextURIsFromURLs(NSPasteboard *pasteboard, size_t *size)
{
    @autoreleasepool {
        NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey: @YES};
        NSArray<NSURL *> *urls = [pasteboard readObjectsForClasses:@[[NSURL class]] options:options];
        NSMutableString *string = [NSMutableString string];

        for (NSURL *url in urls) {
            [string appendFormat:@"%@\r\n", [[url filePathURL] absoluteString]];
        }

        if ([string length]) {
            // Copy to the destination, trimming the trailing '/r/n'.
            *size = [string length] - 1;
            char *buf = SDL_malloc(*size);
            if (buf) {
                SDL_strlcpy(buf, [string UTF8String], *size);
                return buf;
            }
        }

        return NULL;
    }
}

static void SetURLsFromTextURIs(SDL_VideoDevice *_this, NSPasteboard *pasteboard)
{
    @autoreleasepool {
        size_t size = 0;
        const char *urls = _this->clipboard_callback(_this->clipboard_userdata, "text/uri-list", &size);
        if (urls) {
            char *str = SDL_strdup(urls);
            if (str) {
                NSMutableArray *array = [NSMutableArray new];
                char *saveptr = NULL;
                char *token = SDL_strtok_r(str, "\r\n", &saveptr);
                while (token) {
                    NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:token]];
                    if (url && [url isFileURL]) {
                        [array addObject:url];
                    }
                    token = SDL_strtok_r(NULL, "\r\n", &saveptr);
                }
                [pasteboard writeObjects:array];
                SDL_free(str);
            }
        }
    }
}

static char **GetMimeTypes(int *pnformats)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    char **new_mime_types = NULL;

    *pnformats = 0;

    int nformats = 0;
    int formatsSz = 0;
    NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey : @YES};
    const bool has_urls = [pasteboard canReadObjectForClasses:@[ [NSURL class] ] options:options];
    NSArray<NSPasteboardItem *> *items = [pasteboard pasteboardItems];
    bool has_text_uri = false;
    NSUInteger nitems = [items count];
    if (nitems > 0) {
        for (NSPasteboardItem *item in items) {
            NSArray<NSString *> *types = [item types];
            for (NSString *type in types) {
                if (@available(macOS 11.0, *)) {
                    UTType *uttype = [UTType typeWithIdentifier:type];
                    NSString *mime_type = [uttype preferredMIMEType];
                    if (mime_type) {
                        NSUInteger len = [mime_type lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
                        formatsSz += len;
                        ++nformats;
                        if (!has_text_uri && [mime_type isEqualToString:@"text/uri-list"]) {
                            has_text_uri = true;
                        }
                    }
                }
                NSUInteger len = [type lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
                formatsSz += len;
                ++nformats;
            }
        }

        // Synthesize the "text/uri-list" type from URLs, if only the latter is present.
        if (has_urls && !has_text_uri) {
            NSUInteger len = [@"text/uri-list" lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
            formatsSz += len;
            ++nformats;
        }

        new_mime_types = SDL_AllocateTemporaryMemory((nformats + 1) * sizeof(char *) + formatsSz);
        if (new_mime_types) {
            int i = 0;
            char *strPtr = (char *)(new_mime_types + nformats + 1);
            for (NSPasteboardItem *item in items) {
                NSArray<NSString *> *types = [item types];
                for (NSString *type in types) {
                    if (@available(macOS 11.0, *)) {
                        UTType *uttype = [UTType typeWithIdentifier:type];
                        NSString *mime_type = [uttype preferredMIMEType];
                        if (mime_type) {
                            NSUInteger len = [mime_type lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
                            SDL_memcpy(strPtr, [mime_type UTF8String], len);
                            new_mime_types[i++] = strPtr;
                            strPtr += len;
                        }
                    }
                    NSUInteger len = [type lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
                    SDL_memcpy(strPtr, [type UTF8String], len);
                    new_mime_types[i++] = strPtr;
                    strPtr += len;
                }
            }

            if (has_urls && !has_text_uri) {
                NSString *text_uri_list = @"text/uri-list";
                NSUInteger len = [text_uri_list lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
                SDL_memcpy(strPtr, [text_uri_list UTF8String], len);
                new_mime_types[i++] = strPtr;
                strPtr += len;
            }

            new_mime_types[nformats] = NULL;
            *pnformats = nformats;
        }
    }
    return new_mime_types;
}


void Cocoa_CheckClipboardUpdate(SDL_CocoaVideoData *data)
{
    @autoreleasepool {
        NSPasteboard *pasteboard;
        NSInteger count;

        pasteboard = [NSPasteboard generalPasteboard];
        count = [pasteboard changeCount];
        if (count != data.clipboard_count) {
            if (count) {
                int nformats = 0;
                char **new_mime_types = GetMimeTypes(&nformats);
                if (new_mime_types) {
                    SDL_SendClipboardUpdate(false, new_mime_types, nformats);
                }
            }
            data.clipboard_count = count;
        }
    }
}

bool Cocoa_SetClipboardData(SDL_VideoDevice *_this)
{
    @autoreleasepool {
        SDL_CocoaVideoData *data = (__bridge SDL_CocoaVideoData *)_this->internal;
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

        // SetClipboardText specialization so text is available after the app quits
        if (_this->clipboard_callback && _this->num_clipboard_mime_types == 1) {
            if (SDL_strncmp(_this->clipboard_mime_types[0], "text/plain;charset=utf-8", 24) == 0) {
                [pasteboard declareTypes:@[ NSPasteboardTypeString ] owner:nil];
                [pasteboard setString:@((char *)_this->clipboard_userdata) forType:NSPasteboardTypeString];
                data.clipboard_count = [pasteboard changeCount];
                return true;
            }
        }

        NSPasteboardItem *newItem = [NSPasteboardItem new];
        NSMutableArray *utiTypes = [NSMutableArray new];
        Cocoa_PasteboardDataProvider *provider = [[Cocoa_PasteboardDataProvider alloc] initWith: _this->clipboard_callback userData: _this->clipboard_userdata];
        BOOL itemResult = FALSE;
        BOOL writeResult = FALSE;
        BOOL appendURLs = FALSE;

        if (_this->clipboard_callback) {
            for (int i = 0; i < _this->num_clipboard_mime_types; i++) {
                CFStringRef mimeType = CFStringCreateWithCString(NULL, _this->clipboard_mime_types[i], kCFStringEncodingUTF8);
                CFStringRef utiType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeType, NULL);
                CFRelease(mimeType);

                [utiTypes addObject: (__bridge NSString *)utiType];
                CFRelease(utiType);

                if (SDL_strcmp(_this->clipboard_mime_types[i], "text/uri-list") == 0) {
                    appendURLs = TRUE;
                }
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
            if (appendURLs) {
                SetURLsFromTextURIs(_this, pasteboard);
            }
        } else {
            [pasteboard clearContents];
        }
        data.clipboard_count = [pasteboard changeCount];
    }
    return true;
}

static bool IsMimeType(const char *tag)
{
    if (SDL_strchr(tag, '/')) {
        // MIME types have slashes
        return true;
    } else if (SDL_strchr(tag, '.')) {
        // UTI identifiers have periods
        return false;
    } else {
        // Not sure, but it's not a UTI identifier
        return true;
    }
}

static CFStringRef GetUTIType(const char *tag)
{
    CFStringRef utiType;
    if (IsMimeType(tag)) {
        CFStringRef mimeType = CFStringCreateWithCString(NULL, tag, kCFStringEncodingUTF8);
        utiType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeType, NULL);
        CFRelease(mimeType);
    } else {
        utiType = CFStringCreateWithCString(NULL, tag, kCFStringEncodingUTF8);
    }
    return utiType;
}

void *Cocoa_GetClipboardData(SDL_VideoDevice *_this, const char *mime_type, size_t *size)
{
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        void *data = NULL;
        *size = 0;
        for (NSPasteboardItem *item in [pasteboard pasteboardItems]) {
            NSData *itemData;
            CFStringRef utiType = GetUTIType(mime_type);
            NSString *availableType = [pasteboard availableTypeFromArray:@[(__bridge NSString *) utiType]];
            itemData = [item dataForType: availableType];
            CFRelease(utiType);
            if (itemData != nil) {
                NSUInteger length = [itemData length];
                *size = (size_t)length;
                data = SDL_malloc(*size + sizeof(Uint32));
                if (data) {
                    [itemData getBytes: data length: length];
                    SDL_memset((Uint8 *)data + length, 0, sizeof(Uint32));
                }
                break;
            }
        }

        if (!data && SDL_strcmp(mime_type, "text/uri-list") == 0) {
            // Try to synthesize text/uri-list entries from public.file-url.
            data = GetTextURIsFromURLs(pasteboard, size);
        }
        return data;
    }
}

bool Cocoa_HasClipboardData(SDL_VideoDevice *_this, const char *mime_type)
{
    bool result = false;
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        CFStringRef utiType = GetUTIType(mime_type);
        if ([pasteboard canReadItemWithDataConformingToTypes: @[(__bridge NSString *)utiType]]) {
            result = true;
        }
        CFRelease(utiType);

        // If there's no text/uri-list type, see if URLs are available.
        if (!result && SDL_strcmp(mime_type, "text/uri-list") == 0) {
            NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey: @YES};
            result = [pasteboard canReadObjectForClasses:@[ [NSURL class] ] options:options];
        }
    }
    return result;

}

#endif // SDL_VIDEO_DRIVER_COCOA
