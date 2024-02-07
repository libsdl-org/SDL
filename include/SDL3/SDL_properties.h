/*
  Simple DiretMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
 *  Header file for SDL properties.
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
 * SDL property type
 */
typedef enum
{
    SDL_PROPERTY_TYPE_INVALID,
    SDL_PROPERTY_TYPE_POINTER,
    SDL_PROPERTY_TYPE_STRING,
    SDL_PROPERTY_TYPE_NUMBER,
    SDL_PROPERTY_TYPE_FLOAT,
    SDL_PROPERTY_TYPE_BOOLEAN,
} SDL_PropertyType;

/**
 * Get the global SDL properties
 *
 * \returns a valid property ID on success or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetProperty
 * \sa SDL_SetProperty
 */
extern DECLSPEC SDL_PropertiesID SDLCALL SDL_GetGlobalProperties(void);

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
 * Copy a set of properties
 *
 * Copy all the properties from one set of properties to another, with the
 * exception of properties requiring cleanup (set using
 * SDL_SetPropertyWithCleanup()), which will not be copied. Any property that
 * already exists on `dst` will be overwritten.
 *
 * \param src the properties to copy
 * \param dst the destination properties
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_CopyProperties(SDL_PropertiesID src, SDL_PropertiesID dst);

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
 * Set a property on a set of properties with a cleanup function that is
 * called when the property is deleted
 *
 * The cleanup function is also called if setting the property fails for any
 * reason.
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
 * \sa SDL_SetProperty
 */
extern DECLSPEC int SDLCALL SDL_SetPropertyWithCleanup(SDL_PropertiesID props, const char *name, void *value, void (SDLCALL *cleanup)(void *userdata, void *value), void *userdata);

/**
 * Set a property on a set of properties
 *
 * \param props the properties to modify
 * \param name the name of the property to modify
 * \param value the new value of the property, or NULL to delete the property
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetProperty
 * \sa SDL_SetPropertyWithCleanup
 */
extern DECLSPEC int SDLCALL SDL_SetProperty(SDL_PropertiesID props, const char *name, void *value);

/**
 * Set a string property on a set of properties
 *
 * This function makes a copy of the string; the caller does not have to
 * preserve the data after this call completes.
 *
 * \param props the properties to modify
 * \param name the name of the property to modify
 * \param value the new value of the property, or NULL to delete the property
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetStringProperty
 */
extern DECLSPEC int SDLCALL SDL_SetStringProperty(SDL_PropertiesID props, const char *name, const char *value);

/**
 * Set an integer property on a set of properties
 *
 * \param props the properties to modify
 * \param name the name of the property to modify
 * \param value the new value of the property
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetNumberProperty
 */
extern DECLSPEC int SDLCALL SDL_SetNumberProperty(SDL_PropertiesID props, const char *name, Sint64 value);

/**
 * Set a floating point property on a set of properties
 *
 * \param props the properties to modify
 * \param name the name of the property to modify
 * \param value the new value of the property
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetFloatProperty
 */
extern DECLSPEC int SDLCALL SDL_SetFloatProperty(SDL_PropertiesID props, const char *name, float value);

/**
 * Set a boolean property on a set of properties
 *
 * \param props the properties to modify
 * \param name the name of the property to modify
 * \param value the new value of the property
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetBooleanProperty
 */
extern DECLSPEC int SDLCALL SDL_SetBooleanProperty(SDL_PropertiesID props, const char *name, SDL_bool value);

/**
 * Return whether a property exists in a set of properties.
 *
 * \param props the properties to query
 * \param name the name of the property to query
 * \returns SDL_TRUE if the property exists, or SDL_FALSE if it doesn't.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPropertyType
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasProperty(SDL_PropertiesID props, const char *name);

/**
 * Get the type of a property on a set of properties
 *
 * \param props the properties to query
 * \param name the name of the property to query
 * \returns the type of the property, or SDL_PROPERTY_TYPE_INVALID if it is
 *          not set.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_HasProperty
 */
extern DECLSPEC SDL_PropertyType SDLCALL SDL_GetPropertyType(SDL_PropertiesID props, const char *name);

/**
 * Get a property on a set of properties
 *
 * By convention, the names of properties that SDL exposes on objects will
 * start with "SDL.", and properties that SDL uses internally will start with
 * "SDL.internal.". These should be considered read-only and should not be
 * modified by applications.
 *
 * \param props the properties to query
 * \param name the name of the property to query
 * \param default_value the default value of the property
 * \returns the value of the property, or `default_value` if it is not set or
 *          not a pointer property.
 *
 * \threadsafety It is safe to call this function from any thread, although
 *               the data returned is not protected and could potentially be
 *               freed if you call SDL_SetProperty() or SDL_ClearProperty() on
 *               these properties from another thread. If you need to avoid
 *               this, use SDL_LockProperties() and SDL_UnlockProperties().
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPropertyType
 * \sa SDL_HasProperty
 * \sa SDL_SetProperty
 */
extern DECLSPEC void *SDLCALL SDL_GetProperty(SDL_PropertiesID props, const char *name, void *default_value);

/**
 * Get a string property on a set of properties
 *
 * \param props the properties to query
 * \param name the name of the property to query
 * \param default_value the default value of the property
 * \returns the value of the property, or `default_value` if it is not set or
 *          not a string property.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPropertyType
 * \sa SDL_HasProperty
 * \sa SDL_SetStringProperty
 */
extern DECLSPEC const char *SDLCALL SDL_GetStringProperty(SDL_PropertiesID props, const char *name, const char *default_value);

/**
 * Get a number property on a set of properties
 *
 * You can use SDL_GetPropertyType() to query whether the property exists and
 * is a number property.
 *
 * \param props the properties to query
 * \param name the name of the property to query
 * \param default_value the default value of the property
 * \returns the value of the property, or `default_value` if it is not set or
 *          not a number property.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPropertyType
 * \sa SDL_HasProperty
 * \sa SDL_SetNumberProperty
 */
extern DECLSPEC Sint64 SDLCALL SDL_GetNumberProperty(SDL_PropertiesID props, const char *name, Sint64 default_value);

/**
 * Get a floating point property on a set of properties
 *
 * You can use SDL_GetPropertyType() to query whether the property exists and
 * is a floating point property.
 *
 * \param props the properties to query
 * \param name the name of the property to query
 * \param default_value the default value of the property
 * \returns the value of the property, or `default_value` if it is not set or
 *          not a float property.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPropertyType
 * \sa SDL_HasProperty
 * \sa SDL_SetFloatProperty
 */
extern DECLSPEC float SDLCALL SDL_GetFloatProperty(SDL_PropertiesID props, const char *name, float default_value);

/**
 * Get a boolean property on a set of properties
 *
 * You can use SDL_GetPropertyType() to query whether the property exists and
 * is a boolean property.
 *
 * \param props the properties to query
 * \param name the name of the property to query
 * \param default_value the default value of the property
 * \returns the value of the property, or `default_value` if it is not set or
 *          not a float property.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPropertyType
 * \sa SDL_HasProperty
 * \sa SDL_SetBooleanProperty
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GetBooleanProperty(SDL_PropertiesID props, const char *name, SDL_bool default_value);

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
 */
extern DECLSPEC int SDLCALL SDL_ClearProperty(SDL_PropertiesID props, const char *name);

typedef void (SDLCALL *SDL_EnumeratePropertiesCallback)(void *userdata, SDL_PropertiesID props, const char *name);

/**
 * Enumerate the properties on a set of properties
 *
 * The callback function is called for each property on the set of properties.
 * The properties are locked during enumeration.
 *
 * \param props the properties to query
 * \param callback the function to call for each property
 * \param userdata a pointer that is passed to `callback`
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_EnumerateProperties(SDL_PropertiesID props, SDL_EnumeratePropertiesCallback callback, void *userdata);

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
