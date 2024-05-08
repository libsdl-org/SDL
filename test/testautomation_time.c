/**
 * Timer test suite
 */
#include "testautomation_suites.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>

/* 2000-01-01T16:35:42 UTC */
#define JAN_1_2000_NS SDL_SECONDS_TO_NS(946744542)

/* Test case functions */

/**
 * Call to SDL_GetRealtimeClock
 */
static int time_getRealtimeClock(void *arg)
{
    int result;
    SDL_Time ticks;

    result = SDL_GetCurrentTime(&ticks);
    SDLTest_AssertPass("Call to SDL_GetRealtimeClockTicks()");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    return TEST_COMPLETED;
}

/**
 * Test bidirectional SDL_DateTime conversions.
 */
static int time_dateTimeConversion(void *arg)
{
    int result;
    SDL_Time ticks[2];
    SDL_DateTime dt;

    ticks[0] = JAN_1_2000_NS;

    result = SDL_TimeToDateTime(ticks[0], &dt, SDL_FALSE);
    SDLTest_AssertPass("Call to SDL_TimeToUTCDateTime()");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);
    SDLTest_AssertCheck(dt.year == 2000, "Check year value, expected 2000, got: %i", dt.year);
    SDLTest_AssertCheck(dt.month == 1, "Check month value, expected 1, got: %i", dt.month);
    SDLTest_AssertCheck(dt.day == 1, "Check day value, expected 1, got: %i", dt.day);
    SDLTest_AssertCheck(dt.hour == 16, "Check hour value, expected 16, got: %i", dt.hour);
    SDLTest_AssertCheck(dt.minute == 35, "Check hour value, expected 35, got: %i", dt.minute);
    SDLTest_AssertCheck(dt.second == 42, "Check hour value, expected 42, got: %i", dt.second);

    result = SDL_DateTimeToTime(&dt, &ticks[1]);
    SDLTest_AssertPass("Call to SDL_DateTimeToTime()");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    result = ticks[0] == ticks[1];
    SDLTest_AssertCheck(result, "Check that original and converted SDL_Time values match: ticks0 = %" SDL_PRIs64 ", ticks1 = %" SDL_PRIs64, ticks[0], ticks[1]);

    /* Local time unknown, so just verify success. */
    result = SDL_TimeToDateTime(ticks[0], &dt, SDL_TRUE);
    SDLTest_AssertPass("Call to SDL_TimeToLocalDateTime()");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    /* Convert back and verify result. */
    result = SDL_DateTimeToTime(&dt, &ticks[1]);
    SDLTest_AssertPass("Call to SDL_DateTimeToTime()");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    result = ticks[0] == ticks[1];
    SDLTest_AssertCheck(result, "Check that original and converted SDL_Time values match: ticks0 = %" SDL_PRIs64 ", ticks1 = %" SDL_PRIs64, ticks[0], ticks[1]);

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

    result = SDL_DateTimeToTime(&dt, &ticks[1]);
    SDLTest_AssertPass("Call to SDL_DateTimeToTime() (one day advanced)");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    result = (ticks[0] + (Sint64)SDL_SECONDS_TO_NS(86400)) == ticks[1];
    SDLTest_AssertCheck(result, "Check that the difference is exactly 86400 seconds, got: %" SDL_PRIs64, (Sint64)SDL_NS_TO_SECONDS(ticks[1] - ticks[0]));

    /* Check dates that overflow/underflow an SDL_Time */
    dt.year = 2400;
    dt.month = 1;
    dt.day = 1;
    result = SDL_DateTimeToTime(&dt, &ticks[0]);
    SDLTest_AssertPass("Call to SDL_DateTimeToTime() (year overflows an SDL_Time)");
    SDLTest_AssertCheck(result == -1, "Check result value, expected -1, got: %i", result);

    dt.year = 1601;
    result = SDL_DateTimeToTime(&dt, &ticks[0]);
    SDLTest_AssertPass("Call to SDL_DateTimeToTime() (year underflows an SDL_Time)");
    SDLTest_AssertCheck(result == -1, "Check result value, expected -1, got: %i", result);

    return TEST_COMPLETED;
}

/**
 * Test time utility functions.
 */
static int time_dateTimeUtilities(void *arg)
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
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    result = SDL_GetDateTimeLocalePreferences(&dateFormat, NULL);
    SDLTest_AssertPass("Call to SDL_GetDateTimeLocalePreferences(&dateFormat, NULL)");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    result = SDL_GetDateTimeLocalePreferences(NULL, &timeFormat);
    SDLTest_AssertPass("Call to SDL_GetDateTimeLocalePreferences(NULL, &timeFormat)");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    result = SDL_GetDateTimeLocalePreferences(NULL, NULL);
    SDLTest_AssertPass("Call to SDL_GetDateTimeLocalePreferences(NULL, NULL)");
    SDLTest_AssertCheck(result == 0, "Check result value, expected 0, got: %i", result);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Time test cases */
static const SDLTest_TestCaseReference timeTest1 = {
    (SDLTest_TestCaseFp)time_getRealtimeClock, "time_getRealtimeClock", "Call to SDL_GetRealtimeClockTicks", TEST_ENABLED
};

static const SDLTest_TestCaseReference timeTest2 = {
    (SDLTest_TestCaseFp)time_dateTimeConversion, "time_dateTimeConversion", "Call to SDL_TimeToDateTime/SDL_DateTimeToTime", TEST_ENABLED
};

static const SDLTest_TestCaseReference timeTest3 = {
    (SDLTest_TestCaseFp)time_dateTimeUtilities, "time_dateTimeUtilities", "Call to SDL_TimeToDateTime/SDL_DateTimeToTime", TEST_ENABLED
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
