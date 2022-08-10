/**
 * Hints test suite
 */

#include <stdio.h>

#include "SDL.h"
#include "SDL_test.h"


const char* _HintsEnum[] =
  {
    SDL_HINT_ACCELEROMETER_AS_JOYSTICK,
    SDL_HINT_FRAMEBUFFER_ACCELERATION,
    SDL_HINT_GAMECONTROLLERCONFIG,
    SDL_HINT_GRAB_KEYBOARD,
    SDL_HINT_IDLE_TIMER_DISABLED,
    SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
    SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK,
    SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
    SDL_HINT_ORIENTATIONS,
    SDL_HINT_RENDER_DIRECT3D_THREADSAFE,
    SDL_HINT_RENDER_DRIVER,
    SDL_HINT_RENDER_OPENGL_SHADERS,
    SDL_HINT_RENDER_SCALE_QUALITY,
    SDL_HINT_RENDER_VSYNC,
    SDL_HINT_TIMER_RESOLUTION,
    SDL_HINT_VIDEO_ALLOW_SCREENSAVER,
    SDL_HINT_VIDEO_HIGHDPI_DISABLED,
    SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES,
    SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,
    SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT,
    SDL_HINT_VIDEO_WIN_D3DCOMPILER,
    SDL_HINT_VIDEO_X11_XRANDR,
    SDL_HINT_XINPUT_ENABLED,
  };
const char* _HintsVerbose[] =
  {
    "SDL_ACCELEROMETER_AS_JOYSTICK",
    "SDL_FRAMEBUFFER_ACCELERATION",
    "SDL_GAMECONTROLLERCONFIG",
    "SDL_GRAB_KEYBOARD",
    "SDL_IDLE_TIMER_DISABLED",
    "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS",
    "SDL_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK",
    "SDL_MOUSE_RELATIVE_MODE_WARP",
    "SDL_ORIENTATIONS",
    "SDL_RENDER_DIRECT3D_THREADSAFE",
    "SDL_RENDER_DRIVER",
    "SDL_RENDER_OPENGL_SHADERS",
    "SDL_RENDER_SCALE_QUALITY",
    "SDL_RENDER_VSYNC",
    "SDL_TIMER_RESOLUTION",
    "SDL_VIDEO_ALLOW_SCREENSAVER",
    "SDL_VIDEO_HIGHDPI_DISABLED",
    "SDL_VIDEO_MAC_FULLSCREEN_SPACES",
    "SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS",
    "SDL_VIDEO_WINDOW_SHARE_PIXEL_FORMAT",
    "SDL_VIDEO_WIN_D3DCOMPILER",
    "SDL_VIDEO_X11_XRANDR",
    "SDL_XINPUT_ENABLED"
  };

SDL_COMPILE_TIME_ASSERT(HintsEnum, SDL_arraysize(_HintsEnum) == SDL_arraysize(_HintsVerbose));

const int _numHintsEnum = SDL_arraysize(_HintsEnum);

/* Test case functions */

/**
 * @brief Call to SDL_GetHint
 */
int
hints_getHint(void *arg)
{
  const char *result1;
  const char *result2;
  int i;

  for (i=0; i<_numHintsEnum; i++) {
    result1 = SDL_GetHint(_HintsEnum[i]);
    SDLTest_AssertPass("Call to SDL_GetHint(%s) - using define definition", (char*)_HintsEnum[i]);
    result2 = SDL_GetHint(_HintsVerbose[i]);
    SDLTest_AssertPass("Call to SDL_GetHint(%s) - using string definition", (char*)_HintsVerbose[i]);
    SDLTest_AssertCheck(
      (result1 == NULL && result2 == NULL) || (SDL_strcmp(result1, result2) == 0),
      "Verify returned values are equal; got: result1='%s' result2='%s",
      (result1 == NULL) ? "null" : result1,
      (result2 == NULL) ? "null" : result2);
  }

  return TEST_COMPLETED;
}

/**
 * @brief Call to SDL_SetHint
 */
int
hints_setHint(void *arg)
{
  const char *testHint = "SDL_AUTOMATED_TEST_HINT";
  const char *originalValue;
  char *value;
  const char *testValue;
  SDL_bool result;
  int i, j;

  /* Create random values to set */
  value = SDLTest_RandomAsciiStringOfSize(10);

  for (i=0; i<_numHintsEnum; i++) {
    /* Capture current value */
    originalValue = SDL_GetHint(_HintsEnum[i]);
    SDLTest_AssertPass("Call to SDL_GetHint(%s)", _HintsEnum[i]);

    /* Copy the original value, since it will be freed when we set it again */
    originalValue = originalValue ? SDL_strdup(originalValue) : NULL;

    /* Set value (twice) */
    for (j=1; j<=2; j++) {
      result = SDL_SetHint(_HintsEnum[i], value);
      SDLTest_AssertPass("Call to SDL_SetHint(%s, %s) (iteration %i)", _HintsEnum[i], value, j);
      SDLTest_AssertCheck(
        result == SDL_TRUE || result == SDL_FALSE, 
        "Verify valid result was returned, got: %i",
        (int)result);
      testValue = SDL_GetHint(_HintsEnum[i]);
      SDLTest_AssertPass("Call to SDL_GetHint(%s) - using string definition", _HintsVerbose[i]);
      SDLTest_AssertCheck(
        (SDL_strcmp(value, testValue) == 0),
        "Verify returned value equals set value; got: testValue='%s' value='%s",
        (testValue == NULL) ? "null" : testValue,
        value);
    }

    /* Reset original value */
    result = SDL_SetHint(_HintsEnum[i], originalValue);
    SDLTest_AssertPass("Call to SDL_SetHint(%s, originalValue)", _HintsEnum[i]);
    SDLTest_AssertCheck(
      result == SDL_TRUE || result == SDL_FALSE, 
      "Verify valid result was returned, got: %i",
      (int)result);
    SDL_free((void *)originalValue);
  }

  SDL_free(value);

  /* Set default value in environment */
  SDL_setenv(testHint, "original", 1);

  SDLTest_AssertPass("Call to SDL_GetHint() after saving and restoring hint");
  originalValue = SDL_GetHint(testHint);
  value = (originalValue == NULL) ? NULL : SDL_strdup(originalValue);
  SDL_SetHint(testHint, "temp");
  SDL_SetHint(testHint, value);
  SDL_free(value);
  testValue = SDL_GetHint(testHint);
  SDLTest_AssertCheck(
    testValue && SDL_strcmp(testValue, "original") == 0,
    "testValue = %s, expected \"original\"",
    testValue);

  SDLTest_AssertPass("Call to SDL_SetHintWithPriority(NULL, SDL_HINT_DEFAULT)");
  SDL_SetHintWithPriority(testHint, NULL, SDL_HINT_DEFAULT);
  testValue = SDL_GetHint(testHint);
  SDLTest_AssertCheck(
    testValue && SDL_strcmp(testValue, "original") == 0,
    "testValue = %s, expected \"original\"",
    testValue);

  SDLTest_AssertPass("Call to SDL_SetHintWithPriority(\"temp\", SDL_HINT_OVERRIDE)");
  SDL_SetHintWithPriority(testHint, "temp", SDL_HINT_OVERRIDE);
  testValue = SDL_GetHint(testHint);
  SDLTest_AssertCheck(
    testValue && SDL_strcmp(testValue, "temp") == 0,
    "testValue = %s, expected \"temp\"",
    testValue);

  SDLTest_AssertPass("Call to SDL_SetHintWithPriority(NULL, SDL_HINT_OVERRIDE)");
  SDL_SetHintWithPriority(testHint, NULL, SDL_HINT_OVERRIDE);
  testValue = SDL_GetHint(testHint);
  SDLTest_AssertCheck(
    testValue == NULL,
    "testValue = %s, expected NULL",
    testValue);

  SDLTest_AssertPass("Call to SDL_ResetHint()");
  SDL_ResetHint(testHint);
  testValue = SDL_GetHint(testHint);
  SDLTest_AssertCheck(
    testValue && SDL_strcmp(testValue, "original") == 0,
    "testValue = %s, expected \"original\"",
    testValue);

  return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Hints test cases */
static const SDLTest_TestCaseReference hintsTest1 =
        { (SDLTest_TestCaseFp)hints_getHint, "hints_getHint", "Call to SDL_GetHint", TEST_ENABLED };

static const SDLTest_TestCaseReference hintsTest2 =
        { (SDLTest_TestCaseFp)hints_setHint, "hints_setHint", "Call to SDL_SetHint", TEST_ENABLED };

/* Sequence of Hints test cases */
static const SDLTest_TestCaseReference *hintsTests[] =  {
    &hintsTest1, &hintsTest2, NULL
};

/* Hints test suite (global) */
SDLTest_TestSuiteReference hintsTestSuite = {
    "Hints",
    NULL,
    hintsTests,
    NULL
};
