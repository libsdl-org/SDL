/**
 * Original code: automated SDL rect test written by Edgar Simo "bobbens"
 * New/updated tests: aschiffler at ferzkopp dot net
 */
#include <limits.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

/* Helper functions */

/**
 * Private helper to check SDL_GetRectAndLineIntersectionFloat results
 */
static void validateIntersectRectAndLineFloatResults(
    bool intersection, bool expectedIntersection,
    SDL_FRect *rect,
    float x1, float y1, float x2, float y2,
    float x1Ref, float y1Ref, float x2Ref, float y2Ref)
{
    SDLTest_AssertCheck(intersection == expectedIntersection,
                        "Check for correct intersection result: expected %s, got %s intersecting rect (%.2f,%.2f,%.2f,%.2f) with line (%.2f,%.2f - %.2f,%.2f)",
                        (expectedIntersection == true) ? "true" : "false",
                        (intersection == true) ? "true" : "false",
                        rect->x, rect->y, rect->w, rect->h,
                        x1Ref, y1Ref, x2Ref, y2Ref);
    SDLTest_AssertCheck(x1 == x1Ref && y1 == y1Ref && x2 == x2Ref && y2 == y2Ref,
                        "Check if line was incorrectly clipped or modified: got (%.2f,%.2f - %.2f,%.2f) expected (%.2f,%.2f - %.2f,%.2f)",
                        x1, y1, x2, y2,
                        x1Ref, y1Ref, x2Ref, y2Ref);
}

/**
 * Private helper to check SDL_GetRectAndLineIntersection results
 */
static void validateIntersectRectAndLineResults(
    bool intersection, bool expectedIntersection,
    SDL_Rect *rect, SDL_Rect *refRect,
    int x1, int y1, int x2, int y2,
    int x1Ref, int y1Ref, int x2Ref, int y2Ref)
{
    SDLTest_AssertCheck(intersection == expectedIntersection,
                        "Check for correct intersection result: expected %s, got %s intersecting rect (%d,%d,%d,%d) with line (%d,%d - %d,%d)",
                        (expectedIntersection == true) ? "true" : "false",
                        (intersection == true) ? "true" : "false",
                        refRect->x, refRect->y, refRect->w, refRect->h,
                        x1Ref, y1Ref, x2Ref, y2Ref);
    SDLTest_AssertCheck(rect->x == refRect->x && rect->y == refRect->y && rect->w == refRect->w && rect->h == refRect->h,
                        "Check that source rectangle was not modified: got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rect->x, rect->y, rect->w, rect->h,
                        refRect->x, refRect->y, refRect->w, refRect->h);
    SDLTest_AssertCheck(x1 == x1Ref && y1 == y1Ref && x2 == x2Ref && y2 == y2Ref,
                        "Check if line was incorrectly clipped or modified: got (%d,%d - %d,%d) expected (%d,%d - %d,%d)",
                        x1, y1, x2, y2,
                        x1Ref, y1Ref, x2Ref, y2Ref);
}

/* Test case functions */

/**
 * Tests SDL_GetRectAndLineIntersectionFloat() clipping cases
 *
 * \sa SDL_GetRectAndLineIntersectionFloat
 */
static int SDLCALL rect_testIntersectRectAndLineFloat(void *arg)
{
    SDL_FRect rect;
    float x1, y1;
    float x2, y2;
    bool intersected;

    x1 = 5.0f;
    y1 = 6.0f;
    x2 = 23.0f;
    y2 = 6.0f;
    rect.x = 2.5f;
    rect.y = 1.5f;
    rect.w = 15.25f;
    rect.h = 12.0f;
    intersected = SDL_GetRectAndLineIntersectionFloat(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineFloatResults(intersected, true, &rect, x1, y1, x2, y2, 5.0f, 6.0f, 17.75f, 6.0f);

    x1 = 0.0f;
    y1 = 6.0f;
    x2 = 23.0f;
    y2 = 6.0f;
    rect.x = 2.5f;
    rect.y = 1.5f;
    rect.w = 0.25f;
    rect.h = 12.0f;
    intersected = SDL_GetRectAndLineIntersectionFloat(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineFloatResults(intersected, true, &rect, x1, y1, x2, y2, 2.5f, 6.0f, 2.75f, 6.0f);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectAndLineIntersection() clipping cases
 *
 * \sa SDL_GetRectAndLineIntersection
 */
static int SDLCALL rect_testIntersectRectAndLine(void *arg)
{
    SDL_Rect refRect = { 0, 0, 32, 32 };
    SDL_Rect rect;
    int x1, y1;
    int x2, y2;
    bool intersected;

    int xLeft = -SDLTest_RandomIntegerInRange(1, refRect.w);
    int xRight = refRect.w + SDLTest_RandomIntegerInRange(1, refRect.w);
    int yTop = -SDLTest_RandomIntegerInRange(1, refRect.h);
    int yBottom = refRect.h + SDLTest_RandomIntegerInRange(1, refRect.h);

    x1 = xLeft;
    y1 = 15;
    x2 = xRight;
    y2 = 15;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, 0, 15, 31, 15);

    x1 = 15;
    y1 = yTop;
    x2 = 15;
    y2 = yBottom;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, 15, 0, 15, 31);

    x1 = -refRect.w;
    y1 = -refRect.h;
    x2 = 2 * refRect.w;
    y2 = 2 * refRect.h;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, 0, 0, 31, 31);

    x1 = 2 * refRect.w;
    y1 = 2 * refRect.h;
    x2 = -refRect.w;
    y2 = -refRect.h;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, 31, 31, 0, 0);

    x1 = -1;
    y1 = 32;
    x2 = 32;
    y2 = -1;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, 0, 31, 31, 0);

    x1 = 32;
    y1 = -1;
    x2 = -1;
    y2 = 32;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, 31, 0, 0, 31);

    /* Test some overflow cases */
    refRect.x = INT_MAX - 4;
    refRect.y = INT_MAX - 4;
    x1 = INT_MAX;
    y1 = INT_MIN;
    x2 = INT_MIN;
    y2 = INT_MAX;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, false, &rect, &refRect, x1, y1, x2, y2, x1, y1, x2, y2);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectAndLineIntersection() non-clipping case line inside
 *
 * \sa SDL_GetRectAndLineIntersection
 */
static int SDLCALL rect_testIntersectRectAndLineInside(void *arg)
{
    SDL_Rect refRect = { 0, 0, 32, 32 };
    SDL_Rect rect;
    int x1, y1;
    int x2, y2;
    bool intersected;

    int xmin = refRect.x;
    int xmax = refRect.x + refRect.w - 1;
    int ymin = refRect.y;
    int ymax = refRect.y + refRect.h - 1;
    int x1Ref = SDLTest_RandomIntegerInRange(xmin + 1, xmax - 1);
    int y1Ref = SDLTest_RandomIntegerInRange(ymin + 1, ymax - 1);
    int x2Ref = SDLTest_RandomIntegerInRange(xmin + 1, xmax - 1);
    int y2Ref = SDLTest_RandomIntegerInRange(ymin + 1, ymax - 1);

    x1 = x1Ref;
    y1 = y1Ref;
    x2 = x2Ref;
    y2 = y2Ref;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, x1Ref, y1Ref, x2Ref, y2Ref);

    x1 = x1Ref;
    y1 = y1Ref;
    x2 = xmax;
    y2 = ymax;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, x1Ref, y1Ref, xmax, ymax);

    x1 = xmin;
    y1 = ymin;
    x2 = x2Ref;
    y2 = y2Ref;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, xmin, ymin, x2Ref, y2Ref);

    x1 = xmin;
    y1 = ymin;
    x2 = xmax;
    y2 = ymax;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, xmin, ymin, xmax, ymax);

    x1 = xmin;
    y1 = ymax;
    x2 = xmax;
    y2 = ymin;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, true, &rect, &refRect, x1, y1, x2, y2, xmin, ymax, xmax, ymin);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectAndLineIntersection() non-clipping cases outside
 *
 * \sa SDL_GetRectAndLineIntersection
 */
static int SDLCALL rect_testIntersectRectAndLineOutside(void *arg)
{
    SDL_Rect refRect = { 0, 0, 32, 32 };
    SDL_Rect rect;
    int x1, y1;
    int x2, y2;
    bool intersected;

    int xLeft = -SDLTest_RandomIntegerInRange(1, refRect.w);
    int xRight = refRect.w + SDLTest_RandomIntegerInRange(1, refRect.w);
    int yTop = -SDLTest_RandomIntegerInRange(1, refRect.h);
    int yBottom = refRect.h + SDLTest_RandomIntegerInRange(1, refRect.h);

    x1 = xLeft;
    y1 = 0;
    x2 = xLeft;
    y2 = 31;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, false, &rect, &refRect, x1, y1, x2, y2, xLeft, 0, xLeft, 31);

    x1 = xRight;
    y1 = 0;
    x2 = xRight;
    y2 = 31;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, false, &rect, &refRect, x1, y1, x2, y2, xRight, 0, xRight, 31);

    x1 = 0;
    y1 = yTop;
    x2 = 31;
    y2 = yTop;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, false, &rect, &refRect, x1, y1, x2, y2, 0, yTop, 31, yTop);

    x1 = 0;
    y1 = yBottom;
    x2 = 31;
    y2 = yBottom;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, false, &rect, &refRect, x1, y1, x2, y2, 0, yBottom, 31, yBottom);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectAndLineIntersection() with empty rectangle
 *
 * \sa SDL_GetRectAndLineIntersection
 */
static int SDLCALL rect_testIntersectRectAndLineEmpty(void *arg)
{
    SDL_Rect refRect;
    SDL_Rect rect;
    int x1, y1, x1Ref, y1Ref;
    int x2, y2, x2Ref, y2Ref;
    bool intersected;

    refRect.x = SDLTest_RandomIntegerInRange(1, 1024);
    refRect.y = SDLTest_RandomIntegerInRange(1, 1024);
    refRect.w = 0;
    refRect.h = 0;
    x1Ref = refRect.x;
    y1Ref = refRect.y;
    x2Ref = SDLTest_RandomIntegerInRange(1, 1024);
    y2Ref = SDLTest_RandomIntegerInRange(1, 1024);

    x1 = x1Ref;
    y1 = y1Ref;
    x2 = x2Ref;
    y2 = y2Ref;
    rect = refRect;
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    validateIntersectRectAndLineResults(intersected, false, &rect, &refRect, x1, y1, x2, y2, x1Ref, y1Ref, x2Ref, y2Ref);

    return TEST_COMPLETED;
}

/**
 * Negative tests against SDL_GetRectAndLineIntersection() with invalid parameters
 *
 * \sa SDL_GetRectAndLineIntersection
 */
static int SDLCALL rect_testIntersectRectAndLineParam(void *arg)
{
    SDL_Rect rect = { 0, 0, 32, 32 };
    int x1 = rect.w / 2;
    int y1 = rect.h / 2;
    int x2 = x1;
    int y2 = 2 * rect.h;
    bool intersected;

    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2);
    SDLTest_AssertCheck(intersected == true, "Check that intersection result was true");

    intersected = SDL_GetRectAndLineIntersection((SDL_Rect *)NULL, &x1, &y1, &x2, &y2);
    SDLTest_AssertCheck(intersected == false, "Check that function returns false when 1st parameter is NULL");
    intersected = SDL_GetRectAndLineIntersection(&rect, (int *)NULL, &y1, &x2, &y2);
    SDLTest_AssertCheck(intersected == false, "Check that function returns false when 2nd parameter is NULL");
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, (int *)NULL, &x2, &y2);
    SDLTest_AssertCheck(intersected == false, "Check that function returns false when 3rd parameter is NULL");
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, (int *)NULL, &y2);
    SDLTest_AssertCheck(intersected == false, "Check that function returns false when 4th parameter is NULL");
    intersected = SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, (int *)NULL);
    SDLTest_AssertCheck(intersected == false, "Check that function returns false when 5th parameter is NULL");
    intersected = SDL_GetRectAndLineIntersection((SDL_Rect *)NULL, (int *)NULL, (int *)NULL, (int *)NULL, (int *)NULL);
    SDLTest_AssertCheck(intersected == false, "Check that function returns false when all parameters are NULL");

    return TEST_COMPLETED;
}

/**
 * Private helper to check SDL_HasRectIntersectionFloat results
 */
static void validateHasIntersectionFloatResults(
    bool intersection, bool expectedIntersection,
    SDL_FRect *rectA, SDL_FRect *rectB)
{
    SDLTest_AssertCheck(intersection == expectedIntersection,
                        "Check intersection result: expected %s, got %s intersecting A (%.2f,%.2f,%.2f,%.2f) with B (%.2f,%.2f,%.2f,%.2f)",
                        (expectedIntersection == true) ? "true" : "false",
                        (intersection == true) ? "true" : "false",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        rectB->x, rectB->y, rectB->w, rectB->h);
}

/**
 * Private helper to check SDL_HasRectIntersection results
 */
static void validateHasIntersectionResults(
    bool intersection, bool expectedIntersection,
    SDL_Rect *rectA, SDL_Rect *rectB, SDL_Rect *refRectA, SDL_Rect *refRectB)
{
    SDLTest_AssertCheck(intersection == expectedIntersection,
                        "Check intersection result: expected %s, got %s intersecting A (%d,%d,%d,%d) with B (%d,%d,%d,%d)",
                        (expectedIntersection == true) ? "true" : "false",
                        (intersection == true) ? "true" : "false",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        rectB->x, rectB->y, rectB->w, rectB->h);
    SDLTest_AssertCheck(rectA->x == refRectA->x && rectA->y == refRectA->y && rectA->w == refRectA->w && rectA->h == refRectA->h,
                        "Check that source rectangle A was not modified: got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        refRectA->x, refRectA->y, refRectA->w, refRectA->h);
    SDLTest_AssertCheck(rectB->x == refRectB->x && rectB->y == refRectB->y && rectB->w == refRectB->w && rectB->h == refRectB->h,
                        "Check that source rectangle B was not modified: got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rectB->x, rectB->y, rectB->w, rectB->h,
                        refRectB->x, refRectB->y, refRectB->w, refRectB->h);
}

/**
 * Private helper to check SDL_GetRectIntersection results
 */
static void validateIntersectRectFloatResults(
    bool intersection, bool expectedIntersection,
    SDL_FRect *rectA, SDL_FRect *rectB,
    SDL_FRect *result, SDL_FRect *expectedResult)
{
    validateHasIntersectionFloatResults(intersection, expectedIntersection, rectA, rectB);
    if (result && expectedResult) {
        SDLTest_AssertCheck(result->x == expectedResult->x && result->y == expectedResult->y && result->w == expectedResult->w && result->h == expectedResult->h,
                            "Check that intersection of rectangles A (%.2f,%.2f, %.2fx%.2f) and B (%.2f,%.2f %.2fx%.2f) was correctly calculated, got (%.2f,%.2f %.2fx%.2f) expected (%.2f,%.2f,%.2f,%.2f)",
                            rectA->x, rectA->y, rectA->w, rectA->h,
                            rectB->x, rectB->y, rectB->w, rectB->h,
                            result->x, result->y, result->w, result->h,
                            expectedResult->x, expectedResult->y, expectedResult->w, expectedResult->h);
    }
    SDLTest_AssertCheck(intersection == SDL_HasRectIntersectionFloat(rectA, rectB),
                        "Check that intersection (%s) matches SDL_HasRectIntersectionFloat() result (%s)",
                        intersection ? "true" : "false",
                        SDL_HasRectIntersectionFloat(rectA, rectB) ? "true" : "false");
}

/**
 * Private helper to check SDL_GetRectIntersection results
 */
static void validateIntersectRectResults(
    bool intersection, bool expectedIntersection,
    SDL_Rect *rectA, SDL_Rect *rectB, SDL_Rect *refRectA, SDL_Rect *refRectB,
    SDL_Rect *result, SDL_Rect *expectedResult)
{
    validateHasIntersectionResults(intersection, expectedIntersection, rectA, rectB, refRectA, refRectB);
    if (result && expectedResult) {
        SDLTest_AssertCheck(result->x == expectedResult->x && result->y == expectedResult->y && result->w == expectedResult->w && result->h == expectedResult->h,
                            "Check that intersection of rectangles A (%d,%d,%d,%d) and B (%d,%d,%d,%d) was correctly calculated, got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                            rectA->x, rectA->y, rectA->w, rectA->h,
                            rectB->x, rectB->y, rectB->w, rectB->h,
                            result->x, result->y, result->w, result->h,
                            expectedResult->x, expectedResult->y, expectedResult->w, expectedResult->h);
    }
}

/**
 * Private helper to check SDL_GetRectUnion results
 */
static void validateUnionRectResults(
    SDL_Rect *rectA, SDL_Rect *rectB, SDL_Rect *refRectA, SDL_Rect *refRectB,
    SDL_Rect *result, SDL_Rect *expectedResult)
{
    SDLTest_AssertCheck(rectA->x == refRectA->x && rectA->y == refRectA->y && rectA->w == refRectA->w && rectA->h == refRectA->h,
                        "Check that source rectangle A was not modified: got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        refRectA->x, refRectA->y, refRectA->w, refRectA->h);
    SDLTest_AssertCheck(rectB->x == refRectB->x && rectB->y == refRectB->y && rectB->w == refRectB->w && rectB->h == refRectB->h,
                        "Check that source rectangle B was not modified: got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rectB->x, rectB->y, rectB->w, rectB->h,
                        refRectB->x, refRectB->y, refRectB->w, refRectB->h);
    SDLTest_AssertCheck(result->x == expectedResult->x && result->y == expectedResult->y && result->w == expectedResult->w && result->h == expectedResult->h,
                        "Check that union of rectangles A (%d,%d,%d,%d) and B (%d,%d,%d,%d) was correctly calculated, got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        rectB->x, rectB->y, rectB->w, rectB->h,
                        result->x, result->y, result->w, result->h,
                        expectedResult->x, expectedResult->y, expectedResult->w, expectedResult->h);
}

/**
 * Private helper to check SDL_RectEmptyFloat results
 */
static void validateRectEmptyFloatResults(
    bool empty, bool expectedEmpty,
    SDL_FRect *rect)
{
    SDLTest_AssertCheck(empty == expectedEmpty,
                        "Check for correct empty result: expected %s, got %s testing (%.2f,%.2f,%.2f,%.2f)",
                        (expectedEmpty == true) ? "true" : "false",
                        (empty == true) ? "true" : "false",
                        rect->x, rect->y, rect->w, rect->h);
}

/**
 * Private helper to check SDL_RectEmpty results
 */
static void validateRectEmptyResults(
    bool empty, bool expectedEmpty,
    SDL_Rect *rect, SDL_Rect *refRect)
{
    SDLTest_AssertCheck(empty == expectedEmpty,
                        "Check for correct empty result: expected %s, got %s testing (%d,%d,%d,%d)",
                        (expectedEmpty == true) ? "true" : "false",
                        (empty == true) ? "true" : "false",
                        rect->x, rect->y, rect->w, rect->h);
    SDLTest_AssertCheck(rect->x == refRect->x && rect->y == refRect->y && rect->w == refRect->w && rect->h == refRect->h,
                        "Check that source rectangle was not modified: got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rect->x, rect->y, rect->w, rect->h,
                        refRect->x, refRect->y, refRect->w, refRect->h);
}

/**
 * Private helper to check SDL_RectsEqual results
 */
static void validateRectEqualsResults(
    bool equals, bool expectedEquals,
    SDL_Rect *rectA, SDL_Rect *rectB, SDL_Rect *refRectA, SDL_Rect *refRectB)
{
    SDLTest_AssertCheck(equals == expectedEquals,
                        "Check for correct equals result: expected %s, got %s testing (%d,%d,%d,%d) and (%d,%d,%d,%d)",
                        (expectedEquals == true) ? "true" : "false",
                        (equals == true) ? "true" : "false",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        rectB->x, rectB->y, rectB->w, rectB->h);
    SDLTest_AssertCheck(rectA->x == refRectA->x && rectA->y == refRectA->y && rectA->w == refRectA->w && rectA->h == refRectA->h,
                        "Check that source rectangle A was not modified: got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        refRectA->x, refRectA->y, refRectA->w, refRectA->h);
    SDLTest_AssertCheck(rectB->x == refRectB->x && rectB->y == refRectB->y && rectB->w == refRectB->w && rectB->h == refRectB->h,
                        "Check that source rectangle B was not modified: got (%d,%d,%d,%d) expected (%d,%d,%d,%d)",
                        rectB->x, rectB->y, rectB->w, rectB->h,
                        refRectB->x, refRectB->y, refRectB->w, refRectB->h);
}

/**
 * Private helper to check SDL_RectsEqualFloat results
 */
static void validateFRectEqualsResults(
    bool equals, bool expectedEquals,
    SDL_FRect *rectA, SDL_FRect *rectB, SDL_FRect *refRectA, SDL_FRect *refRectB)
{
    int cmpRes;
    SDLTest_AssertCheck(equals == expectedEquals,
                        "Check for correct equals result: expected %s, got %s testing (%f,%f,%f,%f) and (%f,%f,%f,%f)",
                        (expectedEquals == true) ? "true" : "false",
                        (equals == true) ? "true" : "false",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        rectB->x, rectB->y, rectB->w, rectB->h);
    cmpRes = SDL_memcmp(rectA, refRectA, sizeof(*rectA));
    SDLTest_AssertCheck(cmpRes == 0,
                        "Check that source rectangle A was not modified: got (%f,%f,%f,%f) expected (%f,%f,%f,%f)",
                        rectA->x, rectA->y, rectA->w, rectA->h,
                        refRectA->x, refRectA->y, refRectA->w, refRectA->h);
    cmpRes = SDL_memcmp(rectB, refRectB, sizeof(*rectB));
    SDLTest_AssertCheck(cmpRes == 0,
                        "Check that source rectangle B was not modified: got (%f,%f,%f,%f) expected (%f,%f,%f,%f)",
                        rectB->x, rectB->y, rectB->w, rectB->h,
                        refRectB->x, refRectB->y, refRectB->w, refRectB->h);
}

/**
 * Tests SDL_GetRectIntersectionFloat()
 *
 * \sa SDL_GetRectIntersectionFloat
 */
static int SDLCALL rect_testIntersectRectFloat(void *arg)
{
    SDL_FRect rectA;
    SDL_FRect rectB;
    SDL_FRect result;
    SDL_FRect expectedResult;
    bool intersection;

    rectA.x = 0.0f;
    rectA.y = 0.0f;
    rectA.w = 1.0f;
    rectA.h = 1.0f;
    rectB.x = 0.0f;
    rectB.y = 0.0f;
    rectB.w = 1.0f;
    rectB.h = 1.0f;
    expectedResult = rectA;
    intersection = SDL_GetRectIntersectionFloat(&rectA, &rectB, &result);
    validateIntersectRectFloatResults(intersection, true, &rectA, &rectB, &result, &expectedResult);

    rectA.x = 0.0f;
    rectA.y = 0.0f;
    rectA.w = 1.0f;
    rectA.h = 1.0f;
    rectB.x = 1.0f;
    rectB.y = 0.0f;
    rectB.w = 1.0f;
    rectB.h = 1.0f;
    expectedResult = rectB;
    expectedResult.w = 0.0f;
    intersection = SDL_GetRectIntersectionFloat(&rectA, &rectB, &result);
    validateIntersectRectFloatResults(intersection, true, &rectA, &rectB, &result, &expectedResult);

    rectA.x = 0.0f;
    rectA.y = 0.0f;
    rectA.w = 1.0f;
    rectA.h = 1.0f;
    rectB.x = 1.0f;
    rectB.y = 1.0f;
    rectB.w = 1.0f;
    rectB.h = 1.0f;
    expectedResult = rectB;
    expectedResult.w = 0.0f;
    expectedResult.h = 0.0f;
    intersection = SDL_GetRectIntersectionFloat(&rectA, &rectB, &result);
    validateIntersectRectFloatResults(intersection, true, &rectA, &rectB, &result, &expectedResult);

    rectA.x = 0.0f;
    rectA.y = 0.0f;
    rectA.w = 1.0f;
    rectA.h = 1.0f;
    rectB.x = 2.0f;
    rectB.y = 0.0f;
    rectB.w = 1.0f;
    rectB.h = 1.0f;
    expectedResult = rectB;
    expectedResult.w = -1.0f;
    intersection = SDL_GetRectIntersectionFloat(&rectA, &rectB, &result);
    validateIntersectRectFloatResults(intersection, false, &rectA, &rectB, &result, &expectedResult);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectIntersection() with B fully inside A
 *
 * \sa SDL_GetRectIntersection
 */
static int SDLCALL rect_testIntersectRectInside(void *arg)
{
    SDL_Rect refRectA = { 0, 0, 32, 32 };
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    SDL_Rect result;
    bool intersection;

    /* rectB fully contained in rectA */
    refRectB.x = 0;
    refRectB.y = 0;
    refRectB.w = SDLTest_RandomIntegerInRange(refRectA.x + 1, refRectA.x + refRectA.w - 1);
    refRectB.h = SDLTest_RandomIntegerInRange(refRectA.y + 1, refRectA.y + refRectA.h - 1);
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB, &result, &refRectB);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectIntersection() with B fully outside A
 *
 * \sa SDL_GetRectIntersection
 */
static int SDLCALL rect_testIntersectRectOutside(void *arg)
{
    SDL_Rect refRectA = { 0, 0, 32, 32 };
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    SDL_Rect result;
    bool intersection;

    /* rectB fully outside of rectA */
    refRectB.x = refRectA.x + refRectA.w + SDLTest_RandomIntegerInRange(1, 10);
    refRectB.y = refRectA.y + refRectA.h + SDLTest_RandomIntegerInRange(1, 10);
    refRectB.w = refRectA.w;
    refRectB.h = refRectA.h;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB, (SDL_Rect *)NULL, (SDL_Rect *)NULL);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectIntersection() with B partially intersecting A
 *
 * \sa SDL_GetRectIntersection
 */
static int SDLCALL rect_testIntersectRectPartial(void *arg)
{
    SDL_Rect refRectA = { 0, 0, 32, 32 };
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    SDL_Rect result;
    SDL_Rect expectedResult;
    bool intersection;

    /* rectB partially contained in rectA */
    refRectB.x = SDLTest_RandomIntegerInRange(refRectA.x + 1, refRectA.x + refRectA.w - 1);
    refRectB.y = SDLTest_RandomIntegerInRange(refRectA.y + 1, refRectA.y + refRectA.h - 1);
    refRectB.w = refRectA.w;
    refRectB.h = refRectA.h;
    rectA = refRectA;
    rectB = refRectB;
    expectedResult.x = refRectB.x;
    expectedResult.y = refRectB.y;
    expectedResult.w = refRectA.w - refRectB.x;
    expectedResult.h = refRectA.h - refRectB.y;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    /* rectB right edge */
    refRectB.x = rectA.w - 1;
    refRectB.y = rectA.y;
    refRectB.w = SDLTest_RandomIntegerInRange(1, refRectA.w - 1);
    refRectB.h = SDLTest_RandomIntegerInRange(1, refRectA.h - 1);
    rectA = refRectA;
    rectB = refRectB;
    expectedResult.x = refRectB.x;
    expectedResult.y = refRectB.y;
    expectedResult.w = 1;
    expectedResult.h = refRectB.h;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    /* rectB left edge */
    refRectB.x = 1 - rectA.w;
    refRectB.y = rectA.y;
    refRectB.w = refRectA.w;
    refRectB.h = SDLTest_RandomIntegerInRange(1, refRectA.h - 1);
    rectA = refRectA;
    rectB = refRectB;
    expectedResult.x = 0;
    expectedResult.y = refRectB.y;
    expectedResult.w = 1;
    expectedResult.h = refRectB.h;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    /* rectB bottom edge */
    refRectB.x = rectA.x;
    refRectB.y = rectA.h - 1;
    refRectB.w = SDLTest_RandomIntegerInRange(1, refRectA.w - 1);
    refRectB.h = SDLTest_RandomIntegerInRange(1, refRectA.h - 1);
    rectA = refRectA;
    rectB = refRectB;
    expectedResult.x = refRectB.x;
    expectedResult.y = refRectB.y;
    expectedResult.w = refRectB.w;
    expectedResult.h = 1;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    /* rectB top edge */
    refRectB.x = rectA.x;
    refRectB.y = 1 - rectA.h;
    refRectB.w = SDLTest_RandomIntegerInRange(1, refRectA.w - 1);
    refRectB.h = rectA.h;
    rectA = refRectA;
    rectB = refRectB;
    expectedResult.x = refRectB.x;
    expectedResult.y = 0;
    expectedResult.w = refRectB.w;
    expectedResult.h = 1;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectIntersection() with 1x1 pixel sized rectangles
 *
 * \sa SDL_GetRectIntersection
 */
static int SDLCALL rect_testIntersectRectPoint(void *arg)
{
    SDL_Rect refRectA = { 0, 0, 1, 1 };
    SDL_Rect refRectB = { 0, 0, 1, 1 };
    SDL_Rect rectA;
    SDL_Rect rectB;
    SDL_Rect result;
    bool intersection;
    int offsetX, offsetY;

    /* intersecting pixels */
    refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
    refRectB.x = refRectA.x;
    refRectB.y = refRectA.y;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB, &result, &refRectA);

    /* non-intersecting pixels cases */
    for (offsetX = -1; offsetX <= 1; offsetX++) {
        for (offsetY = -1; offsetY <= 1; offsetY++) {
            if (offsetX != 0 || offsetY != 0) {
                refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
                refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
                refRectB.x = refRectA.x;
                refRectB.y = refRectA.y;
                refRectB.x += offsetX;
                refRectB.y += offsetY;
                rectA = refRectA;
                rectB = refRectB;
                intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
                validateIntersectRectResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB, (SDL_Rect *)NULL, (SDL_Rect *)NULL);
            }
        }
    }

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectIntersection() with empty rectangles
 *
 * \sa SDL_GetRectIntersection
 */
static int SDLCALL rect_testIntersectRectEmpty(void *arg)
{
    SDL_Rect refRectA;
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    SDL_Rect result;
    bool intersection;
    bool empty;

    /* Rect A empty */
    result.w = SDLTest_RandomIntegerInRange(1, 100);
    result.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.w = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectB = refRectA;
    refRectA.w = 0;
    refRectA.h = 0;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB, (SDL_Rect *)NULL, (SDL_Rect *)NULL);
    empty = SDL_RectEmpty(&result);
    SDLTest_AssertCheck(empty == true, "Validate result is empty Rect; got: %s", (empty == true) ? "true" : "false");

    /* Rect B empty */
    result.w = SDLTest_RandomIntegerInRange(1, 100);
    result.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.w = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectB = refRectA;
    refRectB.w = 0;
    refRectB.h = 0;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB, (SDL_Rect *)NULL, (SDL_Rect *)NULL);
    empty = SDL_RectEmpty(&result);
    SDLTest_AssertCheck(empty == true, "Validate result is empty Rect; got: %s", (empty == true) ? "true" : "false");

    /* Rect A and B empty */
    result.w = SDLTest_RandomIntegerInRange(1, 100);
    result.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.w = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectB = refRectA;
    refRectA.w = 0;
    refRectA.h = 0;
    refRectB.w = 0;
    refRectB.h = 0;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_GetRectIntersection(&rectA, &rectB, &result);
    validateIntersectRectResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB, (SDL_Rect *)NULL, (SDL_Rect *)NULL);
    empty = SDL_RectEmpty(&result);
    SDLTest_AssertCheck(empty == true, "Validate result is empty Rect; got: %s", (empty == true) ? "true" : "false");

    return TEST_COMPLETED;
}

/**
 * Negative tests against SDL_GetRectIntersection() with invalid parameters
 *
 * \sa SDL_GetRectIntersection
 */
static int SDLCALL rect_testIntersectRectParam(void *arg)
{
    SDL_Rect rectA;
    SDL_Rect rectB = { 0 };
    SDL_Rect result;
    bool intersection;

    /* invalid parameter combinations */
    intersection = SDL_GetRectIntersection((SDL_Rect *)NULL, &rectB, &result);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when 1st parameter is NULL");
    intersection = SDL_GetRectIntersection(&rectA, (SDL_Rect *)NULL, &result);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when 2nd parameter is NULL");
    intersection = SDL_GetRectIntersection(&rectA, &rectB, (SDL_Rect *)NULL);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when 3rd parameter is NULL");
    intersection = SDL_GetRectIntersection((SDL_Rect *)NULL, (SDL_Rect *)NULL, &result);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when 1st and 2nd parameters are NULL");
    intersection = SDL_GetRectIntersection((SDL_Rect *)NULL, &rectB, (SDL_Rect *)NULL);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when 1st and 3rd parameters are NULL ");
    intersection = SDL_GetRectIntersection((SDL_Rect *)NULL, (SDL_Rect *)NULL, (SDL_Rect *)NULL);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when all parameters are NULL");

    return TEST_COMPLETED;
}

/**
 * Tests SDL_HasRectIntersection() with B fully inside A
 *
 * \sa SDL_HasRectIntersection
 */
static int SDLCALL rect_testHasIntersectionInside(void *arg)
{
    SDL_Rect refRectA = { 0, 0, 32, 32 };
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    bool intersection;

    /* rectB fully contained in rectA */
    refRectB.x = 0;
    refRectB.y = 0;
    refRectB.w = SDLTest_RandomIntegerInRange(refRectA.x + 1, refRectA.x + refRectA.w - 1);
    refRectB.h = SDLTest_RandomIntegerInRange(refRectA.y + 1, refRectA.y + refRectA.h - 1);
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_HasRectIntersection() with B fully outside A
 *
 * \sa SDL_HasRectIntersection
 */
static int SDLCALL rect_testHasIntersectionOutside(void *arg)
{
    SDL_Rect refRectA = { 0, 0, 32, 32 };
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    bool intersection;

    /* rectB fully outside of rectA */
    refRectB.x = refRectA.x + refRectA.w + SDLTest_RandomIntegerInRange(1, 10);
    refRectB.y = refRectA.y + refRectA.h + SDLTest_RandomIntegerInRange(1, 10);
    refRectB.w = refRectA.w;
    refRectB.h = refRectA.h;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_HasRectIntersection() with B partially intersecting A
 *
 * \sa SDL_HasRectIntersection
 */
static int SDLCALL rect_testHasIntersectionPartial(void *arg)
{
    SDL_Rect refRectA = { 0, 0, 32, 32 };
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    bool intersection;

    /* rectB partially contained in rectA */
    refRectB.x = SDLTest_RandomIntegerInRange(refRectA.x + 1, refRectA.x + refRectA.w - 1);
    refRectB.y = SDLTest_RandomIntegerInRange(refRectA.y + 1, refRectA.y + refRectA.h - 1);
    refRectB.w = refRectA.w;
    refRectB.h = refRectA.h;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB);

    /* rectB right edge */
    refRectB.x = rectA.w - 1;
    refRectB.y = rectA.y;
    refRectB.w = SDLTest_RandomIntegerInRange(1, refRectA.w - 1);
    refRectB.h = SDLTest_RandomIntegerInRange(1, refRectA.h - 1);
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB);

    /* rectB left edge */
    refRectB.x = 1 - rectA.w;
    refRectB.y = rectA.y;
    refRectB.w = refRectA.w;
    refRectB.h = SDLTest_RandomIntegerInRange(1, refRectA.h - 1);
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB);

    /* rectB bottom edge */
    refRectB.x = rectA.x;
    refRectB.y = rectA.h - 1;
    refRectB.w = SDLTest_RandomIntegerInRange(1, refRectA.w - 1);
    refRectB.h = SDLTest_RandomIntegerInRange(1, refRectA.h - 1);
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB);

    /* rectB top edge */
    refRectB.x = rectA.x;
    refRectB.y = 1 - rectA.h;
    refRectB.w = SDLTest_RandomIntegerInRange(1, refRectA.w - 1);
    refRectB.h = rectA.h;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_HasRectIntersection() with 1x1 pixel sized rectangles
 *
 * \sa SDL_HasRectIntersection
 */
static int SDLCALL rect_testHasIntersectionPoint(void *arg)
{
    SDL_Rect refRectA = { 0, 0, 1, 1 };
    SDL_Rect refRectB = { 0, 0, 1, 1 };
    SDL_Rect rectA;
    SDL_Rect rectB;
    bool intersection;
    int offsetX, offsetY;

    /* intersecting pixels */
    refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
    refRectB.x = refRectA.x;
    refRectB.y = refRectA.y;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, true, &rectA, &rectB, &refRectA, &refRectB);

    /* non-intersecting pixels cases */
    for (offsetX = -1; offsetX <= 1; offsetX++) {
        for (offsetY = -1; offsetY <= 1; offsetY++) {
            if (offsetX != 0 || offsetY != 0) {
                refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
                refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
                refRectB.x = refRectA.x;
                refRectB.y = refRectA.y;
                refRectB.x += offsetX;
                refRectB.y += offsetY;
                rectA = refRectA;
                rectB = refRectB;
                intersection = SDL_HasRectIntersection(&rectA, &rectB);
                validateHasIntersectionResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB);
            }
        }
    }

    return TEST_COMPLETED;
}

/**
 * Tests SDL_HasRectIntersection() with empty rectangles
 *
 * \sa SDL_HasRectIntersection
 */
static int SDLCALL rect_testHasIntersectionEmpty(void *arg)
{
    SDL_Rect refRectA;
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    bool intersection;

    /* Rect A empty */
    refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.w = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectB = refRectA;
    refRectA.w = 0;
    refRectA.h = 0;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB);

    /* Rect B empty */
    refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.w = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectB = refRectA;
    refRectB.w = 0;
    refRectB.h = 0;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB);

    /* Rect A and B empty */
    refRectA.x = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.y = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.w = SDLTest_RandomIntegerInRange(1, 100);
    refRectA.h = SDLTest_RandomIntegerInRange(1, 100);
    refRectB = refRectA;
    refRectA.w = 0;
    refRectA.h = 0;
    refRectB.w = 0;
    refRectB.h = 0;
    rectA = refRectA;
    rectB = refRectB;
    intersection = SDL_HasRectIntersection(&rectA, &rectB);
    validateHasIntersectionResults(intersection, false, &rectA, &rectB, &refRectA, &refRectB);

    return TEST_COMPLETED;
}

/**
 * Negative tests against SDL_HasRectIntersection() with invalid parameters
 *
 * \sa SDL_HasRectIntersection
 */
static int SDLCALL rect_testHasIntersectionParam(void *arg)
{
    SDL_Rect rectA;
    SDL_Rect rectB = { 0 };
    bool intersection;

    /* invalid parameter combinations */
    intersection = SDL_HasRectIntersection((SDL_Rect *)NULL, &rectB);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when 1st parameter is NULL");
    intersection = SDL_HasRectIntersection(&rectA, (SDL_Rect *)NULL);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when 2nd parameter is NULL");
    intersection = SDL_HasRectIntersection((SDL_Rect *)NULL, (SDL_Rect *)NULL);
    SDLTest_AssertCheck(intersection == false, "Check that function returns false when all parameters are NULL");

    return TEST_COMPLETED;
}

/**
 * Test SDL_GetRectEnclosingPointsFloat()
 *
 * \sa SDL_GetRectEnclosingPointsFloat
 */
static int SDLCALL rect_testEnclosePointsFloat(void *arg)
{
    SDL_FPoint fpts[3] = { { 1.25f, 2.5f }, { 1.75f, 3.75f }, { 3.5f, 3.0f } };
    int i, count = 3;
    SDL_FRect clip = { 0.0f, 1.0f, 4.0f, 4.0f };
    SDL_FRect result;

    SDL_GetRectEnclosingPointsFloat(fpts, count, &clip, &result);
    SDLTest_AssertCheck(result.x == 1.25f && result.y == 2.5f && result.w == 2.25f && result.h == 1.25f,
                        "Resulting enclosing rectangle incorrect: expected (%.2f,%.2f - %.2fx%.2f), actual (%.2f,%.2f - %.2fx%.2f)",
                        1.25f, 2.5f, 2.25f, 1.25f, result.x, result.y, result.w, result.h);
    for (i = 0; i != count; i++) {
        bool inside;

        inside = SDL_PointInRectFloat(&fpts[i], &clip);
        SDLTest_AssertCheck(inside,
                            "Expected point (%.2f,%.2f) to be inside clip rect (%.2f,%.2f - %.2fx%.2f)",
                            fpts[i].x, fpts[i].y, clip.x, clip.y, clip.w, clip.h);

        inside = SDL_PointInRectFloat(&fpts[i], &result);
        SDLTest_AssertCheck(inside,
                            "Expected point (%.2f,%.2f) to be inside result rect (%.2f,%.2f - %.2fx%.2f)",
                            fpts[i].x, fpts[i].y, result.x, result.y, result.w, result.h);
    }

    return TEST_COMPLETED;
}

/**
 * Test SDL_GetRectEnclosingPoints() without clipping
 *
 * \sa SDL_GetRectEnclosingPoints
 */
static int SDLCALL rect_testEnclosePoints(void *arg)
{
    const int numPoints = 16;
    SDL_Point refPoints[16];
    SDL_Point points[16];
    SDL_Rect result;
    bool anyEnclosed;
    bool anyEnclosedNoResult;
    bool expectedEnclosed = true;
    int newx, newy;
    int minx = 0, maxx = 0, miny = 0, maxy = 0;
    int i;

    /* Create input data, tracking result */
    for (i = 0; i < numPoints; i++) {
        newx = SDLTest_RandomIntegerInRange(-1024, 1024);
        newy = SDLTest_RandomIntegerInRange(-1024, 1024);
        refPoints[i].x = newx;
        refPoints[i].y = newy;
        points[i].x = newx;
        points[i].y = newy;
        if (i == 0) {
            minx = newx;
            maxx = newx;
            miny = newy;
            maxy = newy;
        } else {
            if (newx < minx) {
                minx = newx;
            }
            if (newx > maxx) {
                maxx = newx;
            }
            if (newy < miny) {
                miny = newy;
            }
            if (newy > maxy) {
                maxy = newy;
            }
        }
    }

    /* Call function and validate - special case: no result requested */
    anyEnclosedNoResult = SDL_GetRectEnclosingPoints(points, numPoints, NULL, (SDL_Rect *)NULL);
    SDLTest_AssertCheck(expectedEnclosed == anyEnclosedNoResult,
                        "Check expected return value %s, got %s",
                        (expectedEnclosed == true) ? "true" : "false",
                        (anyEnclosedNoResult == true) ? "true" : "false");
    for (i = 0; i < numPoints; i++) {
        SDLTest_AssertCheck(refPoints[i].x == points[i].x && refPoints[i].y == points[i].y,
                            "Check that source point %i was not modified: expected (%i,%i) actual (%i,%i)",
                            i, refPoints[i].x, refPoints[i].y, points[i].x, points[i].y);
    }

    /* Call function and validate */
    anyEnclosed = SDL_GetRectEnclosingPoints(points, numPoints, NULL, &result);
    SDLTest_AssertCheck(expectedEnclosed == anyEnclosed,
                        "Check return value %s, got %s",
                        (expectedEnclosed == true) ? "true" : "false",
                        (anyEnclosed == true) ? "true" : "false");
    for (i = 0; i < numPoints; i++) {
        SDLTest_AssertCheck(refPoints[i].x == points[i].x && refPoints[i].y == points[i].y,
                            "Check that source point %i was not modified: expected (%i,%i) actual (%i,%i)",
                            i, refPoints[i].x, refPoints[i].y, points[i].x, points[i].y);
    }
    SDLTest_AssertCheck(result.x == minx && result.y == miny && result.w == (maxx - minx + 1) && result.h == (maxy - miny + 1),
                        "Resulting enclosing rectangle incorrect: expected (%i,%i - %i,%i), actual (%i,%i - %i,%i)",
                        minx, miny, maxx, maxy, result.x, result.y, result.x + result.w - 1, result.y + result.h - 1);

    return TEST_COMPLETED;
}

/**
 * Test SDL_GetRectEnclosingPoints() with repeated input points
 *
 * \sa SDL_GetRectEnclosingPoints
 */
static int SDLCALL rect_testEnclosePointsRepeatedInput(void *arg)
{
    const int numPoints = 8;
    const int halfPoints = 4;
    SDL_Point refPoints[8];
    SDL_Point points[8];
    SDL_Rect result;
    bool anyEnclosed;
    bool anyEnclosedNoResult;
    bool expectedEnclosed = true;
    int newx, newy;
    int minx = 0, maxx = 0, miny = 0, maxy = 0;
    int i;

    /* Create input data, tracking result */
    for (i = 0; i < numPoints; i++) {
        if (i < halfPoints) {
            newx = SDLTest_RandomIntegerInRange(-1024, 1024);
            newy = SDLTest_RandomIntegerInRange(-1024, 1024);
        } else {
            newx = refPoints[i - halfPoints].x;
            newy = refPoints[i - halfPoints].y;
        }
        refPoints[i].x = newx;
        refPoints[i].y = newy;
        points[i].x = newx;
        points[i].y = newy;
        if (i == 0) {
            minx = newx;
            maxx = newx;
            miny = newy;
            maxy = newy;
        } else {
            if (newx < minx) {
                minx = newx;
            }
            if (newx > maxx) {
                maxx = newx;
            }
            if (newy < miny) {
                miny = newy;
            }
            if (newy > maxy) {
                maxy = newy;
            }
        }
    }

    /* Call function and validate - special case: no result requested */
    anyEnclosedNoResult = SDL_GetRectEnclosingPoints(points, numPoints, NULL, (SDL_Rect *)NULL);
    SDLTest_AssertCheck(expectedEnclosed == anyEnclosedNoResult,
                        "Check return value %s, got %s",
                        (expectedEnclosed == true) ? "true" : "false",
                        (anyEnclosedNoResult == true) ? "true" : "false");
    for (i = 0; i < numPoints; i++) {
        SDLTest_AssertCheck(refPoints[i].x == points[i].x && refPoints[i].y == points[i].y,
                            "Check that source point %i was not modified: expected (%i,%i) actual (%i,%i)",
                            i, refPoints[i].x, refPoints[i].y, points[i].x, points[i].y);
    }

    /* Call function and validate */
    anyEnclosed = SDL_GetRectEnclosingPoints(points, numPoints, NULL, &result);
    SDLTest_AssertCheck(expectedEnclosed == anyEnclosed,
                        "Check return value %s, got %s",
                        (expectedEnclosed == true) ? "true" : "false",
                        (anyEnclosed == true) ? "true" : "false");
    for (i = 0; i < numPoints; i++) {
        SDLTest_AssertCheck(refPoints[i].x == points[i].x && refPoints[i].y == points[i].y,
                            "Check that source point %i was not modified: expected (%i,%i) actual (%i,%i)",
                            i, refPoints[i].x, refPoints[i].y, points[i].x, points[i].y);
    }
    SDLTest_AssertCheck(result.x == minx && result.y == miny && result.w == (maxx - minx + 1) && result.h == (maxy - miny + 1),
                        "Check resulting enclosing rectangle: expected (%i,%i - %i,%i), actual (%i,%i - %i,%i)",
                        minx, miny, maxx, maxy, result.x, result.y, result.x + result.w - 1, result.y + result.h - 1);

    return TEST_COMPLETED;
}

/**
 * Test SDL_GetRectEnclosingPoints() with clipping
 *
 * \sa SDL_GetRectEnclosingPoints
 */
static int SDLCALL rect_testEnclosePointsWithClipping(void *arg)
{
    const int numPoints = 16;
    SDL_Point refPoints[16];
    SDL_Point points[16];
    SDL_Rect refClip;
    SDL_Rect clip;
    SDL_Rect result;
    bool anyEnclosed;
    bool anyEnclosedNoResult;
    bool expectedEnclosed = false;
    int newx, newy;
    int minx = 0, maxx = 0, miny = 0, maxy = 0;
    int i;

    /* Setup clipping rectangle */
    refClip.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refClip.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refClip.w = SDLTest_RandomIntegerInRange(1, 1024);
    refClip.h = SDLTest_RandomIntegerInRange(1, 1024);

    /* Create input data, tracking result */
    for (i = 0; i < numPoints; i++) {
        newx = SDLTest_RandomIntegerInRange(-1024, 1024);
        newy = SDLTest_RandomIntegerInRange(-1024, 1024);
        refPoints[i].x = newx;
        refPoints[i].y = newy;
        points[i].x = newx;
        points[i].y = newy;
        if ((newx >= refClip.x) && (newx < (refClip.x + refClip.w)) &&
            (newy >= refClip.y) && (newy < (refClip.y + refClip.h))) {
            if (expectedEnclosed == false) {
                minx = newx;
                maxx = newx;
                miny = newy;
                maxy = newy;
            } else {
                if (newx < minx) {
                    minx = newx;
                }
                if (newx > maxx) {
                    maxx = newx;
                }
                if (newy < miny) {
                    miny = newy;
                }
                if (newy > maxy) {
                    maxy = newy;
                }
            }
            expectedEnclosed = true;
        }
    }

    /* Call function and validate - special case: no result requested */
    clip = refClip;
    anyEnclosedNoResult = SDL_GetRectEnclosingPoints(points, numPoints, &clip, NULL);
    SDLTest_AssertCheck(expectedEnclosed == anyEnclosedNoResult,
                        "Expected return value %s, got %s",
                        (expectedEnclosed == true) ? "true" : "false",
                        (anyEnclosedNoResult == true) ? "true" : "false");
    for (i = 0; i < numPoints; i++) {
        SDLTest_AssertCheck(refPoints[i].x == points[i].x && refPoints[i].y == points[i].y,
                            "Check that source point %i was not modified: expected (%i,%i) actual (%i,%i)",
                            i, refPoints[i].x, refPoints[i].y, points[i].x, points[i].y);
    }
    SDLTest_AssertCheck(refClip.x == clip.x && refClip.y == clip.y && refClip.w == clip.w && refClip.h == clip.h,
                        "Check that source clipping rectangle was not modified");

    /* Call function and validate */
    anyEnclosed = SDL_GetRectEnclosingPoints(points, numPoints, &clip, &result);
    SDLTest_AssertCheck(expectedEnclosed == anyEnclosed,
                        "Check return value %s, got %s",
                        (expectedEnclosed == true) ? "true" : "false",
                        (anyEnclosed == true) ? "true" : "false");
    for (i = 0; i < numPoints; i++) {
        SDLTest_AssertCheck(refPoints[i].x == points[i].x && refPoints[i].y == points[i].y,
                            "Check that source point %i was not modified: expected (%i,%i) actual (%i,%i)",
                            i, refPoints[i].x, refPoints[i].y, points[i].x, points[i].y);
    }
    SDLTest_AssertCheck(refClip.x == clip.x && refClip.y == clip.y && refClip.w == clip.w && refClip.h == clip.h,
                        "Check that source clipping rectangle was not modified");
    if (expectedEnclosed == true) {
        SDLTest_AssertCheck(result.x == minx && result.y == miny && result.w == (maxx - minx + 1) && result.h == (maxy - miny + 1),
                            "Check resulting enclosing rectangle: expected (%i,%i - %i,%i), actual (%i,%i - %i,%i)",
                            minx, miny, maxx, maxy, result.x, result.y, result.x + result.w - 1, result.y + result.h - 1);
    }

    /* Empty clipping rectangle */
    clip.w = 0;
    clip.h = 0;
    expectedEnclosed = false;
    anyEnclosed = SDL_GetRectEnclosingPoints(points, numPoints, &clip, &result);
    SDLTest_AssertCheck(expectedEnclosed == anyEnclosed,
                        "Check return value %s, got %s",
                        (expectedEnclosed == true) ? "true" : "false",
                        (anyEnclosed == true) ? "true" : "false");

    return TEST_COMPLETED;
}

/**
 * Negative tests against SDL_GetRectEnclosingPoints() with invalid parameters
 *
 * \sa SDL_GetRectEnclosingPoints
 */
static int SDLCALL rect_testEnclosePointsParam(void *arg)
{
    SDL_Point points[1];
    int count;
    SDL_Rect clip = { 0 };
    SDL_Rect result;
    bool anyEnclosed;

    /* invalid parameter combinations */
    anyEnclosed = SDL_GetRectEnclosingPoints((SDL_Point *)NULL, 1, &clip, &result);
    SDLTest_AssertCheck(anyEnclosed == false, "Check that functions returns false when 1st parameter is NULL");
    anyEnclosed = SDL_GetRectEnclosingPoints(points, 0, &clip, &result);
    SDLTest_AssertCheck(anyEnclosed == false, "Check that functions returns false when 2nd parameter is 0");
    count = SDLTest_RandomIntegerInRange(-100, -1);
    anyEnclosed = SDL_GetRectEnclosingPoints(points, count, &clip, &result);
    SDLTest_AssertCheck(anyEnclosed == false, "Check that functions returns false when 2nd parameter is %i (negative)", count);
    anyEnclosed = SDL_GetRectEnclosingPoints((SDL_Point *)NULL, 0, &clip, &result);
    SDLTest_AssertCheck(anyEnclosed == false, "Check that functions returns false when 1st parameter is NULL and 2nd parameter was 0");

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectUnion() where rect B is outside rect A
 *
 * \sa SDL_GetRectUnion
 */
static int SDLCALL rect_testUnionRectOutside(void *arg)
{
    SDL_Rect refRectA, refRectB;
    SDL_Rect rectA, rectB;
    SDL_Rect expectedResult;
    SDL_Rect result;
    int minx, maxx, miny, maxy;
    int dx, dy;

    /* Union 1x1 outside */
    for (dx = -1; dx < 2; dx++) {
        for (dy = -1; dy < 2; dy++) {
            if ((dx != 0) || (dy != 0)) {
                refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
                refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
                refRectA.w = 1;
                refRectA.h = 1;
                refRectB.x = SDLTest_RandomIntegerInRange(-1024, 1024) + dx * 2048;
                refRectB.y = SDLTest_RandomIntegerInRange(-1024, 1024) + dx * 2048;
                refRectB.w = 1;
                refRectB.h = 1;
                minx = (refRectA.x < refRectB.x) ? refRectA.x : refRectB.x;
                maxx = (refRectA.x > refRectB.x) ? refRectA.x : refRectB.x;
                miny = (refRectA.y < refRectB.y) ? refRectA.y : refRectB.y;
                maxy = (refRectA.y > refRectB.y) ? refRectA.y : refRectB.y;
                expectedResult.x = minx;
                expectedResult.y = miny;
                expectedResult.w = maxx - minx + 1;
                expectedResult.h = maxy - miny + 1;
                rectA = refRectA;
                rectB = refRectB;
                SDL_GetRectUnion(&rectA, &rectB, &result);
                validateUnionRectResults(&rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);
            }
        }
    }

    /* Union outside overlap */
    for (dx = -1; dx < 2; dx++) {
        for (dy = -1; dy < 2; dy++) {
            if ((dx != 0) || (dy != 0)) {
                refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
                refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
                refRectA.w = SDLTest_RandomIntegerInRange(256, 512);
                refRectA.h = SDLTest_RandomIntegerInRange(256, 512);
                refRectB.x = refRectA.x + 1 + dx * 2;
                refRectB.y = refRectA.y + 1 + dy * 2;
                refRectB.w = refRectA.w - 2;
                refRectB.h = refRectA.h - 2;
                expectedResult = refRectA;
                if (dx == -1) {
                    expectedResult.x--;
                }
                if (dy == -1) {
                    expectedResult.y--;
                }
                if ((dx == 1) || (dx == -1)) {
                    expectedResult.w++;
                }
                if ((dy == 1) || (dy == -1)) {
                    expectedResult.h++;
                }
                rectA = refRectA;
                rectB = refRectB;
                SDL_GetRectUnion(&rectA, &rectB, &result);
                validateUnionRectResults(&rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);
            }
        }
    }

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectUnion() where rect A or rect B are empty
 *
 * \sa SDL_GetRectUnion
 */
static int SDLCALL rect_testUnionRectEmpty(void *arg)
{
    SDL_Rect refRectA, refRectB;
    SDL_Rect rectA, rectB;
    SDL_Rect expectedResult;
    SDL_Rect result;

    /* A empty */
    refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.w = 0;
    refRectA.h = 0;
    refRectB.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectB.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectB.w = SDLTest_RandomIntegerInRange(1, 1024);
    refRectB.h = SDLTest_RandomIntegerInRange(1, 1024);
    expectedResult = refRectB;
    rectA = refRectA;
    rectB = refRectB;
    SDL_GetRectUnion(&rectA, &rectB, &result);
    validateUnionRectResults(&rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    /* B empty */
    refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.w = SDLTest_RandomIntegerInRange(1, 1024);
    refRectA.h = SDLTest_RandomIntegerInRange(1, 1024);
    refRectB.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectB.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectB.w = 0;
    refRectB.h = 0;
    expectedResult = refRectA;
    rectA = refRectA;
    rectB = refRectB;
    SDL_GetRectUnion(&rectA, &rectB, &result);
    validateUnionRectResults(&rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    /* A and B empty */
    refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.w = 0;
    refRectA.h = 0;
    refRectB.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectB.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectB.w = 0;
    refRectB.h = 0;
    result.x = 0;
    result.y = 0;
    result.w = 0;
    result.h = 0;
    expectedResult = result;
    rectA = refRectA;
    rectB = refRectB;
    SDL_GetRectUnion(&rectA, &rectB, &result);
    validateUnionRectResults(&rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    return TEST_COMPLETED;
}

/**
 * Tests SDL_GetRectUnion() where rect B is inside rect A
 *
 * \sa SDL_GetRectUnion
 */
static int SDLCALL rect_testUnionRectInside(void *arg)
{
    SDL_Rect refRectA, refRectB;
    SDL_Rect rectA, rectB;
    SDL_Rect expectedResult;
    SDL_Rect result;
    int dx, dy;

    /* Union 1x1 with itself */
    refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.w = 1;
    refRectA.h = 1;
    expectedResult = refRectA;
    rectA = refRectA;
    SDL_GetRectUnion(&rectA, &rectA, &result);
    validateUnionRectResults(&rectA, &rectA, &refRectA, &refRectA, &result, &expectedResult);

    /* Union 1x1 somewhere inside */
    refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.w = SDLTest_RandomIntegerInRange(256, 1024);
    refRectA.h = SDLTest_RandomIntegerInRange(256, 1024);
    refRectB.x = refRectA.x + 1 + SDLTest_RandomIntegerInRange(1, refRectA.w - 2);
    refRectB.y = refRectA.y + 1 + SDLTest_RandomIntegerInRange(1, refRectA.h - 2);
    refRectB.w = 1;
    refRectB.h = 1;
    expectedResult = refRectA;
    rectA = refRectA;
    rectB = refRectB;
    SDL_GetRectUnion(&rectA, &rectB, &result);
    validateUnionRectResults(&rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);

    /* Union inside with edges modified */
    for (dx = -1; dx < 2; dx++) {
        for (dy = -1; dy < 2; dy++) {
            if ((dx != 0) || (dy != 0)) {
                refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
                refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
                refRectA.w = SDLTest_RandomIntegerInRange(256, 1024);
                refRectA.h = SDLTest_RandomIntegerInRange(256, 1024);
                refRectB = refRectA;
                if (dx == -1) {
                    refRectB.x++;
                }
                if ((dx == 1) || (dx == -1)) {
                    refRectB.w--;
                }
                if (dy == -1) {
                    refRectB.y++;
                }
                if ((dy == 1) || (dy == -1)) {
                    refRectB.h--;
                }
                expectedResult = refRectA;
                rectA = refRectA;
                rectB = refRectB;
                SDL_GetRectUnion(&rectA, &rectB, &result);
                validateUnionRectResults(&rectA, &rectB, &refRectA, &refRectB, &result, &expectedResult);
            }
        }
    }

    return TEST_COMPLETED;
}

/**
 * Negative tests against SDL_GetRectUnion() with invalid parameters
 *
 * \sa SDL_GetRectUnion
 */
static int SDLCALL rect_testUnionRectParam(void *arg)
{
    SDL_Rect rectA, rectB = { 0 };
    SDL_Rect result;

    /* invalid parameter combinations */
    SDL_GetRectUnion((SDL_Rect *)NULL, &rectB, &result);
    SDLTest_AssertPass("Check that function returns when 1st parameter is NULL");
    SDL_GetRectUnion(&rectA, (SDL_Rect *)NULL, &result);
    SDLTest_AssertPass("Check that function returns  when 2nd parameter is NULL");
    SDL_GetRectUnion(&rectA, &rectB, (SDL_Rect *)NULL);
    SDLTest_AssertPass("Check that function returns  when 3rd parameter is NULL");
    SDL_GetRectUnion((SDL_Rect *)NULL, &rectB, (SDL_Rect *)NULL);
    SDLTest_AssertPass("Check that function returns  when 1st and 3rd parameter are NULL");
    SDL_GetRectUnion(&rectA, (SDL_Rect *)NULL, (SDL_Rect *)NULL);
    SDLTest_AssertPass("Check that function returns  when 2nd and 3rd parameter are NULL");
    SDL_GetRectUnion((SDL_Rect *)NULL, (SDL_Rect *)NULL, (SDL_Rect *)NULL);
    SDLTest_AssertPass("Check that function returns  when all parameters are NULL");

    return TEST_COMPLETED;
}

/**
 * Tests SDL_RectEmptyFloat() with various inputs
 *
 * \sa SDL_RectEmptyFloat
 */
static int SDLCALL rect_testRectEmptyFloat(void *arg)
{
    SDL_FRect rect;
    bool result;

    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = 1.0f;
    rect.h = 1.0f;
    result = SDL_RectEmptyFloat(&rect);
    validateRectEmptyFloatResults(result, false, &rect);

    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = 0.0f;
    rect.h = 0.0f;
    result = SDL_RectEmptyFloat(&rect);
    validateRectEmptyFloatResults(result, false, &rect);

    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = -1.0f;
    rect.h = 1.0f;
    result = SDL_RectEmptyFloat(&rect);
    validateRectEmptyFloatResults(result, true, &rect);

    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = 1.0f;
    rect.h = -1.0f;
    result = SDL_RectEmptyFloat(&rect);
    validateRectEmptyFloatResults(result, true, &rect);


    return TEST_COMPLETED;
}

/**
 * Tests SDL_RectEmpty() with various inputs
 *
 * \sa SDL_RectEmpty
 */
static int SDLCALL rect_testRectEmpty(void *arg)
{
    SDL_Rect refRect;
    SDL_Rect rect;
    bool expectedResult;
    bool result;
    int w, h;

    /* Non-empty case */
    refRect.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRect.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRect.w = SDLTest_RandomIntegerInRange(256, 1024);
    refRect.h = SDLTest_RandomIntegerInRange(256, 1024);
    expectedResult = false;
    rect = refRect;
    result = SDL_RectEmpty(&rect);
    validateRectEmptyResults(result, expectedResult, &rect, &refRect);

    /* Empty case */
    for (w = -1; w < 2; w++) {
        for (h = -1; h < 2; h++) {
            if ((w != 1) || (h != 1)) {
                refRect.x = SDLTest_RandomIntegerInRange(-1024, 1024);
                refRect.y = SDLTest_RandomIntegerInRange(-1024, 1024);
                refRect.w = w;
                refRect.h = h;
                expectedResult = true;
                rect = refRect;
                result = SDL_RectEmpty(&rect);
                validateRectEmptyResults(result, expectedResult, &rect, &refRect);
            }
        }
    }

    return TEST_COMPLETED;
}

/**
 * Negative tests against SDL_RectEmpty() with invalid parameters
 *
 * \sa SDL_RectEmpty
 */
static int SDLCALL rect_testRectEmptyParam(void *arg)
{
    bool result;

    /* invalid parameter combinations */
    result = SDL_RectEmpty(NULL);
    SDLTest_AssertCheck(result == true, "Check that function returns TRUE when 1st parameter is NULL");

    return TEST_COMPLETED;
}

/**
 * Tests SDL_RectsEqual() with various inputs
 *
 * \sa SDL_RectsEqual
 */
static int SDLCALL rect_testRectEquals(void *arg)
{
    SDL_Rect refRectA;
    SDL_Rect refRectB;
    SDL_Rect rectA;
    SDL_Rect rectB;
    bool expectedResult;
    bool result;

    /* Equals */
    refRectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.w = SDLTest_RandomIntegerInRange(1, 1024);
    refRectA.h = SDLTest_RandomIntegerInRange(1, 1024);
    refRectB = refRectA;
    expectedResult = true;
    rectA = refRectA;
    rectB = refRectB;
    result = SDL_RectsEqual(&rectA, &rectB);
    validateRectEqualsResults(result, expectedResult, &rectA, &rectB, &refRectA, &refRectB);

    return TEST_COMPLETED;
}

/**
 * Negative tests against SDL_RectsEqual() with invalid parameters
 *
 * \sa SDL_RectsEqual
 */
static int SDLCALL rect_testRectEqualsParam(void *arg)
{
    SDL_Rect rectA;
    SDL_Rect rectB;
    bool result;

    /* data setup */
    rectA.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    rectA.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    rectA.w = SDLTest_RandomIntegerInRange(1, 1024);
    rectA.h = SDLTest_RandomIntegerInRange(1, 1024);
    rectB.x = SDLTest_RandomIntegerInRange(-1024, 1024);
    rectB.y = SDLTest_RandomIntegerInRange(-1024, 1024);
    rectB.w = SDLTest_RandomIntegerInRange(1, 1024);
    rectB.h = SDLTest_RandomIntegerInRange(1, 1024);

    /* invalid parameter combinations */
    result = SDL_RectsEqual(NULL, &rectB);
    SDLTest_AssertCheck(result == false, "Check that function returns false when 1st parameter is NULL");
    result = SDL_RectsEqual(&rectA, NULL);
    SDLTest_AssertCheck(result == false, "Check that function returns false when 2nd parameter is NULL");
    result = SDL_RectsEqual(NULL, NULL);
    SDLTest_AssertCheck(result == false, "Check that function returns false when 1st and 2nd parameter are NULL");

    return TEST_COMPLETED;
}

/**
 * Tests SDL_RectsEqualFloat() with various inputs
 *
 * \sa SDL_RectsEqualFloat
 */
static int SDLCALL rect_testFRectEquals(void *arg)
{
    SDL_FRect refRectA;
    SDL_FRect refRectB;
    SDL_FRect rectA;
    SDL_FRect rectB;
    bool expectedResult;
    bool result;

    /* Equals */
    refRectA.x = (float)SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.y = (float)SDLTest_RandomIntegerInRange(-1024, 1024);
    refRectA.w = (float)SDLTest_RandomIntegerInRange(1, 1024);
    refRectA.h = (float)SDLTest_RandomIntegerInRange(1, 1024);
    refRectB = refRectA;
    expectedResult = true;
    rectA = refRectA;
    rectB = refRectB;
    result = SDL_RectsEqualFloat(&rectA, &rectB);
    validateFRectEqualsResults(result, expectedResult, &rectA, &rectB, &refRectA, &refRectB);

    return TEST_COMPLETED;
}

/**
 * Negative tests against SDL_RectsEqualFloat() with invalid parameters
 *
 * \sa SDL_RectsEqualFloat
 */
static int SDLCALL rect_testFRectEqualsParam(void *arg)
{
    SDL_FRect rectA;
    SDL_FRect rectB;
    bool result;

    /* data setup -- For the purpose of this test, the values don't matter. */
    rectA.x = SDLTest_RandomFloat();
    rectA.y = SDLTest_RandomFloat();
    rectA.w = SDLTest_RandomFloat();
    rectA.h = SDLTest_RandomFloat();
    rectB.x = SDLTest_RandomFloat();
    rectB.y = SDLTest_RandomFloat();
    rectB.w = SDLTest_RandomFloat();
    rectB.h = SDLTest_RandomFloat();

    /* invalid parameter combinations */
    result = SDL_RectsEqualFloat(NULL, &rectB);
    SDLTest_AssertCheck(result == false, "Check that function returns false when 1st parameter is NULL");
    result = SDL_RectsEqualFloat(&rectA, NULL);
    SDLTest_AssertCheck(result == false, "Check that function returns false when 2nd parameter is NULL");
    result = SDL_RectsEqualFloat(NULL, NULL);
    SDLTest_AssertCheck(result == false, "Check that function returns false when 1st and 2nd parameter are NULL");

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Rect test cases */

/* SDL_GetRectAndLineIntersectionFloat */
static const SDLTest_TestCaseReference rectTestIntersectRectAndLineFloat = {
    rect_testIntersectRectAndLineFloat, "rect_testIntersectRectAndLineFloat", "Tests SDL_GetRectAndLineIntersectionFloat", TEST_ENABLED
};

/* SDL_GetRectAndLineIntersection */
static const SDLTest_TestCaseReference rectTestIntersectRectAndLine = {
    rect_testIntersectRectAndLine, "rect_testIntersectRectAndLine", "Tests SDL_GetRectAndLineIntersection clipping cases", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectAndLineInside = {
    rect_testIntersectRectAndLineInside, "rect_testIntersectRectAndLineInside", "Tests SDL_GetRectAndLineIntersection with line fully contained in rect", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectAndLineOutside = {
    rect_testIntersectRectAndLineOutside, "rect_testIntersectRectAndLineOutside", "Tests SDL_GetRectAndLineIntersection with line fully outside of rect", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectAndLineEmpty = {
    rect_testIntersectRectAndLineEmpty, "rect_testIntersectRectAndLineEmpty", "Tests SDL_GetRectAndLineIntersection with empty rectangle ", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectAndLineParam = {
    rect_testIntersectRectAndLineParam, "rect_testIntersectRectAndLineParam", "Negative tests against SDL_GetRectAndLineIntersection with invalid parameters", TEST_ENABLED
};

/* SDL_GetRectIntersectionFloat */
static const SDLTest_TestCaseReference rectTestIntersectRectFloat = {
    rect_testIntersectRectFloat, "rect_testIntersectRectFloat", "Tests SDL_GetRectIntersectionFloat", TEST_ENABLED
};

/* SDL_GetRectIntersection */
static const SDLTest_TestCaseReference rectTestIntersectRectInside = {
    rect_testIntersectRectInside, "rect_testIntersectRectInside", "Tests SDL_GetRectIntersection with B fully contained in A", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectOutside = {
    rect_testIntersectRectOutside, "rect_testIntersectRectOutside", "Tests SDL_GetRectIntersection with B fully outside of A", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectPartial = {
    rect_testIntersectRectPartial, "rect_testIntersectRectPartial", "Tests SDL_GetRectIntersection with B partially intersecting A", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectPoint = {
    rect_testIntersectRectPoint, "rect_testIntersectRectPoint", "Tests SDL_GetRectIntersection with 1x1 sized rectangles", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectEmpty = {
    rect_testIntersectRectEmpty, "rect_testIntersectRectEmpty", "Tests SDL_GetRectIntersection with empty rectangles", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestIntersectRectParam = {
    rect_testIntersectRectParam, "rect_testIntersectRectParam", "Negative tests against SDL_GetRectIntersection with invalid parameters", TEST_ENABLED
};

/* SDL_HasRectIntersection */
static const SDLTest_TestCaseReference rectTestHasIntersectionInside = {
    rect_testHasIntersectionInside, "rect_testHasIntersectionInside", "Tests SDL_HasRectIntersection with B fully contained in A", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestHasIntersectionOutside = {
    rect_testHasIntersectionOutside, "rect_testHasIntersectionOutside", "Tests SDL_HasRectIntersection with B fully outside of A", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestHasIntersectionPartial = {
    rect_testHasIntersectionPartial, "rect_testHasIntersectionPartial", "Tests SDL_HasRectIntersection with B partially intersecting A", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestHasIntersectionPoint = {
    rect_testHasIntersectionPoint, "rect_testHasIntersectionPoint", "Tests SDL_HasRectIntersection with 1x1 sized rectangles", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestHasIntersectionEmpty = {
    rect_testHasIntersectionEmpty, "rect_testHasIntersectionEmpty", "Tests SDL_HasRectIntersection with empty rectangles", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestHasIntersectionParam = {
    rect_testHasIntersectionParam, "rect_testHasIntersectionParam", "Negative tests against SDL_HasRectIntersection with invalid parameters", TEST_ENABLED
};

/* SDL_GetRectEnclosingPointsFloat */
static const SDLTest_TestCaseReference rectTestEnclosePointsFloat = {
    rect_testEnclosePointsFloat, "rect_testEnclosePointsFloat", "Tests SDL_GetRectEnclosingPointsFloat", TEST_ENABLED
};

/* SDL_GetRectEnclosingPoints */
static const SDLTest_TestCaseReference rectTestEnclosePoints = {
    rect_testEnclosePoints, "rect_testEnclosePoints", "Tests SDL_GetRectEnclosingPoints without clipping", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestEnclosePointsWithClipping = {
    rect_testEnclosePointsWithClipping, "rect_testEnclosePointsWithClipping", "Tests SDL_GetRectEnclosingPoints with clipping", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestEnclosePointsRepeatedInput = {
    rect_testEnclosePointsRepeatedInput, "rect_testEnclosePointsRepeatedInput", "Tests SDL_GetRectEnclosingPoints with repeated input", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestEnclosePointsParam = {
    rect_testEnclosePointsParam, "rect_testEnclosePointsParam", "Negative tests against SDL_GetRectEnclosingPoints with invalid parameters", TEST_ENABLED
};

/* SDL_GetRectUnion */
static const SDLTest_TestCaseReference rectTestUnionRectInside = {
    rect_testUnionRectInside, "rect_testUnionRectInside", "Tests SDL_GetRectUnion where rect B is inside rect A", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestUnionRectOutside = {
    rect_testUnionRectOutside, "rect_testUnionRectOutside", "Tests SDL_GetRectUnion where rect B is outside rect A", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestUnionRectEmpty = {
    rect_testUnionRectEmpty, "rect_testUnionRectEmpty", "Tests SDL_GetRectUnion where rect A or rect B are empty", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestUnionRectParam = {
    rect_testUnionRectParam, "rect_testUnionRectParam", "Negative tests against SDL_GetRectUnion with invalid parameters", TEST_ENABLED
};

/* SDL_RectEmptyFloat */
static const SDLTest_TestCaseReference rectTestRectEmptyFloat = {
    rect_testRectEmptyFloat, "rect_testRectEmptyFloat", "Tests SDL_RectEmptyFloat with various inputs", TEST_ENABLED
};

/* SDL_RectEmpty */
static const SDLTest_TestCaseReference rectTestRectEmpty = {
    rect_testRectEmpty, "rect_testRectEmpty", "Tests SDL_RectEmpty with various inputs", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestRectEmptyParam = {
    rect_testRectEmptyParam, "rect_testRectEmptyParam", "Negative tests against SDL_RectEmpty with invalid parameters", TEST_ENABLED
};

/* SDL_RectsEqual */
static const SDLTest_TestCaseReference rectTestRectEquals = {
    rect_testRectEquals, "rect_testRectEquals", "Tests SDL_RectsEqual with various inputs", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestRectEqualsParam = {
    rect_testRectEqualsParam, "rect_testRectEqualsParam", "Negative tests against SDL_RectsEqual with invalid parameters", TEST_ENABLED
};

/* SDL_RectsEqualFloat */
static const SDLTest_TestCaseReference rectTestFRectEquals = {
    rect_testFRectEquals, "rect_testFRectEquals", "Tests SDL_RectsEqualFloat with various inputs", TEST_ENABLED
};

static const SDLTest_TestCaseReference rectTestFRectEqualsParam = {
    rect_testFRectEqualsParam, "rect_testFRectEqualsParam", "Negative tests against SDL_RectsEqualFloat with invalid parameters", TEST_ENABLED
};

/**
 * Sequence of Rect test cases; functions that handle simple rectangles including overlaps and merges.
 */
static const SDLTest_TestCaseReference *rectTests[] = {
    &rectTestIntersectRectAndLineFloat,
    &rectTestIntersectRectAndLine,
    &rectTestIntersectRectAndLineInside,
    &rectTestIntersectRectAndLineOutside,
    &rectTestIntersectRectAndLineEmpty,
    &rectTestIntersectRectAndLineParam,
    &rectTestIntersectRectFloat,
    &rectTestIntersectRectInside,
    &rectTestIntersectRectOutside,
    &rectTestIntersectRectPartial,
    &rectTestIntersectRectPoint,
    &rectTestIntersectRectEmpty,
    &rectTestIntersectRectParam,
    &rectTestHasIntersectionInside,
    &rectTestHasIntersectionOutside,
    &rectTestHasIntersectionPartial,
    &rectTestHasIntersectionPoint,
    &rectTestHasIntersectionEmpty,
    &rectTestHasIntersectionParam,
    &rectTestEnclosePointsFloat,
    &rectTestEnclosePoints,
    &rectTestEnclosePointsWithClipping,
    &rectTestEnclosePointsRepeatedInput,
    &rectTestEnclosePointsParam,
    &rectTestUnionRectInside,
    &rectTestUnionRectOutside,
    &rectTestUnionRectEmpty,
    &rectTestUnionRectParam,
    &rectTestRectEmptyFloat,
    &rectTestRectEmpty,
    &rectTestRectEmptyParam,
    &rectTestRectEquals,
    &rectTestRectEqualsParam,
    &rectTestFRectEquals,
    &rectTestFRectEqualsParam,
    NULL
};

/* Rect test suite (global) */
SDLTest_TestSuiteReference rectTestSuite = {
    "Rect",
    NULL,
    rectTests,
    NULL
};
