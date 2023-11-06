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
 *  \file SDL_filesystem.h
 *
 *  Include file for filesystem SDL API functions
 */

#ifndef SDL_filesystem_h_
#define SDL_filesystem_h_

#include <SDL3/SDL_stdinc.h>

#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the directory where the application was run from.
 *
 * This is not necessarily a fast call, so you should call this once near
 * startup and save the string if you need it.
 *
 * **macOS and iOS Specific Functionality**: If the application is in a ".app"
 * bundle, this function returns the Resource directory (e.g.
 * MyApp.app/Contents/Resources/). This behaviour can be overridden by adding
 * a property to the Info.plist file. Adding a string key with the name
 * SDL_FILESYSTEM_BASE_DIR_TYPE with a supported value will change the
 * behaviour.
 *
 * Supported values for the SDL_FILESYSTEM_BASE_DIR_TYPE property (Given an
 * application in /Applications/SDLApp/MyApp.app):
 *
 * - `resource`: bundle resource directory (the default). For example:
 *   `/Applications/SDLApp/MyApp.app/Contents/Resources`
 * - `bundle`: the Bundle directory. For example:
 *   `/Applications/SDLApp/MyApp.app/`
 * - `parent`: the containing directory of the bundle. For example:
 *   `/Applications/SDLApp/`
 *
 * **Nintendo 3DS Specific Functionality**: This function returns "romfs"
 * directory of the application as it is uncommon to store resources outside
 * the executable. As such it is not a writable directory.
 *
 * The returned path is guaranteed to end with a path separator ('\' on
 * Windows, '/' on most other platforms).
 *
 * The pointer returned is owned by the caller. Please call SDL_free() on the
 * pointer when done with it.
 *
 * \returns an absolute path in UTF-8 encoding to the application data
 *          directory. NULL will be returned on error or when the platform
 *          doesn't implement this functionality, call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPrefPath
 */
extern DECLSPEC char *SDLCALL SDL_GetBasePath(void);

/**
 * Get the user-and-app-specific path where files can be written.
 *
 * Get the "pref dir". This is meant to be where users can write personal
 * files (preferences and save games, etc) that are specific to your
 * application. This directory is unique per user, per application.
 *
 * This function will decide the appropriate location in the native
 * filesystem, create the directory if necessary, and return a string of the
 * absolute path to the directory in UTF-8 encoding.
 *
 * On Windows, the string might look like:
 *
 * `C:\\Users\\bob\\AppData\\Roaming\\My Company\\My Program Name\\`
 *
 * On Linux, the string might look like:
 *
 * `/home/bob/.local/share/My Program Name/`
 *
 * On macOS, the string might look like:
 *
 * `/Users/bob/Library/Application Support/My Program Name/`
 *
 * You should assume the path returned by this function is the only safe place
 * to write files (and that SDL_GetBasePath(), while it might be writable, or
 * even the parent of the returned path, isn't where you should be writing
 * things).
 *
 * Both the org and app strings may become part of a directory name, so please
 * follow these rules:
 *
 * - Try to use the same org string (_including case-sensitivity_) for all
 *   your applications that use this function.
 * - Always use a unique app string for each one, and make sure it never
 *   changes for an app once you've decided on it.
 * - Unicode characters are legal, as long as it's UTF-8 encoded, but...
 * - ...only use letters, numbers, and spaces. Avoid punctuation like "Game
 *   Name 2: Bad Guy's Revenge!" ... "Game Name 2" is sufficient.
 *
 * The returned path is guaranteed to end with a path separator ('\' on
 * Windows, '/' on most other platforms).
 *
 * The pointer returned is owned by the caller. Please call SDL_free() on the
 * pointer when done with it.
 *
 * \param org the name of your organization
 * \param app the name of your application
 * \returns a UTF-8 string of the user directory in platform-dependent
 *          notation. NULL if there's a problem (creating directory failed,
 *          etc.).
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetBasePath
 */
extern DECLSPEC char *SDLCALL SDL_GetPrefPath(const char *org, const char *app);

/**
 * The type of the OS-provided default folder for a specific purpose.
 *
 * Note that the Trash folder isn't included here, because trashing files usually
 * involves extra OS-specific functionality to remember the file's original
 * location.
 *
 * The folders supported per platform are:
 *
 * |             | Windows | macOS/iOS | tvOS | Unix (XDG) | Haiku | Emscripten |
 * | ----------- | ------- | --------- | ---- | ---------- | ----- | ---------- |
 * | HOME        | X       | X         |      | X          | X     | X          |
 * | DESKTOP     | X       | X         |      | X          | X     |            |
 * | DOCUMENTS   | X       | X         |      | X          |       |            |
 * | DOWNLOADS   | Vista+  | X         |      | X          |       |            |
 * | MUSIC       | X       | X         |      | X          |       |            |
 * | PICTURES    | X       | X         |      | X          |       |            |
 * | PUBLICSHARE |         | X         |      | X          |       |            |
 * | SAVEDGAMES  | Vista+  |           |      |            |       |            |
 * | SCREENSHOTS | Vista+  |           |      |            |       |            |
 * | TEMPLATES   | X       | X         |      | X          |       |            |
 * | VIDEOS      | X       | X*        |      | X          |       |            |
 *
 * * Note that on macOS/iOS, the Videos folder is called "Movies".
 *
 * \sa SDL_GetUserFolder
 */
typedef enum
{
  /** The folder which contains all of the current user's data, preferences,
      and documents. It usually contains most of the other folders. If a
      requested folder does not exist, the home folder can be considered a safe
      fallback to store a user's documents. */
  SDL_FOLDER_HOME,
  /** The folder of files that are displayed on the desktop. Note that the
      existence of a desktop folder does not guarantee that the system does
      show icons on its desktop; certain GNU/Linux distros with a graphical
      environment may not have desktop icons. */
  SDL_FOLDER_DESKTOP,
  /** User document files, possibly application-specific. This is a good
      place to save a user's projects. */
  SDL_FOLDER_DOCUMENTS,
  /** Standard folder for user files downloaded from the internet. */
  SDL_FOLDER_DOWNLOADS,
  /** Music files that can be played using a standard music player (mp3,
      ogg...). */
  SDL_FOLDER_MUSIC,
  /** Image files that can be displayed using a standard viewer (png,
      jpg...). */
  SDL_FOLDER_PICTURES,
  /** Files that are meant to be shared with other users on the same
      computer. */
  SDL_FOLDER_PUBLICSHARE,
  /** Save files for games. */
  SDL_FOLDER_SAVEDGAMES,
  /** Application screenshots. */
  SDL_FOLDER_SCREENSHOTS,
  /** Template files to be used when the user requests the desktop environment
      to create a new file in a certain folder, such as "New Text File.txt".
      Any file in the Templates folder can be used as a starting point for a
      new file. */
  SDL_FOLDER_TEMPLATES,
  /** Video files that can be played using a standard video player (mp4,
      webm...). */
  SDL_FOLDER_VIDEOS
} SDL_Folder;

/**
 * Finds the most suitable user folder for @p purpose, and returns its path in
 * OS-specific notation.
 *
 * Many OSes provide certain standard folders for certain purposes, such as
 * storing pictures, music or videos for a certain user. This function gives
 * the path for many of those special locations.
 *
 * This function is specifically for _user_ folders, which are meant for the
 * user to access and manage. For application-specific folders, meant to hold
 * data for the application to manage, see SDL_GetBasePath() and
 * SDL_GetPrefPath().
 *
 * Note that the function is expensive, and should be called once at the
 * beginning of the execution and kept for as long as needed.
 *
 * The returned value is owned by the caller and should be freed with
 * SDL_free().
 *
 * If NULL is returned, the error may be obtained with SDL_GetError().
 *
 * \param folder The type of folder to find
 * \returns Either a null-terminated C string containing the full path to the
 *          folder, or NULL if an error happened.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_Folder
 */
extern DECLSPEC char *SDLCALL SDL_GetUserFolder(SDL_Folder folder);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_filesystem_h_ */
