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

#ifndef SDL_time_h_
#define SDL_time_h_

/**
 *  \file SDL_time.h
 *
 *  Header for the SDL realtime clock and date/time routines.
 */

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * A structure holding a calendar date and time broken down into its
 * components.
 *
 * \since This struct is available since SDL 3.0.0.
 */
typedef struct SDL_DateTime
{
    int year;                  /**< Year */
    int month;                 /**< Month [01-12] */
    int day;                   /**< Day of the month [01-31] */
    int hour;                  /**< Hour [0-23] */
    int minute;                /**< Minute [0-59] */
    int second;                /**< Seconds [0-60] */
    int nanosecond;            /**< Nanoseconds [0-999999999] */
    int day_of_week;           /**< Day of the week [0-6] (0 being Sunday) */
    int utc_offset;            /**< Seconds east of UTC */
} SDL_DateTime;

/**
 * The preferred date format of the current system locale.
 *
 * \since This enum is available since SDL 3.0.0.
 *
 * \sa SDL_PROP_GLOBAL_SYSTEM_DATE_FORMAT_NUMBER
 */
typedef enum SDL_DateFormat
{
    SDL_DATE_FORMAT_YYYYMMDD = 0, /**< Year/Month/Day */
    SDL_DATE_FORMAT_DDMMYYYY = 1, /**< Day/Month/Year */
    SDL_DATE_FORMAT_MMDDYYYY = 2  /**< Month/Day/Year */
} SDL_DateFormat;

/**
 * The preferred time format of the current system locale.
 *
 * \since This enum is available since SDL 3.0.0.
 *
 * \sa SDL_PROP_GLOBAL_SYSTEM_TIME_FORMAT_NUMBER
 */
typedef enum SDL_TimeFormat
{
    SDL_TIME_FORMAT_24HR = 0, /**< 24 hour time */
    SDL_TIME_FORMAT_12HR = 1  /**< 12 hour time */
} SDL_TimeFormat;

/*
 *  Global date/time properties.
 *
 *  - `SDL_PROP_GLOBAL_SYSTEM_DATE_FORMAT_NUMBER`: the SDL_DateFormat to use as the preferred date display format
 *    for the current system locale.
 *  - `SDL_PROP_GLOBAL_SYSTEM_TIME_FORMAT_NUMBER`: the SDL_TimeFormat to use as the preferred time display format
 *    for the current system locale.
 */
#define SDL_PROP_GLOBAL_SYSTEM_DATE_FORMAT_NUMBER "SDL.time.date_format"
#define SDL_PROP_GLOBAL_SYSTEM_TIME_FORMAT_NUMBER "SDL.time.time_format"

/**
 * Gets the current value of the system realtime clock in nanoseconds since
 * Jan 1, 1970 in Universal Coordinated Time (UTC).
 *
 * \param ticks the SDL_Time to hold the returned tick count
 * \returns 0 on success or -1 on error; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetCurrentTime(SDL_Time *ticks);

/**
 * Converts an SDL_Time in nanoseconds since the epoch to a calendar time in
 * the SDL_DateTime format.
 *
 * \param ticks the SDL_Time to be converted
 * \param dt the resulting SDL_DateTime
 * \param localTime the resulting SDL_DateTime will be expressed in local time
 *                  if true, otherwise it will be in Universal Coordinated
 *                  Time (UTC)
 * \returns 0 on success or -1 on error; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_TimeToDateTime(SDL_Time ticks, SDL_DateTime *dt, SDL_bool localTime);

/**
 * Converts a calendar time to an SDL_Time in nanoseconds since the epoch.
 *
 * This function ignores the day_of_week member of the SDL_DateTime struct, so
 * it may remain unset.
 *
 * \param dt the source SDL_DateTime
 * \param ticks the resulting SDL_Time
 * \returns 0 on success or -1 on error; call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_DateTimeToTime(const SDL_DateTime *dt, SDL_Time *ticks);

/**
 * Converts an SDL time into a Windows FILETIME (100-nanosecond intervals
 * since January 1, 1601).
 *
 * This function fills in the two 32-bit values of the FILETIME structure.
 *
 * \param ticks the time to convert
 * \param dwLowDateTime a pointer filled in with the low portion of the
 *                      Windows FILETIME value
 * \param dwHighDateTime a pointer filled in with the high portion of the
 *                       Windows FILETIME value
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC void SDLCALL SDL_TimeToWindows(SDL_Time ticks, Uint32 *dwLowDateTime, Uint32 *dwHighDateTime);

/**
 * Converts a Windows FILETIME (100-nanosecond intervals since January 1,
 * 1601) to an SDL time.
 *
 * This function takes the two 32-bit values of the FILETIME structure as
 * parameters.
 *
 * \param dwLowDateTime the low portion of the Windows FILETIME value
 * \param dwHighDateTime the high portion of the Windows FILETIME value
 * \returns the converted SDL time
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Time SDLCALL SDL_TimeFromWindows(Uint32 dwLowDateTime, Uint32 dwHighDateTime);

/**
 * Get the number of days in a month for a given year.
 *
 * \param year the year
 * \param month the month [1-12]
 * \returns the number of days in the requested month, otherwise -1; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetDaysInMonth(int year, int month);

/**
 * Get the day of year for a calendar date.
 *
 * \param year the year component of the date
 * \param month the month component of the date
 * \param day the day component of the date
 * \returns the day of year [0-365] if the date is valid, otherwise -1; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetDayOfYear(int year, int month, int day);

/**
 * Get the day of week for a calendar date.
 *
 * \param year the year component of the date
 * \param month the month component of the date
 * \param day the day component of the date
 * \returns a value between 0 and 6 (0 being Sunday) if the date is valid,
 *          otherwise -1; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetDayOfWeek(int year, int month, int day);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_time_h_ */
