/**
 * Math test suite
 */

#include "SDL.h"
#include "SDL_test.h"

static const SDLTest_TestCaseReference* mathTests[] = {NULL};

SDLTest_TestSuiteReference mathTestSuite = {"Math", NULL, mathTests, NULL};
