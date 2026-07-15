/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_EMSCRIPTEN

#include "SDL_emscriptenvideo.h"
#include "SDL_emscriptenclipboard.h"
#include "../SDL_clipboard_c.h"
#include "../../events/SDL_events_c.h"

void Emscripten_InitClipboard(SDL_VideoDevice *device)
{
    MAIN_THREAD_EM_ASM_INT({
        // !!! FIXME: we make sure `Module['SDL3']` exists like this all over. Can we move this to the "main" startup code?
        if (typeof(Module['SDL3']) === 'undefined') {
            Module['SDL3'] = {};
        }

        var SDL3 = Module['SDL3'];
        SDL3.clipboard = {};
        SDL3.clipboard.contents = [];
        SDL3.clipboard.polling_id = -1;

        // we don't touch the clipboard until the app explicitly requests it, because it pops
        //  up a permissions dialog and might result in ongoing overhead. So we call this
        //  function when we finally need to make contact with the outside world.
        SDL3.clipboard.prepare = function() {
            SDL3.clipboard.refresh = function() {
                navigator.clipboard.read().then((clipboardItems) => {
                    SDL3.clipboard.contents = [];
                    for (clipboardItem of clipboardItems) {
                        for (type of clipboardItem.types) {
                            clipboardItem.getType(type).then((blob) => {
                                blob.arrayBuffer().then((buffer) => {
                                    console.log("Got a new clipboard buffer for type '" + type + "'");
                                    SDL3.clipboard.contents[type] = new Uint8Array(buffer);
                                });
                            });
                        }
                    }
                }).catch((err) => { /*console.error(err);*/ /* oh well, no clipboard update for you. */ });
            };

            SDL3.clipboard.refresh();  // get this started right now.

            // There's a new "clipboardchanged" event proposed (as of September 2025). Try to use it.
            if ('onclipboardchange' in navigator.clipboard) {
                SDL3.clipboard.eventHandlerClipboardChange = function(event) {
                    SDL3.clipboard.refresh();
                };
                navigator.clipboard.addEventListener('clipboardchange', SDL3.clipboard.eventHandlerClipboardChange);
            } else {  // fall back to polling once per second. Gross.
                SDL3.clipboard.polling_id = setInterval(SDL3.clipboard.refresh, 1000);
            }

            SDL3.clipboard.prepare = function() {};   // turn this into a no-op now that we're ready.
        };
    });
}

const char **Emscripten_GetTextMimeTypes(SDL_VideoDevice *device, size_t *num_mime_types)
{
    static const char *text_plain = "text/plain";
    *num_mime_types = 1;
    return &text_plain;
}

bool Emscripten_SetClipboardData(SDL_VideoDevice *device)
{
    MAIN_THREAD_EM_ASM({ var SDL3 = Module['SDL3']; SDL3.clipboard.contents = []; SDL3.clipboard.pending_contents = []; });

    for (size_t i = 0; i < device->num_clipboard_mime_types; i++) {
        const char *mime_type = device->clipboard_mime_types[i];
        size_t clipboard_data_size = 0;
        const void *clipboard_data = device->clipboard_callback(device->clipboard_userdata, mime_type, &clipboard_data_size);
        if (clipboard_data && (clipboard_data_size > 0)) {
            MAIN_THREAD_EM_ASM({
                var mime_type = UTF8ToString($0);
                var clipboard_data = $1;
                var clipboard_data_size = $2;
                var ui8array = new Uint8Array(Module.HEAPU8.subarray(clipboard_data, clipboard_data + clipboard_data_size));  // !!! FIXME: I _think_ this makes a copy of the data, not just a view into the heap.
                var SDL3 = Module['SDL3'];
                SDL3.clipboard.contents[mime_type] = ui8array;
                SDL3.clipboard.pending_contents[mime_type] = new Blob([ui8array], { type: mime_type} );
            }, mime_type, clipboard_data, clipboard_data_size);
        }
    }

    MAIN_THREAD_EM_ASM({
        var SDL3 = Module['SDL3'];
        const clipboardItem = new ClipboardItem(SDL3.clipboard.pending_contents);
        SDL3.clipboard.pending_contents = undefined;
        navigator.clipboard.write([clipboardItem]).then(() => {
            //console.log("We set the clipboard!");
        }).catch((err) => { console.error(err); /* oh well, ignore it. */ });
    });

    return true;  // this is an async call, just pretend it worked, even if it didn't.
}

void *Emscripten_GetClipboardData(SDL_VideoDevice *device, const char *mime_type, size_t *length)
{
    if (!Emscripten_HasClipboardData(device, mime_type)) {
        return NULL;
    }

    const size_t buflen = (size_t) MAIN_THREAD_EM_ASM_INT({ return Module['SDL3'].clipboard.contents[UTF8ToString($0)].byteLength; }, mime_type);
    void *retval = SDL_malloc(buflen + 1);
    if (retval) {
        MAIN_THREAD_EM_ASM({
            var ui8array = Module['SDL3'].clipboard.contents[UTF8ToString($0)];
            var heapu8 = new Uint8Array(Module.HEAPU8.buffer, $1, $2);
            heapu8.set(ui8array);
        }, mime_type, retval, buflen);
        ((char *)retval)[buflen] = '\0';  // make sure it's null-terminated.
    }

    return retval;
}

bool Emscripten_HasClipboardData(SDL_VideoDevice *device, const char *mime_type)
{
    return MAIN_THREAD_EM_ASM_INT({
        var SDL3 = Module['SDL3'];
        SDL3.clipboard.prepare();
        return (SDL3.clipboard.contents[UTF8ToString($0)] !== undefined) ? 1 : 0;
    }, mime_type);
}

void Emscripten_QuitClipboard(SDL_VideoDevice *device)
{
    MAIN_THREAD_EM_ASM({
        var SDL3 = Module['SDL3'];
        if (SDL3.clipboard.eventHandlerClipboardChange !== undefined) {
            navigator.clipboard.removeEventListener('clipboardchange', SDL3.clipboard.eventHandlerClipboardChange);
            SDL3.clipboard.eventHandlerClipboardChange = undefined;
        }
        if (SDL3.clipboard.polling_id != -1) {
            clearInterval(SDL3.clipboard.polling_id);
        }
        SDL3.clipboard = undefined;
    });
}

#endif // SDL_VIDEO_DRIVER_EMSCRIPTEN
