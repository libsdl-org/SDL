/*
  Simple DirectMedia Layer
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
#include "SDL_internal.h"

#include "SDL_getenv_c.h"

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
#include "../core/windows/SDL_windows.h"
#endif

#ifdef SDL_PLATFORM_ANDROID
#include "../core/android/SDL_android.h"
#endif

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
#define HAVE_WIN32_ENVIRONMENT
#elif defined(HAVE_GETENV) && \
      (defined(HAVE_SETENV) || defined(HAVE_PUTENV)) && \
      (defined(HAVE_UNSETENV) || defined(HAVE_PUTENV))
#define HAVE_LIBC_ENVIRONMENT
#else
#define HAVE_LOCAL_ENVIRONMENT
#endif

// Put a variable into the environment
// Note: Name may not contain a '=' character. (Reference: http://www.unix.com/man-page/Linux/3/setenv/)
#ifdef HAVE_LIBC_ENVIRONMENT
#if defined(HAVE_SETENV)
int SDL_setenv(const char *name, const char *value, int overwrite)
{
    // Input validation
    if (!name || *name == '\0' || SDL_strchr(name, '=') != NULL || !value) {
        return -1;
    }

    return setenv(name, value, overwrite);
}
// We have a real environment table, but no real setenv? Fake it w/ putenv.
#else
int SDL_setenv(const char *name, const char *value, int overwrite)
{
    char *new_variable;

    // Input validation
    if (!name || *name == '\0' || SDL_strchr(name, '=') != NULL || !value) {
        return -1;
    }

    if (getenv(name) != NULL) {
        if (!overwrite) {
            return 0; // leave the existing one there.
        }
    }

    // This leaks. Sorry. Get a better OS so we don't have to do this.
    SDL_asprintf(&new_variable, "%s=%s", name, value);
    if (!new_variable) {
        return -1;
    }
    return putenv(new_variable);
}
#endif
#elif defined(HAVE_WIN32_ENVIRONMENT)
int SDL_setenv(const char *name, const char *value, int overwrite)
{
    // Input validation
    if (!name || *name == '\0' || SDL_strchr(name, '=') != NULL || !value) {
        return -1;
    }

    if (!overwrite) {
        if (GetEnvironmentVariableA(name, NULL, 0) > 0) {
            return 0; // asked not to overwrite existing value.
        }
    }
    if (!SetEnvironmentVariableA(name, value)) {
        return -1;
    }
    return 0;
}
#else // roll our own

// We'll leak this, as environment variables are intended to persist past SDL_Quit()
static char **SDL_env;

int SDL_setenv(const char *name, const char *value, int overwrite)
{
    int added;
    size_t len, i;
    char **new_env;
    char *new_variable;

    // Input validation
    if (!name || *name == '\0' || SDL_strchr(name, '=') != NULL || !value) {
        return -1;
    }

    // See if it already exists
    if (!overwrite && SDL_getenv(name)) {
        return 0;
    }

    // Allocate memory for the variable
    len = SDL_strlen(name) + SDL_strlen(value) + 2;
    new_variable = (char *)SDL_malloc(len);
    if (!new_variable) {
        return -1;
    }

    SDL_snprintf(new_variable, len, "%s=%s", name, value);
    value = new_variable + SDL_strlen(name) + 1;
    name = new_variable;

    // Actually put it into the environment
    added = 0;
    i = 0;
    if (SDL_env) {
        // Check to see if it's already there...
        len = (value - name);
        for (; SDL_env[i]; ++i) {
            if (SDL_strncmp(SDL_env[i], name, len) == 0) {
                // If we found it, just replace the entry
                SDL_free(SDL_env[i]);
                SDL_env[i] = new_variable;
                added = 1;
                break;
            }
        }
    }

    // Didn't find it in the environment, expand and add
    if (!added) {
        new_env = SDL_realloc(SDL_env, (i + 2) * sizeof(char *));
        if (new_env) {
            SDL_env = new_env;
            SDL_env[i++] = new_variable;
            SDL_env[i++] = (char *)0;
            added = 1;
        } else {
            SDL_free(new_variable);
        }
    }
    return added ? 0 : -1;
}
#endif // HAVE_LIBC_ENVIRONMENT

#ifdef HAVE_LIBC_ENVIRONMENT
#if defined(HAVE_UNSETENV)
int SDL_unsetenv(const char *name)
{
    // Input validation
    if (!name || *name == '\0' || SDL_strchr(name, '=') != NULL) {
        return -1;
    }

    return unsetenv(name);
}
// We have a real environment table, but no unsetenv? Fake it w/ putenv.
#else
int SDL_unsetenv(const char *name)
{
    // Input validation
    if (!name || *name == '\0' || SDL_strchr(name, '=') != NULL) {
        return -1;
    }

    // Hope this environment uses the non-standard extension of removing the environment variable if it has no '='
    return putenv(name);
}
#endif
#elif defined(HAVE_WIN32_ENVIRONMENT)
int SDL_unsetenv(const char *name)
{
    // Input validation
    if (!name || *name == '\0' || SDL_strchr(name, '=') != NULL) {
        return -1;
    }

    if (!SetEnvironmentVariableA(name, NULL)) {
        return -1;
    }
    return 0;
}
#else
int SDL_unsetenv(const char *name)
{
    size_t len, i;

    // Input validation
    if (!name || *name == '\0' || SDL_strchr(name, '=') != NULL) {
        return -1;
    }

    if (SDL_env) {
        len = SDL_strlen(name);
        for (i = 0; SDL_env[i]; ++i) {
            if ((SDL_strncmp(SDL_env[i], name, len) == 0) &&
                (SDL_env[i][len] == '=')) {
                // Just clear out this entry for now
                *SDL_env[i] = '\0';
                break;
            }
        }
    }
    return 0;
}
#endif // HAVE_LIBC_ENVIRONMENT

// Retrieve a variable named "name" from the environment
#ifdef HAVE_LIBC_ENVIRONMENT
const char *SDL_getenv(const char *name)
{
#ifdef SDL_PLATFORM_ANDROID
    // Make sure variables from the application manifest are available
    Android_JNI_GetManifestEnvironmentVariables();
#endif

    // Input validation
    if (!name || *name == '\0') {
        return NULL;
    }

    return getenv(name);
}
#elif defined(HAVE_WIN32_ENVIRONMENT)
const char *SDL_getenv(const char *name)
{
    DWORD length, maxlen = 0;
    char *string = NULL;
    const char *result = NULL;

    // Input validation
    if (!name || *name == '\0') {
        return NULL;
    }

    for ( ; ; ) {
        SetLastError(ERROR_SUCCESS);
        length = GetEnvironmentVariableA(name, string, maxlen);

        if (length > maxlen) {
            char *temp = (char *)SDL_realloc(string, length);
            if (!temp)  {
                return NULL;
            }
            string = temp;
            maxlen = length;
        } else {
            if (GetLastError() != ERROR_SUCCESS) {
                if (string) {
                    SDL_free(string);
                }
                return NULL;
            }
            break;
        }
    }
    if (string) {
        result = SDL_GetPersistentString(string);
        SDL_free(string);
    }
    return result;
}
#else
const char *SDL_getenv(const char *name)
{
    size_t len, i;
    char *value;

    // Input validation
    if (!name || *name == '\0') {
        return NULL;
    }

    value = (char *)0;
    if (SDL_env) {
        len = SDL_strlen(name);
        for (i = 0; SDL_env[i]; ++i) {
            if ((SDL_strncmp(SDL_env[i], name, len) == 0) &&
                (SDL_env[i][len] == '=')) {
                value = &SDL_env[i][len + 1];
                break;
            }
        }
    }
    return value;
}
#endif // HAVE_LIBC_ENVIRONMENT
