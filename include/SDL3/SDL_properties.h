/*
  Simple DiretMedia Layer
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
 *  \file SDL_properties.h
 *
 *  \brief Header file for SDL properties.
 */

#ifndef SDL_properties_h_
#define SDL_properties_h_

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * SDL properties ID
 */
typedef Uint32 SDL_PropertiesID;

/**
 * Create a set of properties
 *
 * All properties are automatically destroyed when SDL_Quit() is called.
 *
 * \returns an ID for a new set of properties, or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroyProperties
 */
extern DECLSPEC SDL_PropertiesID SDLCALL SDL_CreateProperties(void);

/**
 * Lock a set of properties
 *
 * Obtain a multi-threaded lock for these properties. Other threads will wait
 * while trying to lock these properties until they are unlocked. Properties
 * must be unlocked before they are destroyed.
 *
 * The lock is automatically taken when setting individual properties, this
 * function is only needed when you want to set several properties atomically
 * or want to guarantee that properties being queried aren't freed in another
 * thread.
 *
 * \param props the properties to lock
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_UnlockProperties
 */
extern DECLSPEC int SDLCALL SDL_LockProperties(SDL_PropertiesID props);

/**
 * Unlock a set of properties
 *
 * \param props the properties to unlock
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockProperties
 */
extern DECLSPEC void SDLCALL SDL_UnlockProperties(SDL_PropertiesID props);

/**
 * Set a property on a set of properties
 *
 * \param props the properties to modify
 * \param name the name of the property to modify
 * \param value the new value of the property, or NULL to delete the property
 * \param cleanup the function to call when this property is deleted, or NULL
 *                if no cleanup is necessary
 * \param userdata a pointer that is passed to the cleanup function
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetProperty
 */
extern DECLSPEC int SDLCALL SDL_SetProperty(SDL_PropertiesID props, const char *name, void *value, void (SDLCALL *cleanup)(void *userdata, void *value), void *userdata);

/**
 * Get a property on a set of properties
 *
 * \param props the properties to query
 * \param name the name of the property to query
 * \returns the value of the property, or NULL if it is not set.
 *
 * \threadsafety It is safe to call this function from any thread, although
 *               the data returned is not protected and could potentially be
 *               freed if you call SDL_SetProperty() or SDL_ClearProperty() on
 *               these properties from another thread. If you need to avoid
 *               this, use SDL_LockProperties() and SDL_UnlockProperties().
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetProperty
 */
extern DECLSPEC void *SDLCALL SDL_GetProperty(SDL_PropertiesID props, const char *name);

/**
 * Clear a property on a set of properties
 *
 * \param props the properties to modify
 * \param name the name of the property to clear
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetProperty
 */
extern DECLSPEC int SDLCALL SDL_ClearProperty(SDL_PropertiesID props, const char *name);

/**
 * Destroy a set of properties
 *
 * All properties are deleted and their cleanup functions will be called, if
 * any.
 *
 * \param props the properties to destroy
 *
 * \threadsafety This function should not be called while these properties are
 *               locked or other threads might be setting or getting values
 *               from these properties.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateProperties
 */
extern DECLSPEC void SDLCALL SDL_DestroyProperties(SDL_PropertiesID props);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_properties_h_ */
