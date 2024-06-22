/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Test program to verify the SDL date/time APIs */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#define CAL_Y_OFF   100.0f
#define CAL_X_OFF   19.0f
#define CELL_WIDTH  86.0f
#define CELL_HEIGHT 60.0f

static int cal_year;
static int cal_month;
static SDL_TimeFormat time_format;
static SDL_DateFormat date_format;

static void RenderDateTime(SDL_Renderer *r)
{
    const char *const WDAY[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    const char *const MNAME[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                                  "Aug", "Sep", "Oct", "Nov", "Dec" };
    const char *const TIMEPOST[] = { "", " AM", " PM" };

    int day, len;
    const char *postfix = TIMEPOST[0];
    float x, y;
    const float x_max = CAL_X_OFF + (CELL_WIDTH * 7);
    const float y_max = CAL_Y_OFF + (CELL_HEIGHT * 6);
    char str[256];
    char short_date[128];
    SDL_Time ticks;
    SDL_DateTime dt;

    SDL_SetRenderDrawColor(r, 0xFF, 0xFF, 0xFF, 0xFF);

    /* Query the current time and print it. */
    SDL_GetCurrentTime(&ticks);
    SDL_TimeToDateTime(ticks, &dt, SDL_FALSE);

    switch (date_format) {
    case SDL_DATE_FORMAT_YYYYMMDD:
        SDL_snprintf(short_date, sizeof(short_date), "%04d-%02d-%02d", dt.year, dt.month, dt.day);
        break;
    case SDL_DATE_FORMAT_DDMMYYYY:
        SDL_snprintf(short_date, sizeof(short_date), "%02d.%02d.%04d", dt.day, dt.month, dt.year);
        break;
    case SDL_DATE_FORMAT_MMDDYYYY:
        SDL_snprintf(short_date, sizeof(short_date), "%02d/%02d/%04d", dt.month, dt.day, dt.year);
        break;
    }

    if (time_format) {
        if (dt.hour > 12) { /* PM */
            dt.hour -= 12;
            postfix = TIMEPOST[2];
        } else {
            if (!dt.hour) { /* AM */
                dt.hour = 12; /* Midnight */
            }
            postfix = TIMEPOST[1];
        }
    }

    SDL_snprintf(str, sizeof(str), "UTC:   %s %02d %s %04d (%s) %02d:%02d:%02d.%09d%s %+05d",
                 WDAY[dt.day_of_week], dt.day, MNAME[dt.month - 1], dt.year, short_date,
                 dt.hour, dt.minute, dt.second, dt.nanosecond, postfix, ((dt.utc_offset / 3600) * 100) + (dt.utc_offset % 3600));

    SDLTest_DrawString(r, 10, 15, str);

    SDL_TimeToDateTime(ticks, &dt, SDL_TRUE);
    if (time_format) {
        if (dt.hour > 12) { /* PM */
            dt.hour -= 12;
            postfix = TIMEPOST[2];
        } else {
            if (!dt.hour) { /* AM */
                dt.hour = 12; /* Midnight */
            }
            postfix = TIMEPOST[1];
        }
    }

    SDL_snprintf(str, sizeof(str), "Local: %s %02d %s %04d (%s) %02d:%02d:%02d.%09d%s %+05d",
                 WDAY[dt.day_of_week], dt.day, MNAME[dt.month - 1], dt.year, short_date,
                 dt.hour, dt.minute, dt.second, dt.nanosecond, postfix,
                 ((dt.utc_offset / 3600) * 100) + (dt.utc_offset % 3600));
    SDLTest_DrawString(r, 10, 30, str);

    /* Draw a calendar. */
    if (!cal_month) {
        cal_month = dt.month;
        cal_year = dt.year;
    }

    for (y = CAL_Y_OFF; y <= CAL_Y_OFF + (CELL_HEIGHT * 6); y += CELL_HEIGHT) {
        SDL_RenderLine(r, CAL_X_OFF, y, x_max, y);
    }
    for (x = CAL_X_OFF; x <= CAL_X_OFF + (CELL_WIDTH * 7); x += CELL_WIDTH) {
        SDL_RenderLine(r, x, CAL_Y_OFF, x, y_max);
    }

    /* Draw the month and year. */
    len = SDL_snprintf(str, sizeof(str), "%s %04d", MNAME[cal_month - 1], cal_year);
    SDLTest_DrawString(r, (CAL_X_OFF + ((x_max - CAL_X_OFF) / 2)) - ((FONT_CHARACTER_SIZE * len) / 2), CAL_Y_OFF - (FONT_LINE_HEIGHT * 3), str);

    /* Draw day names */
    for (x = 0; x < 7; ++x) {
        float offset = ((CAL_X_OFF + (CELL_WIDTH * x)) + (CELL_WIDTH / 2)) - ((FONT_CHARACTER_SIZE * 3) / 2);
        SDLTest_DrawString(r, offset, CAL_Y_OFF - FONT_LINE_HEIGHT, WDAY[(int)x]);
    }

    day = SDL_GetDayOfWeek(cal_year, cal_month, 1);
    x = CAL_X_OFF + (day * CELL_WIDTH + (CELL_WIDTH - (FONT_CHARACTER_SIZE * 3)));
    day = 0;
    y = CAL_Y_OFF + FONT_LINE_HEIGHT;
    while (++day <= SDL_GetDaysInMonth(cal_year, cal_month)) {
        SDL_snprintf(str, sizeof(str), "%02d", day);

        /* Highlight the current day in red. */
        if (cal_year == dt.year && cal_month == dt.month && day == dt.day) {
            SDL_SetRenderDrawColor(r, 0xFF, 0, 0, 0xFF);
        }
        SDLTest_DrawString(r, x, y, str);
        SDL_SetRenderDrawColor(r, 0xFF, 0xFF, 0xFF, 0xFF);

        x += CELL_WIDTH;
        if (x >= x_max) {
            x = CAL_X_OFF + (CELL_WIDTH - (FONT_CHARACTER_SIZE * 3));
            y += CELL_HEIGHT;
        }
    }
}

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    SDL_Event event;
    int done;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDLTest_CommonInit(state)) {
        goto quit;
    }

    SDL_GetDateTimeLocalePreferences(&date_format, &time_format);

    /* Main render loop */
    done = 0;

    while (!done) {
        /* Check for events */
        while (SDL_PollEvent(&event)) {
            SDLTest_CommonEvent(state, &event, &done);
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                case SDLK_UP:
                    if (++cal_month > 12) {
                        cal_month = 1;
                        ++cal_year;
                    }
                    break;
                case SDLK_DOWN:
                    if (--cal_month < 1) {
                        cal_month = 12;
                        --cal_year;
                    }
                    break;
                case SDLK_1:
                    time_format = SDL_TIME_FORMAT_24HR;
                    break;
                case SDLK_2:
                    time_format = SDL_TIME_FORMAT_12HR;
                    break;
                case SDLK_3:
                    date_format = SDL_DATE_FORMAT_YYYYMMDD;
                    break;
                case SDLK_4:
                    date_format = SDL_DATE_FORMAT_DDMMYYYY;
                    break;
                case SDLK_5:
                    date_format = SDL_DATE_FORMAT_MMDDYYYY;
                    break;
                default:
                    break;
                }
            } else if (event.type == SDL_EVENT_LOCALE_CHANGED) {
                SDL_GetDateTimeLocalePreferences(&date_format, &time_format);
            }
        }

        SDL_SetRenderDrawColor(state->renderers[0], 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(state->renderers[0]);

        RenderDateTime(state->renderers[0]);

        SDL_RenderPresent(state->renderers[0]);
    }

quit:
    SDLTest_CommonQuit(state);
    return 0;
}
