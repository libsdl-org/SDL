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

/**
 * \file SDL_clipboard.h
 *
 * \brief Include file for SDL clipboard handling
 */

#ifndef SDL_clipboard_h_
#define SDL_clipboard_h_

#include <SDL3/SDL_stdinc.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Function prototypes */

/**
 * Put UTF-8 text into the clipboard.
 *
 * \param text the text to store in the clipboard
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetClipboardText
 * \sa SDL_HasClipboardText
 */
extern DECLSPEC int SDLCALL SDL_SetClipboardText(const char *text);

/**
 * Get UTF-8 text from the clipboard, which must be freed with SDL_free().
 *
 * This functions returns empty string if there was not enough memory left for
 * a copy of the clipboard's content.
 *
 * \returns the clipboard text on success or an empty string on failure; call
 *          SDL_GetError() for more information. Caller must call SDL_free()
 *          on the returned pointer when done with it (even if there was an
 *          error).
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_HasClipboardText
 * \sa SDL_SetClipboardText
 */
extern DECLSPEC char * SDLCALL SDL_GetClipboardText(void);

/**
 * Query whether the clipboard exists and contains a non-empty text string.
 *
 * \returns SDL_TRUE if the clipboard has text, or SDL_FALSE if it does not.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetClipboardText
 * \sa SDL_SetClipboardText
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasClipboardText(void);

/**
 * Put UTF-8 text into the primary selection.
 *
 * \param text the text to store in the primary selection
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPrimarySelectionText
 * \sa SDL_HasPrimarySelectionText
 */
extern DECLSPEC int SDLCALL SDL_SetPrimarySelectionText(const char *text);

/**
 * Get UTF-8 text from the primary selection, which must be freed with
 * SDL_free().
 *
 * This functions returns empty string if there was not enough memory left for
 * a copy of the primary selection's content.
 *
 * \returns the primary selection text on success or an empty string on
 *          failure; call SDL_GetError() for more information. Caller must
 *          call SDL_free() on the returned pointer when done with it (even if
 *          there was an error).
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_HasPrimarySelectionText
 * \sa SDL_SetPrimarySelectionText
 */
extern DECLSPEC char * SDLCALL SDL_GetPrimarySelectionText(void);

/**
 * Query whether the primary selection exists and contains a non-empty text
 * string.
 *
 * \returns SDL_TRUE if the primary selection has text, or SDL_FALSE if it
 *          does not.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPrimarySelectionText
 * \sa SDL_SetPrimarySelectionText
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasPrimarySelectionText(void);

/**
 * Callback function that will be called when data for the specified mime-type
 * is requested by the OS.
 *
 * The callback function is called with NULL as the mime_type when the clipboard
 * is cleared or new data is set. The clipboard is automatically cleared in SDL_Quit().
 *
 * \param userdata  A pointer to provided user data
 * \param mime_type The requested mime-type
 * \param size      A pointer filled in with the length of the returned data
 * \returns a pointer to the data for the provided mime-type. Returning NULL or
 *          setting length to 0 will cause no data to be sent to the "receiver". It is
 *          up to the receiver to handle this. Essentially returning no data is more or
 *          less undefined behavior and may cause breakage in receiving applications.
 *          The returned data will not be freed so it needs to be retained and dealt
 *          with internally.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetClipboardData
 */
typedef const void *(SDLCALL *SDL_ClipboardDataCallback)(void *userdata, const char *mime_type, size_t *size);

/**
 * Callback function that will be called when the clipboard is cleared, or new data is set.
 *
 * \param userdata A pointer to provided user data
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetClipboardData
 */
typedef void (SDLCALL *SDL_ClipboardCleanupCallback)(void *userdata);

/**
 * Offer clipboard data to the OS
 *
 * Tell the operating system that the application is offering clipboard data
 * for each of the proivded mime-types. Once another application requests the
 * data the callback function will be called allowing it to generate and
 * respond with the data for the requested mime-type.
 *
 * The size of text data does not include any terminator, and the text does
 * not need to be null terminated (e.g. you can directly copy a portion of a
 * document)
 *
 * \param callback A function pointer to the function that provides the
 *                 clipboard data
 * \param cleanup A function pointer to the function that cleans up the
 *                clipboard data
 * \param userdata An opaque pointer that will be forwarded to the callbacks
 * \param mime_types A list of mime-types that are being offered
 * \param num_mime_types The number of mime-types in the mime_types list
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ClipboardDataCallback
 * \sa SDL_SetClipboardData
 * \sa SDL_GetClipboardData
 * \sa SDL_HasClipboardData
 */
extern DECLSPEC int SDLCALL SDL_SetClipboardData(SDL_ClipboardDataCallback callback, SDL_ClipboardCleanupCallback cleanup, void *userdata, const char **mime_types, size_t num_mime_types);

/**
 * Clear the clipboard data
 *
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetClipboardData
 */
extern DECLSPEC int SDLCALL SDL_ClearClipboardData(void);

/**
 * Get the data from clipboard for a given mime type
 *
 * The size of text data does not include the terminator, but the text is
 * guaranteed to be null terminated.
 *
 * \param mime_type The mime type to read from the clipboard
 * \param size A pointer filled in with the length of the returned data
 * \returns the retrieved data buffer or NULL on failure; call SDL_GetError()
 *          for more information. Caller must call SDL_free() on the returned
 *          pointer when done with it.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetClipboardData
 */
extern DECLSPEC void *SDLCALL SDL_GetClipboardData(const char *mime_type, size_t *size);

/**
 * Query whether there is data in the clipboard for the provided mime type
 *
 * \param mime_type The mime type to check for data for
 * \returns SDL_TRUE if there exists data in clipboard for the provided mime
 *          type, SDL_FALSE if it does not.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetClipboardData
 * \sa SDL_GetClipboardData
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasClipboardData(const char *mime_type);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_clipboard_h_ */
