/**
 * Timer test suite
 */
#include "testautomation_suites.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>

/* 1970-01-01T00:00:00.00 UTC */
#define JAN_1_1970_NS 0

/* 1970-01-01T00:00:00.01 UTC */
#define JAN_1_1970_1_NS 1

/* 1969-12-31T23:59:59.999999997 UTC */
#define DEC_31_1969_NS -3

/* 2000-01-01T16:35:42.10 UTC */
#define JAN_1_2000_NS SDL_SECONDS_TO_NS(946744542) + 10

/* 2000-02-29T00:00:00.00 UTC (leap year) */
#define FEB_29_2000_NS SDL_SECONDS_TO_NS(951782400)

/* 1955-11-05T01:21:59.03 */
#define NOV_5_1955_NS SDL_SECONDS_TO_NS(-446769481) + 3

/* 1955-11-05T01:21:59.500000000 */
#define NOV_5_1955_1_NS SDL_SECONDS_TO_NS(-446769481) + 500000000

/* Test case functions */

/**
 * Call to SDL_GetRealtimeClock
 */
static int SDLCALL time_getRealtimeClock(void *arg)
{
    int result;
    SDL_Time ticks;

    result = SDL_GetCurrentTime(&ticks);
    SDLTest_AssertPass("Call to SDL_GetRealtimeClockTicks()");
    SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

    return TEST_COMPLETED;
}

/**
 * Test bidirectional SDL_DateTime conversions.
 */
static int SDLCALL time_dateTimeConversion(void *arg)
{
    static const struct
    {
        SDL_Time ticks;
        const char *desc;

        /* Expected values */
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        int nanosecond;
        int day_of_week;
    } TimeTest[] = {
        { JAN_1_1970_NS, "1970-01-01T00:00:00.00 (Thu)", 1970, 1, 1, 0, 0, 0, 0, 4 },
        { JAN_1_1970_1_NS, "1970-01-01T00:00:00.01 (Thu)", 1970, 1, 1, 0, 0, 0, 1, 4 },
        { DEC_31_1969_NS, "1969-12-31T23:59:59.999999997 (Wed)", 1969, 12, 31, 23, 59, 59, 999999997, 3 },
        { JAN_1_2000_NS, "2000-01-01T16:35:42.10 (Sat)", 2000, 1, 1, 16, 35, 42, 10, 6 },
        { FEB_29_2000_NS, "2000-02-29T00:00:00.00 (Tue)", 2000, 2, 29, 0, 0, 0, 0, 2 },
        { NOV_5_1955_NS, "1955-11-05T01:21:59.03 (Sat)", 1955, 11, 5, 1, 21, 59, 3, 6 },
        { NOV_5_1955_1_NS, "1955-11-05T01:21:59.500000000 (Sat)", 1955, 11, 5, 1, 21, 59, 500000000, 6 }
    };
    int result;
    SDL_Time ticks;
    SDL_DateTime dt;

    for (int i = 0; i < SDL_arraysize(TimeTest); ++i) {
        result = SDL_TimeToDateTime(TimeTest[i].ticks, &dt, false);
        SDLTest_Log("Testing time conversion for %s", TimeTest[i].desc);
        SDLTest_AssertPass("Call to SDL_TimeToUTCDateTime()");
        SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);
        SDLTest_AssertCheck(dt.year == TimeTest[i].year, "Check year value, expected %i, got: %i", TimeTest[i].year, dt.year);
        SDLTest_AssertCheck(dt.month == TimeTest[i].month, "Check month value, expected %i, got: %i", TimeTest[i].month, dt.month);
        SDLTest_AssertCheck(dt.day == TimeTest[i].day, "Check day value, expected %i, got: %i", TimeTest[i].day, dt.day);
        SDLTest_AssertCheck(dt.hour == TimeTest[i].hour, "Check hour value, expected %i, got: %i", TimeTest[i].hour, dt.hour);
        SDLTest_AssertCheck(dt.minute == TimeTest[i].minute, "Check hour value, expected %i, got: %i", TimeTest[i].minute, dt.minute);
        SDLTest_AssertCheck(dt.second == TimeTest[i].second, "Check second value, expected %i, got: %i", TimeTest[i].second, dt.second);
        SDLTest_AssertCheck(dt.nanosecond == TimeTest[i].nanosecond, "Check nanosecond value, expected %i, got: %i", TimeTest[i].nanosecond, dt.nanosecond);
        SDLTest_AssertCheck(dt.day_of_week == TimeTest[i].day_of_week, "Check day of week, expected %i, got: %i", TimeTest[i].day_of_week, dt.day_of_week);

        result = SDL_DateTimeToTime(&dt, &ticks);
        SDLTest_AssertPass("Call to SDL_DateTimeToTime()");
        SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

        result = TimeTest[i].ticks == ticks;
        SDLTest_AssertCheck(result, "Check that original and converted SDL_Time values match: original = %" SDL_PRIs64 ", converted = %" SDL_PRIs64, TimeTest[i].ticks, ticks);

        /* Local time unknown, so just verify success. */
        result = SDL_TimeToDateTime(TimeTest[i].ticks, &dt, true);
        SDLTest_AssertPass("Call to SDL_TimeToLocalDateTime()");
        SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

        /* Convert back and verify result. */
        result = SDL_DateTimeToTime(&dt, &ticks);
        SDLTest_AssertPass("Call to SDL_DateTimeToTime()");
        SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

        result = TimeTest[i].ticks == ticks;
        SDLTest_AssertCheck(result, "Check that original and converted SDL_Time values match: original = %" SDL_PRIs64 ", converted = %" SDL_PRIs64, TimeTest[i].ticks, ticks);

        /* Advance the time one day. */
        ++dt.day;
        if (dt.day > SDL_GetDaysInMonth(dt.year, dt.month)) {
            dt.day = 1;
            ++dt.month;
        }
        if (dt.month > 12) {
            dt.month = 1;
            ++dt.year;
        }

        result = SDL_DateTimeToTime(&dt, &ticks);
        SDLTest_AssertPass("Call to SDL_DateTimeToTime() (one day advanced)");
        SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

        result = (TimeTest[i].ticks + (Sint64)SDL_SECONDS_TO_NS(86400)) == ticks;
        SDLTest_AssertCheck(result, "Check that the difference is exactly 86400 seconds, got: %" SDL_PRIs64, (Sint64)SDL_NS_TO_SECONDS(ticks - TimeTest[i].ticks));
    }

    /* Check dates that overflow/underflow an SDL_Time */
    dt.year = 2400;
    dt.month = 1;
    dt.day = 1;
    result = SDL_DateTimeToTime(&dt, &ticks);
    SDLTest_AssertPass("Call to SDL_DateTimeToTime() (year overflows an SDL_Time)");
    SDLTest_AssertCheck(result == false, "Check result value, expected false, got: %i", result);

    dt.year = 1601;
    result = SDL_DateTimeToTime(&dt, &ticks);
    SDLTest_AssertPass("Call to SDL_DateTimeToTime() (year underflows an SDL_Time)");
    SDLTest_AssertCheck(result == false, "Check result value, expected false, got: %i", result);

    return TEST_COMPLETED;
}

/**
 * Test time utility functions.
 */
static int SDLCALL time_dateTimeUtilities(void *arg)
{
    int result;

    /* Leap-year */
    result = SDL_GetDaysInMonth(2000, 2);
    SDLTest_AssertPass("Call to SDL_GetDaysInMonth(2000, 2)");
    SDLTest_AssertCheck(result == 29, "Check result value, expected 29, got: %i", result);

    result = SDL_GetDaysInMonth(2001, 2);
    SDLTest_AssertPass("Call to SDL_GetDaysInMonth(2001, 2)");
    SDLTest_AssertCheck(result == 28, "Check result value, expected 28, got: %i", result);

    result = SDL_GetDaysInMonth(2001, 13);
    SDLTest_AssertPass("Call to SDL_GetDaysInMonth(2001, 13)");
    SDLTest_AssertCheck(result == -1, "Check result value, expected -1, got: %i", result);

    result = SDL_GetDaysInMonth(2001, -1);
    SDLTest_AssertPass("Call to SDL_GetDaysInMonth(2001, 13)");
    SDLTest_AssertCheck(result == -1, "Check result value, expected -1, got: %i", result);

    /* 2000-02-29 was a Tuesday */
    result = SDL_GetDayOfWeek(2000, 2, 29);
    SDLTest_AssertPass("Call to SDL_GetDayOfWeek(2000, 2, 29)");
    SDLTest_AssertCheck(result == 2, "Check result value, expected %i, got: %i", 2, result);

    /* Nonexistent day */
    result = SDL_GetDayOfWeek(2001, 2, 29);
    SDLTest_AssertPass("Call to SDL_GetDayOfWeek(2001, 2, 29)");
    SDLTest_AssertCheck(result == -1, "Check result value, expected -1, got: %i", result);

    result = SDL_GetDayOfYear(2000, 1, 1);
    SDLTest_AssertPass("Call to SDL_GetDayOfWeek(2001, 1, 1)");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    /* Leap-year */
    result = SDL_GetDayOfYear(2000, 12, 31);
    SDLTest_AssertPass("Call to SDL_GetDayOfYear(2000, 12, 31)");
    SDLTest_AssertCheck(result == 365, "Check result value, expected 365, got: %i", result);

    result = SDL_GetDayOfYear(2001, 12, 31);
    SDLTest_AssertPass("Call to SDL_GetDayOfYear(2000, 12, 31)");
    SDLTest_AssertCheck(result == 364, "Check result value, expected 364, got: %i", result);

    /* Nonexistent day */
    result = SDL_GetDayOfYear(2001, 2, 29);
    SDLTest_AssertPass("Call to SDL_GetDayOfYear(2001, 2, 29)");
    SDLTest_AssertCheck(result == -1, "Check result value, expected -1, got: %i", result);

    /* Test Win32 time conversion */
    Uint64 wintime = 11644473600LL * 10000000LL; /* The epoch */
    SDL_Time ticks = SDL_TimeFromWindows((Uint32)(wintime & 0xFFFFFFFF), (Uint32)(wintime >> 32));
    SDLTest_AssertPass("Call to SDL_TimeFromWindows()");
    SDLTest_AssertCheck(ticks == 0, "Check result value, expected 0, got: %" SDL_PRIs64, ticks);

    /* Out of range times should be clamped instead of rolling over */
    wintime = 0;
    ticks = SDL_TimeFromWindows((Uint32)(wintime & 0xFFFFFFFF), (Uint32)(wintime >> 32));
    SDLTest_AssertPass("Call to SDL_TimeFromWindows()");
    SDLTest_AssertCheck(ticks < 0 && ticks >= SDL_MIN_TIME, "Check result value, expected <0 && >=%" SDL_PRIs64 ", got: %" SDL_PRIs64, SDL_MIN_TIME, ticks);

    wintime = 0xFFFFFFFFFFFFFFFFULL;
    ticks = SDL_TimeFromWindows((Uint32)(wintime & 0xFFFFFFFF), (Uint32)(wintime >> 32));
    SDLTest_AssertPass("Call to SDL_TimeFromWindows()");
    SDLTest_AssertCheck(ticks > 0 && ticks <= SDL_MAX_TIME, "Check result value, expected >0 && <=%" SDL_PRIs64 ", got: %" SDL_PRIs64, SDL_MAX_TIME, ticks);

    /* Test time locale functions */
    SDL_DateFormat dateFormat;
    SDL_TimeFormat timeFormat;

    result = SDL_GetDateTimeLocalePreferences(&dateFormat, &timeFormat);
    SDLTest_AssertPass("Call to SDL_GetDateTimeLocalePreferences(&dateFormat, &timeFormat)");
    SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

    result = SDL_GetDateTimeLocalePreferences(&dateFormat, NULL);
    SDLTest_AssertPass("Call to SDL_GetDateTimeLocalePreferences(&dateFormat, NULL)");
    SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

    result = SDL_GetDateTimeLocalePreferences(NULL, &timeFormat);
    SDLTest_AssertPass("Call to SDL_GetDateTimeLocalePreferences(NULL, &timeFormat)");
    SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

    result = SDL_GetDateTimeLocalePreferences(NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetDateTimeLocalePreferences(NULL, NULL)");
    SDLTest_AssertCheck(result == true, "Check result value, expected true, got: %i", result);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Time test cases */
static const SDLTest_TestCaseReference timeTest1 = {
    time_getRealtimeClock, "time_getRealtimeClock", "Call to SDL_GetRealtimeClockTicks", TEST_ENABLED
};

static const SDLTest_TestCaseReference timeTest2 = {
    time_dateTimeConversion, "time_dateTimeConversion", "Call to SDL_TimeToDateTime/SDL_DateTimeToTime", TEST_ENABLED
};

static const SDLTest_TestCaseReference timeTest3 = {
    time_dateTimeUtilities, "time_dateTimeUtilities", "Call to SDL_TimeToDateTime/SDL_DateTimeToTime", TEST_ENABLED
};

/* Sequence of Timer test cases */
static const SDLTest_TestCaseReference *timeTests[] = {
    &timeTest1, &timeTest2, &timeTest3, NULL
};

/* Time test suite (global) */
SDLTest_TestSuiteReference timeTestSuite = {
    "Time",
    NULL,
    timeTests,
    NULL
};
