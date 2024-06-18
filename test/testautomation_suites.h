/**
 * Reference to all test suites.
 *
 */

#ifndef _testsuites_h
#define _testsuites_h

#include <SDL3/SDL_test.h>

#define ISFINITE(X) isfinite((float)(X))
#define ISINF(X)    isinf((float)(X))
#define ISNAN(X)    isnan((float)(X))

/* Test collections */
extern SDLTest_TestSuiteReference audioTestSuite;
extern SDLTest_TestSuiteReference clipboardTestSuite;
extern SDLTest_TestSuiteReference eventsTestSuite;
extern SDLTest_TestSuiteReference guidTestSuite;
extern SDLTest_TestSuiteReference hintsTestSuite;
extern SDLTest_TestSuiteReference intrinsicsTestSuite;
extern SDLTest_TestSuiteReference joystickTestSuite;
extern SDLTest_TestSuiteReference keyboardTestSuite;
extern SDLTest_TestSuiteReference logTestSuite;
extern SDLTest_TestSuiteReference mainTestSuite;
extern SDLTest_TestSuiteReference mathTestSuite;
extern SDLTest_TestSuiteReference mouseTestSuite;
extern SDLTest_TestSuiteReference penTestSuite;
extern SDLTest_TestSuiteReference pixelsTestSuite;
extern SDLTest_TestSuiteReference platformTestSuite;
extern SDLTest_TestSuiteReference propertiesTestSuite;
extern SDLTest_TestSuiteReference rectTestSuite;
extern SDLTest_TestSuiteReference renderTestSuite;
extern SDLTest_TestSuiteReference iostrmTestSuite;
extern SDLTest_TestSuiteReference sdltestTestSuite;
extern SDLTest_TestSuiteReference stdlibTestSuite;
extern SDLTest_TestSuiteReference subsystemsTestSuite;
extern SDLTest_TestSuiteReference surfaceTestSuite;
extern SDLTest_TestSuiteReference timeTestSuite;
extern SDLTest_TestSuiteReference timerTestSuite;
extern SDLTest_TestSuiteReference videoTestSuite;
extern SDLTest_TestSuiteReference blitTestSuite;

#endif
