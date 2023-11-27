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

#include <stddef.h>

/**
 * Pen test suite
 */

#define SDL_internal_h_ /* Inhibit dynamic symbol redefinitions that clash with ours */

/* ================= System Under Test (SUT) ================== */
/* Renaming SUT operations to avoid link-time symbol clashes */
#define SDL_GetPens            SDL_SUT_GetPens
#define SDL_GetPenStatus       SDL_SUT_GetPenStatus
#define SDL_GetPenFromGUID     SDL_SUT_GetPenFromGUID
#define SDL_GetPenGUID         SDL_SUT_GetPenGUID
#define SDL_PenConnected       SDL_SUT_PenConnected
#define SDL_GetPenName         SDL_SUT_GetPenName
#define SDL_GetPenCapabilities SDL_SUT_GetPenCapabilities
#define SDL_GetPenType         SDL_SUT_GetPenType

#define SDL_GetPenPtr                SDL_SUT_GetPenPtr
#define SDL_PenModifyBegin           SDL_SUT_PenModifyBegin
#define SDL_PenModifyAddCapabilities SDL_SUT_PenModifyAddCapabilities
#define SDL_PenModifyForWacomID      SDL_SUT_PenModifyForWacomID
#define SDL_PenUpdateGUIDForWacom    SDL_SUT_PenUpdateGUIDForWacom
#define SDL_PenUpdateGUIDForType     SDL_SUT_PenUpdateGUIDForType
#define SDL_PenUpdateGUIDForGeneric  SDL_SUT_PenUpdateGUIDForGeneric
#define SDL_PenModifyEnd             SDL_SUT_PenModifyEnd
#define SDL_PenGCMark                SDL_SUT_PenGCMark
#define SDL_PenGCSweep               SDL_SUT_PenGCSweep
#define SDL_SendPenMotion            SDL_SUT_SendPenMotion
#define SDL_SendPenButton            SDL_SUT_SendPenButton
#define SDL_SendPenTipEvent          SDL_SUT_SendPenTipEvent
#define SDL_SendPenWindowEvent       SDL_SUT_SendPenWindowEvent
#define SDL_PenPerformHitTest        SDL_SUT_PenPerformHitTest
#define SDL_PenInit                  SDL_SUT_PenInit
#define SDL_PenQuit                  SDL_SUT_PenQuit

/* ================= Mock API ================== */

#include <stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
/* For SDL_Window, SDL_Mouse, SDL_MouseID: */
#include "../src/events/SDL_mouse_c.h"
/* Divert calls to mock mouse API: */
#define SDL_SendMouseMotion       SDL_Mock_SendMouseMotion
#define SDL_SendMouseButton       SDL_Mock_SendMouseButton
#define SDL_GetMouse              SDL_Mock_GetMouse
#define SDL_MousePositionInWindow SDL_Mock_MousePositionInWindow
#define SDL_SetMouseFocus         SDL_Mock_SetMouseFocus

/* Mock mouse API */
static int SDL_SendMouseMotion(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, int relative, float x, float y);
static int SDL_SendMouseButton(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, Uint8 state, Uint8 button);
static SDL_Mouse *SDL_GetMouse(void);
static SDL_bool SDL_MousePositionInWindow(SDL_Window *window, SDL_MouseID mouseID, float x, float y);
static void SDL_SetMouseFocus(SDL_Window *window);

/* Import SUT code with macro-renamed function names  */
#define SDL_waylanddyn_h_ /* hack: suppress spurious build problem with libdecor.h on Wayland */
#include "../src/events/SDL_pen.c"
#include "../src/events/SDL_pen_c.h"


/* ================= Internal SDL API Compatibility ================== */
/* Mock implementations of Pen -> Mouse calls */
/* Not thread-safe! */

static SDL_bool SDL_MousePositionInWindow(SDL_Window *window, SDL_MouseID mouseID, float x, float y)
{
    return SDL_TRUE;
}

static int _mouseemu_last_event = 0;
static float _mouseemu_last_x = 0.0f;
static float _mouseemu_last_y = 0.0f;
static int _mouseemu_last_mouseid = 0;
static int _mouseemu_last_button = 0;
static int _mouseemu_last_relative = 0;
static int _mouseemu_last_focus = -1;

static int SDL_SendMouseButton(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, Uint8 state, Uint8 button)
{
    if (mouseID == SDL_PEN_MOUSEID) {
        _mouseemu_last_event = (state == SDL_PRESSED) ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        _mouseemu_last_button = button;
        _mouseemu_last_mouseid = mouseID;
    }
    return 1;
}

static int SDL_SendMouseMotion(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, int relative, float x, float y)
{
    if (mouseID == SDL_PEN_MOUSEID) {
        _mouseemu_last_event = SDL_EVENT_MOUSE_MOTION;
        _mouseemu_last_x = x;
        _mouseemu_last_y = y;
        _mouseemu_last_mouseid = mouseID;
        _mouseemu_last_relative = relative;
    }
    return 1;
}

static SDL_Mouse *SDL_GetMouse(void)
{
    static SDL_Mouse dummy_mouse;

    dummy_mouse.focus = NULL;
    dummy_mouse.mouseID = 0;

    return &dummy_mouse;
}

static void SDL_SetMouseFocus(SDL_Window *window)
{
    _mouseemu_last_focus = window ? 1 : 0;
}

/* ================= Test Case Support ================== */

#define PEN_NUM_TEST_IDS 8

/* Helper functions */

/* Iterate over all pens to find index for pen ID, otherwise -1 */
static int _pen_iterationFindsPenIDAt(SDL_PenID needle)
{
    int i;
    int num_pens = -1;

    SDL_PenID *pens = SDL_GetPens(&num_pens);
    /* Check for (a) consistency and (b) ability to handle NULL parameter */
    SDL_PenID *pens2 = SDL_GetPens(NULL);

    SDLTest_AssertCheck(num_pens >= 0,
                        "SDL_GetPens() yielded %d pens", num_pens);
    SDLTest_AssertCheck(pens[num_pens] == 0,
                        "SDL_GetPens() not 0 terminated (num_pens = %d)", num_pens);
    SDLTest_AssertCheck(pens2[num_pens] == 0,
                        "SDL_GetPens(NULL) not 0 terminated (num_pens = %d)", num_pens);

    for (i = 0; i < num_pens; ++i) {
        SDLTest_AssertCheck(pens[i] == pens2[i],
                            "SDL_GetPens(&i) and SDL_GetPens(NULL) disagree at index %d/%d", i, num_pens);
        SDLTest_AssertCheck(pens[i] != SDL_PEN_INVALID,
                            "Invalid pen ID %08lx at index %d/%d after SDL_GetPens()", (unsigned long) pens[i], i, num_pens);
    }
    SDL_free(pens2);

    for (i = 0; pens[i]; ++i) {
        SDL_PenID pen_id = pens[i];

        SDLTest_AssertCheck(pen_id != SDL_PEN_INVALID,
                            "Invalid pen ID %08lx at index %d/%d after SDL_GetPens()", (unsigned long) pen_id, i, num_pens);
        if (pen_id == needle) {
            SDL_free(pens);
            return i;
        }
    }
    SDL_free(pens);
    return -1;
}

/* Retrieve number of pens and sanity-check SDL_GetPens() */
static int
_num_pens(void)
{
    int num_pens = -1;
    SDL_PenID *pens = SDL_GetPens(&num_pens);
    SDLTest_AssertCheck(pens != NULL,
                        "SDL_GetPens() => NULL");
    SDLTest_AssertCheck(num_pens >= 0,
                        "SDL_GetPens() reports %d pens", num_pens);
    SDLTest_AssertCheck(pens[num_pens] == 0,
                        "SDL_GetPens()[%d] != 0", num_pens);
    SDL_free(pens);
    return num_pens;
}

/* Assert number of pens is as expected */
static void _AssertCheck_num_pens(int expected, char *location)
{
    int num_pens = _num_pens();
    SDLTest_AssertCheck(expected == num_pens,
                        "Expected SDL_GetPens() =>count = %d, actual = %d: %s", expected, num_pens, location);
}

/* ---------------------------------------- */
/* Test device deallocation */

typedef struct /* Collection of pen (de)allocation information  */
{
    unsigned int deallocated_id_flags;         /* ith bits set to 1 if the ith test_id is deallocated */
    unsigned int deallocated_deviceinfo_flags; /* ith bits set to 1 if deviceinfo as *int with value i was deallocated */
    SDL_PenID ids[PEN_NUM_TEST_IDS];
    SDL_GUID guids[PEN_NUM_TEST_IDS];
    SDL_Window *window;
    int num_ids;
    int initial_pen_count;
} pen_testdata;

/* SDL_PenGCSweep(): callback for tracking pen deallocation */
static void _pen_testdata_callback(Uint32 deviceid, void *deviceinfo, void *tracker_ref)
{
    pen_testdata *tracker = (pen_testdata *)tracker_ref;
    int offset = -1;
    int i;

    for (i = 0; i < tracker->num_ids; ++i) {
        if (deviceid == tracker->ids[i]) {
            tracker->deallocated_id_flags |= (1 << i);
        }
    }

    SDLTest_AssertCheck(deviceinfo != NULL,
                        "Device %lu has deviceinfo",
                        (unsigned long) deviceid);
    offset = *((int *)deviceinfo);
    SDLTest_AssertCheck(offset >= 0 && offset <= 31,
                        "Device %lu has well-formed deviceinfo %d",
                        (unsigned long) deviceid, offset);
    tracker->deallocated_deviceinfo_flags |= 1 << offset;
    SDL_free(deviceinfo);
}

/* GC Sweep tracking: update "tracker->deallocated_id_flags" and "tracker->deallocated_deviceinfo_flags" to record deallocations */
static void _pen_trackGCSweep(pen_testdata *tracker)
{
    tracker->deallocated_id_flags = 0;
    tracker->deallocated_deviceinfo_flags = 0;
    SDL_PenGCSweep(tracker, _pen_testdata_callback);
}

/* Finds a number of unused pen IDs (does not allocate them).  Also initialises GUIDs. */
static void _pen_unusedIDs(pen_testdata *tracker, int count)
{
    static int guidmod = 0; /* Ensure uniqueness as long as we use no more than 256 test pens */
    Uint32 synthetic_penid = 1000u;
    int index = 0;

    tracker->num_ids = count;
    SDLTest_AssertCheck(count < PEN_NUM_TEST_IDS, "Test setup: Valid number of test IDs requested: %d", (int)count);

    while (count--) {
        int k;

        while (SDL_GetPenPtr(synthetic_penid)) {
            ++synthetic_penid;
        }
        tracker->ids[index] = synthetic_penid;
        for (k = 0; k < 15; ++k) {
            tracker->guids[index].data[k] = (16 * k) + index;
        }
        tracker->guids[index].data[15] = ++guidmod;

        ++synthetic_penid;
        ++index;
    }
}

#define DEVICEINFO_UNCHANGED -17

/* Allocate deviceinfo for pen */
static void _pen_setDeviceinfo(SDL_Pen *pen, int deviceinfo)
{
    if (deviceinfo == DEVICEINFO_UNCHANGED) {
        SDLTest_AssertCheck(pen->deviceinfo != NULL,
                            "pen->deviceinfo was already set for %p (%lu), as expected",
                            pen, (unsigned long) pen->header.id);
    } else {
        int *data = (int *)SDL_malloc(sizeof(int));
        *data = deviceinfo;

        SDLTest_AssertCheck(pen->deviceinfo == NULL,
                            "pen->deviceinfo was NULL for %p (%lu) when requesting deviceinfo %d",
                            pen, (unsigned long) pen->header.id, deviceinfo);

        pen->deviceinfo = data;
    }
    SDL_PenModifyEnd(pen, SDL_TRUE);
}

/* ---------------------------------------- */
/* Back up and restore device information */

typedef struct deviceinfo_backup
{
    Uint32 deviceid;
    void *deviceinfo;
    struct deviceinfo_backup *next;
} deviceinfo_backup;

/* SDL_PenGCSweep(): Helper callback for collecting all deviceinfo records */
static void _pen_accumulate_gc_sweep(Uint32 deviceid, void *deviceinfo, void *backup_ref)
{
    deviceinfo_backup **db_ref = (deviceinfo_backup **)backup_ref;
    deviceinfo_backup *next = *db_ref;

    *db_ref = SDL_calloc(sizeof(deviceinfo_backup), 1);
    (*db_ref)->deviceid = deviceid;
    (*db_ref)->deviceinfo = deviceinfo;
    (*db_ref)->next = next;
}

/* SDL_PenGCSweep(): Helper callback that must never be called  */
static void _pen_assert_impossible(Uint32 deviceid, void *deviceinfo, void *backup_ref)
{
    SDLTest_AssertCheck(0, "Deallocation for deviceid %lu during enableAndRestore: not expected",
                        (unsigned long) deviceid);
}

/* Disable all pens and store their status */
static deviceinfo_backup *_pen_disableAndBackup(void)
{
    deviceinfo_backup *backup = NULL;

    SDL_PenGCMark();
    SDL_PenGCSweep(&backup, _pen_accumulate_gc_sweep);
    return backup;
}

/* Restore all pens to their previous status */
static void _pen_enableAndRestore(deviceinfo_backup *backup, int test_marksweep)
{
    if (test_marksweep) {
        SDL_PenGCMark();
    }
    while (backup) {
        SDL_Pen *disabledpen = SDL_GetPenPtr(backup->deviceid);
        deviceinfo_backup *next = backup->next;

        SDL_PenModifyEnd(SDL_PenModifyBegin(disabledpen->header.id),
                         SDL_TRUE);
        disabledpen->deviceinfo = backup->deviceinfo;

        SDL_free(backup);
        backup = next;
    }
    if (test_marksweep) {
        SDL_PenGCSweep(NULL, _pen_assert_impossible);
    }
}

static struct SDL_Window _test_window = { 0 };

/* ---------------------------------------- */
/* Default set-up and tear down routines    */

/* Back up existing pens, allocate fresh ones but don't assign them yet */
static deviceinfo_backup *_setup_test(pen_testdata *ptest, int pens_for_testing)
{
    int i;
    deviceinfo_backup *backup;

    /* Get number of pens */
    SDL_free(SDL_GetPens(&ptest->initial_pen_count));

    /* Provide fake window for window enter/exit simulation */
    _test_window.id = 0x7e57da7a;
    _test_window.w = 1600;
    _test_window.h = 1200;
    ptest->window = &_test_window;

    /* Grab unused pen IDs for testing */
    _pen_unusedIDs(ptest, pens_for_testing);
    for (i = 0; i < pens_for_testing; ++i) {
        int index = _pen_iterationFindsPenIDAt(ptest->ids[i]);
        SDLTest_AssertCheck(-1 == index,
                            "Registered PenID(%lu) since index %d == -1",
                            (unsigned long) ptest->ids[i], index);
    }

    /* Remove existing pens, but back up */
    backup = _pen_disableAndBackup();

    _AssertCheck_num_pens(0, "after disabling and backing up all current pens");
    SDLTest_AssertPass("Removed existing pens");

    return backup;
}

static void _teardown_test_general(pen_testdata *ptest, deviceinfo_backup *backup, int with_gc_test)
{
    /* Restore previously existing pens */
    _pen_enableAndRestore(backup, with_gc_test);

    /* validate */
    SDLTest_AssertPass("Restored pens to pre-test state");
    _AssertCheck_num_pens(ptest->initial_pen_count, "after restoring all initial pens");
}

static void _teardown_test(pen_testdata *ptest, deviceinfo_backup *backup)
{
    _teardown_test_general(ptest, backup, 0);
}

static void _teardown_test_with_gc(pen_testdata *ptest, deviceinfo_backup *backup)
{
    _teardown_test_general(ptest, backup, 1);
}

/* ---------------------------------------- */
/* Pen simulation                           */

#define SIMPEN_ACTION_DONE           0
#define SIMPEN_ACTION_MOVE_X         1
#define SIMPEN_ACTION_MOVE_Y         2
#define SIMPEN_ACTION_AXIS           3
#define SIMPEN_ACTION_MOTION_EVENT   4 /* epxlicit motion event */
#define SIMPEN_ACTION_MOTION_EVENT_S 5 /* send motion event but expect it to be suppressed */
#define SIMPEN_ACTION_PRESS          6 /* implicit update event */
#define SIMPEN_ACTION_RELEASE        7 /* implicit update event */
#define SIMPEN_ACTION_DOWN           8 /* implicit update event */
#define SIMPEN_ACTION_UP             9 /* implicit update event */
#define SIMPEN_ACTION_ERASER_MODE    10

/* Individual action in pen simulation script */
typedef struct simulated_pen_action
{
    int type;
    int pen_index; /* index into the list of simulated pens */
    int index;     /* button or axis number, if needed */
    float update;  /* x,y; for AXIS, update[0] is the updated axis */
} simulated_pen_action;

static simulated_pen_action _simpen_event(int type, int pen_index, int index, float v, int line_nr)
{
    simulated_pen_action action;
    action.type = type;
    action.pen_index = pen_index;
    action.index = index;
    action.update = v;

    /* Sanity check-- turned out to be necessary */
    if ((type == SIMPEN_ACTION_PRESS || type == SIMPEN_ACTION_RELEASE) && index == 0) {
	SDL_Log("Error: SIMPEN_EVENT_BUTTON must have button > 0  (first button has number 1!), in line %d!", line_nr);
        exit(1);
    }
    return action;
}

/* STEP is passed in later (C macros use dynamic scoping) */

#define SIMPEN_DONE() \
    STEP _simpen_event(SIMPEN_ACTION_DONE, 0, 0, 0.0f, __LINE__)
#define SIMPEN_MOVE(pen_index, x, y)                                         \
    STEP _simpen_event(SIMPEN_ACTION_MOVE_X, (pen_index), 0, (x), __LINE__); \
    STEP _simpen_event(SIMPEN_ACTION_MOVE_Y, (pen_index), 0, (y), __LINE__)

#define SIMPEN_AXIS(pen_index, axis, y) \
    STEP _simpen_event(SIMPEN_ACTION_AXIS, (pen_index), (axis), (y), __LINE__)

#define SIMPEN_EVENT_MOTION(pen_index) \
    STEP _simpen_event(SIMPEN_ACTION_MOTION_EVENT, (pen_index), 0, 0.0f, __LINE__)

#define SIMPEN_EVENT_MOTION_SUPPRESSED(pen_index) \
    STEP _simpen_event(SIMPEN_ACTION_MOTION_EVENT_S, (pen_index), 0, 0.0f, __LINE__)

#define SIMPEN_EVENT_BUTTON(pen_index, push, button) \
    STEP _simpen_event((push) ? SIMPEN_ACTION_PRESS : SIMPEN_ACTION_RELEASE, (pen_index), (button), 0.0f, __LINE__)

#define SIMPEN_EVENT_TIP(pen_index, touch, tip)				\
    STEP _simpen_event((touch) ? SIMPEN_ACTION_DOWN : SIMPEN_ACTION_UP, (pen_index), tip, 0.0f, __LINE__)

#define SIMPEN_SET_ERASER(pen_index, eraser_mode) \
    STEP _simpen_event(SIMPEN_ACTION_ERASER_MODE, (pen_index), eraser_mode, 0.0f, __LINE__)

static void
_pen_dump(const char *prefix, SDL_Pen *pen)
{
    int i;
    char *axes_str;

    if (!pen) {
        SDL_Log("(NULL pen)");
        return;
    }

    axes_str = SDL_strdup("");
    for (i = 0; i < SDL_PEN_NUM_AXES; ++i) {
        char *old_axes_str = axes_str;
        SDL_asprintf(&axes_str, "%s\t%f", old_axes_str, pen->last.axes[i]);
        SDL_free(old_axes_str);
    }
    SDL_Log("%s: pen %lu (%s): status=%04lx, flags=%lx, x,y=(%f, %f) axes = %s",
            prefix,
            (unsigned long) pen->header.id,
            pen->name,
            (unsigned long) pen->last.buttons,
            (unsigned long) pen->header.flags,
            pen->last.x, pen->last.y,
            axes_str);
    SDL_free(axes_str);
}

/* Runs until the next event has been issued or we are done and returns pointer to it.
   Returns NULL once we hit SIMPEN_ACTION_DONE.
   Updates simulated_pens accordingly.  There must be as many simulated_pens as the highest pen_index used in
   any of the "steps".
   Also validates the internal state with expectations (via SDL_GetPenStatus()) and updates the, but does not poll SDL events. */
static simulated_pen_action *
_pen_simulate(simulated_pen_action *steps, int *step_counter, SDL_Pen *simulated_pens, int num_pens)
{
    SDL_bool done = SDL_FALSE;
    SDL_bool dump_pens = SDL_FALSE;
    unsigned int mask;
    int pen_nr;

    do {
        simulated_pen_action step = steps[*step_counter];
        SDL_Pen *simpen = &simulated_pens[step.pen_index];

        if (step.pen_index >= num_pens) {
            SDLTest_AssertCheck(0,
                                "Unexpected pen index %d at step %d, action %d", step.pen_index, *step_counter, step.type);
            return NULL;
        }

        switch (step.type) {
        case SIMPEN_ACTION_DONE:
            SDLTest_AssertPass("SIMPEN_ACTION_DONE");
            return NULL;

        case SIMPEN_ACTION_MOVE_X:
            SDLTest_AssertPass("SIMPEN_ACTION_MOVE_X [pen %d] : y <- %f", step.pen_index, step.update);
            simpen->last.x = step.update;
            break;

        case SIMPEN_ACTION_MOVE_Y:
            SDLTest_AssertPass("SIMPEN_ACTION_MOVE_Y [pen %d] : x <- %f", step.pen_index, step.update);
            simpen->last.y = step.update;
            break;

        case SIMPEN_ACTION_AXIS:
            SDLTest_AssertPass("SIMPEN_ACTION_AXIS [pen %d] : axis[%d] <- %f", step.pen_index, step.index, step.update);
            simpen->last.axes[step.index] = step.update;
            break;

        case SIMPEN_ACTION_MOTION_EVENT:
            done = SDL_TRUE;
            SDLTest_AssertCheck(SDL_SendPenMotion(0, simpen->header.id, SDL_TRUE,
                                                  &simpen->last),
                                "SIMPEN_ACTION_MOTION_EVENT [pen %d]", step.pen_index);
            break;

        case SIMPEN_ACTION_MOTION_EVENT_S:
            SDLTest_AssertCheck(!SDL_SendPenMotion(0, simpen->header.id, SDL_TRUE,
                                                   &simpen->last),
                                "SIMPEN_ACTION_MOTION_EVENT_SUPPRESSED [pen %d]", step.pen_index);
            break;

        case SIMPEN_ACTION_PRESS:
            mask = (1 << (step.index - 1));
            simpen->last.buttons |= mask;
            SDLTest_AssertCheck(SDL_SendPenButton(0, simpen->header.id, SDL_PRESSED, step.index),
                                "SIMPEN_ACTION_PRESS [pen %d]: button %d (mask %x)", step.pen_index, step.index, mask);
            done = SDL_TRUE;
            break;

        case SIMPEN_ACTION_RELEASE:
            mask = ~(1 << (step.index - 1));
            simpen->last.buttons &= mask;
            SDLTest_AssertCheck(SDL_SendPenButton(0, simpen->header.id, SDL_RELEASED, step.index),
                                "SIMPEN_ACTION_RELEASE [pen %d]: button %d (mask %x)", step.pen_index, step.index, mask);
            done = SDL_TRUE;
            break;

        case SIMPEN_ACTION_DOWN:
            simpen->last.buttons |= SDL_PEN_DOWN_MASK;
            SDLTest_AssertCheck(SDL_SendPenTipEvent(0, simpen->header.id, SDL_PRESSED),
                                "SIMPEN_ACTION_DOWN [pen %d]: (mask %lx)", step.pen_index, SDL_PEN_DOWN_MASK);
            done = SDL_TRUE;
            break;

        case SIMPEN_ACTION_UP:
            simpen->last.buttons &= ~SDL_PEN_DOWN_MASK;
            SDLTest_AssertCheck(SDL_SendPenTipEvent(0, simpen->header.id, SDL_RELEASED),
                                "SIMPEN_ACTION_UP [pen %d]: (mask %lx)", step.pen_index, ~SDL_PEN_DOWN_MASK);
            done = SDL_TRUE;
            break;

        case SIMPEN_ACTION_ERASER_MODE: {
	    Uint32 pmask;
	    SDL_Pen *pen = SDL_PenModifyBegin(simpen->header.id);

	    if (step.index) {
		pmask = SDL_PEN_ERASER_MASK;
	    } else {
		pmask = SDL_PEN_INK_MASK;
	    }

	    SDL_PenModifyAddCapabilities(pen, pmask);
	    SDL_PenModifyEnd(pen, SDL_TRUE);

	    simpen->header.flags &= ~(SDL_PEN_INK_MASK | SDL_PEN_ERASER_MASK);
	    simpen->header.flags |= pmask;
	    break;
	}

        default:
            SDLTest_AssertCheck(0,
                                "Unexpected pen simulation action %d", step.type);
            return NULL;
        }
        ++(*step_counter);
    } while (!done);

    for (pen_nr = 0; pen_nr < num_pens; ++pen_nr) {
        SDL_Pen *simpen = &simulated_pens[pen_nr];
        float x = -1.0f, y = -1.0f;
        float axes[SDL_PEN_NUM_AXES];
        Uint32 actual_flags = SDL_GetPenStatus(simpen->header.id, &x, &y, axes, SDL_PEN_NUM_AXES);
        int i;

        if (simpen->last.x != x || simpen->last.y != y) {
            SDLTest_AssertCheck(0, "Coordinate mismatch in pen %d", pen_nr);
            dump_pens = SDL_TRUE;
        }
        if ((actual_flags & ~(SDL_PEN_INK_MASK | SDL_PEN_ERASER_MASK)) != (simpen->last.buttons & ~(SDL_PEN_INK_MASK | SDL_PEN_ERASER_MASK))) {
            SDLTest_AssertCheck(0, "Status mismatch in pen %d (reported: %08x)", pen_nr, (unsigned int)actual_flags);
            dump_pens = SDL_TRUE;
        }
        if ((actual_flags & (SDL_PEN_INK_MASK | SDL_PEN_ERASER_MASK)) != (simpen->header.flags & (SDL_PEN_INK_MASK | SDL_PEN_ERASER_MASK))) {
            SDLTest_AssertCheck(0, "Flags mismatch in pen %d (reported: %08x)", pen_nr, (unsigned int)actual_flags);
            dump_pens = SDL_TRUE;
        }
        for (i = 0; i < SDL_PEN_NUM_AXES; ++i) {
            if (axes[i] != simpen->last.axes[i]) {
                SDLTest_AssertCheck(0, "Axis %d mismatch in pen %d", pen_nr, i);
                dump_pens = SDL_TRUE;
            }
        }
    }

    if (dump_pens) {
        int i;
        for (i = 0; i < num_pens; ++i) {
            SDL_Log("==== pen #%d", i);
            _pen_dump("expect", simulated_pens + i);
            _pen_dump("actual", SDL_GetPenPtr(simulated_pens[i].header.id));
        }
    }

    return &steps[(*step_counter) - 1];
}

/* Init simulated_pens with suitable initial state */
static void
_pen_simulate_init(pen_testdata *ptest, SDL_Pen *simulated_pens, int num_pens)
{
    int i;
    for (i = 0; i < num_pens; ++i) {
        simulated_pens[i] = *SDL_GetPenPtr(ptest->ids[i]);
    }
}

/* ---------------------------------------- */
/* Other helper functions                   */

/* "standard" pen registration process */
static SDL_Pen *
_pen_register(SDL_PenID penid, SDL_GUID guid, char *name, Uint32 flags)
{
    SDL_Pen *pen = SDL_PenModifyBegin(penid);
    pen->guid = guid;
    SDL_strlcpy(pen->name, name, SDL_PEN_MAX_NAME);
    SDL_PenModifyAddCapabilities(pen, flags);
    return pen;
}

/* Test whether EXPECTED and ACTUAL of type TY agree.  Their C format string must be FMT.
   MESSAGE is a string with one format string, passed as ARG0. */
#define SDLTest_AssertEq1(TY, FMT, EXPECTED, ACTUAL, MESSAGE, ARG0)                                                                                               \
    {                                                                                                                                                             \
        TY _t_expect = (EXPECTED);                                                                                                                                \
        TY _t_actual = (ACTUAL);                                                                                                                                  \
        SDLTest_AssertCheck(_t_expect == _t_actual, "L%d: " MESSAGE ": expected " #EXPECTED " = " FMT ", actual = " FMT, __LINE__, (ARG0), _t_expect, _t_actual); \
    }

/* ================= Test Case Implementation ================== */

/**
 * @brief Check basic pen device introduction and iteration, as well as basic queries
 *
 * @sa SDL_GetPens, SDL_GetPenName, SDL_GetPenCapabilities
 */
static int
pen_iteration(void *arg)
{
    pen_testdata ptest;
    int i;
    char long_pen_name[SDL_PEN_MAX_NAME + 10];
    const char *name;
    deviceinfo_backup *backup;

    /* Check initial pens */
    SDL_PumpEvents();
    SDLTest_AssertPass("SDL_GetPens() => count = %d", _num_pens());

    /* Grab unused pen IDs for testing */
    backup = _setup_test(&ptest, 3); /* validates that we have zero pens */

    /* Re-run GC, track deallocations */
    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);
    _AssertCheck_num_pens(0, "after second GC pass");
    SDLTest_AssertCheck(ptest.deallocated_id_flags == 0, "No unexpected device deallocations");
    SDLTest_AssertCheck(ptest.deallocated_deviceinfo_flags == 0, "No unexpected deviceinfo deallocations");
    SDLTest_AssertPass("Validated that GC on empty pen set is idempotent");

    /* Add three pens, validate */
    SDL_PenGCMark();

    SDL_memset(long_pen_name, 'x', sizeof(long_pen_name)); /* Include pen name that is too long */
    long_pen_name[sizeof(long_pen_name) - 1] = 0;

    _pen_setDeviceinfo(_pen_register(ptest.ids[0], ptest.guids[0], "pen 0",
                                     SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                       16);
    _pen_setDeviceinfo(_pen_register(ptest.ids[2], ptest.guids[2], long_pen_name,
                                     SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK),
                       20);
    _pen_setDeviceinfo(_pen_register(ptest.ids[1], ptest.guids[1], "pen 1",
                                     SDL_PEN_ERASER_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_YTILT_MASK),
                       24);
    _pen_trackGCSweep(&ptest);

    _AssertCheck_num_pens(3, "after allocating three pens");

    SDLTest_AssertCheck(ptest.deallocated_id_flags == 0, "No unexpected device deallocations");
    SDLTest_AssertCheck(ptest.deallocated_deviceinfo_flags == 0, "No unexpected deviceinfo deallocations");

    for (i = 0; i < 3; ++i) {
        /* Check that all pens are accounted for */
        int index = _pen_iterationFindsPenIDAt(ptest.ids[i]);
        SDLTest_AssertCheck(-1 != index, "Found PenID(%lu)", (unsigned long) ptest.ids[i]);
    }
    SDLTest_AssertPass("Validated that all three pens are indexable");

    /* Check pen properties */
    SDLTest_AssertCheck(0 == SDL_strcmp("pen 0", SDL_GetPenName(ptest.ids[0])),
                        "Pen #0 name");
    SDLTest_AssertCheck((SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK) == SDL_GetPenCapabilities(ptest.ids[0], NULL),
                        "Pen #0 capabilities");

    SDLTest_AssertCheck(0 == SDL_strcmp("pen 1", SDL_GetPenName(ptest.ids[1])),
                        "Pen #1 name");
    SDLTest_AssertCheck((SDL_PEN_ERASER_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_YTILT_MASK) == SDL_GetPenCapabilities(ptest.ids[1], NULL),
                        "Pen #1 capabilities");

    name = SDL_GetPenName(ptest.ids[2]);
    SDLTest_AssertCheck(SDL_PEN_MAX_NAME - 1 == SDL_strlen(name),
                        "Pen #2 name length");
    SDLTest_AssertCheck(0 == SDL_memcmp(name, long_pen_name, SDL_PEN_MAX_NAME - 1),
                        "Pen #2 name contents");
    SDLTest_AssertCheck((SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK) == SDL_GetPenCapabilities(ptest.ids[2], NULL),
                        "Pen #2 capabilities");
    SDLTest_AssertPass("Pen registration and basic queries");

    /* Re-run GC, track deallocations */
    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);
    _AssertCheck_num_pens(0, "after third GC pass");
    SDLTest_AssertCheck(ptest.deallocated_id_flags == 0x07,
                        "No unexpected device deallocation : %08x", ptest.deallocated_id_flags);
    SDLTest_AssertCheck(ptest.deallocated_deviceinfo_flags == 0x01110000,
                        "No unexpected deviceinfo deallocation : %08x ", ptest.deallocated_deviceinfo_flags);
    SDLTest_AssertPass("Validated that GC on empty pen set is idempotent");

    /* tear down and finish */
    _teardown_test(&ptest, backup);
    return TEST_COMPLETED;
}

static void
_expect_pen_attached(SDL_PenID penid)
{
    SDLTest_AssertCheck(-1 != _pen_iterationFindsPenIDAt(penid),
                        "Found PenID(%lu)", (unsigned long) penid);
    SDLTest_AssertCheck(SDL_PenConnected(penid),
                        "Pen %lu was attached, as expected", (unsigned long) penid);
}

static void
_expect_pen_detached(SDL_PenID penid)
{
    SDLTest_AssertCheck(-1 == _pen_iterationFindsPenIDAt(penid),
                        "Did not find PenID(%lu), as expected", (unsigned long) penid);
    SDLTest_AssertCheck(!SDL_PenConnected(penid),
                        "Pen %lu was detached, as expected", (unsigned long) penid);
}

#define ATTACHED(i) (1 << (i))

static void
_expect_pens_attached_or_detached(SDL_PenID *pen_ids, int ids, Uint32 mask)
{
    int i;
    int attached_count = 0;
    for (i = 0; i < ids; ++i) {
        if (mask & (1 << i)) {
            ++attached_count;
            _expect_pen_attached(pen_ids[i]);
        } else {
            _expect_pen_detached(pen_ids[i]);
        }
    }
    _AssertCheck_num_pens(attached_count, "While checking attached/detached status");
}

/**
 * @brief Check pen device hotplugging
 *
 * @sa SDL_GetPens, SDL_GetPenName, SDL_GetPenCapabilities, SDL_PenConnected
 */
static int
pen_hotplugging(void *arg)
{
    pen_testdata ptest;
    deviceinfo_backup *backup = _setup_test(&ptest, 3);
    SDL_GUID checkguid;

    /* Add two pens */
    SDL_PenGCMark();

    _pen_setDeviceinfo(_pen_register(ptest.ids[0], ptest.guids[0], "pen 0", SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                       16);
    _pen_setDeviceinfo(_pen_register(ptest.ids[2], ptest.guids[2], "pen 2", SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                       24);
    _pen_trackGCSweep(&ptest);

    _AssertCheck_num_pens(2, "after allocating two pens (pass 1)");
    SDLTest_AssertCheck(ptest.deallocated_id_flags == 0, "No unexpected device deallocation (pass 1)");
    SDLTest_AssertCheck(ptest.deallocated_deviceinfo_flags == 0, "No unexpected deviceinfo deallocation (pass 1)");

    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(0) | ATTACHED(2));
    SDLTest_AssertPass("Validated hotplugging (pass 1): attachmend of two pens");

    /* Introduce pen #1, remove pen #2 */
    SDL_PenGCMark();
    _pen_setDeviceinfo(_pen_register(ptest.ids[0], ptest.guids[0], "pen 0", SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                       DEVICEINFO_UNCHANGED);
    _pen_setDeviceinfo(_pen_register(ptest.ids[1], ptest.guids[1], "pen 1", SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                       20);
    _pen_trackGCSweep(&ptest);

    _AssertCheck_num_pens(2, "after allocating two pens (pass 2)");
    SDLTest_AssertCheck(ptest.deallocated_id_flags == 0x04, "No unexpected device deallocation (pass 2): %x", ptest.deallocated_id_flags);
    SDLTest_AssertCheck(ptest.deallocated_deviceinfo_flags == 0x01000000, "No unexpected deviceinfo deallocation (pass 2): %x", ptest.deallocated_deviceinfo_flags);

    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(0) | ATTACHED(1));
    SDLTest_AssertPass("Validated hotplugging (pass 2): unplug one, attach another");

    /* Return to previous state (#0 and #2 attached) */
    SDL_PenGCMark();

    _pen_setDeviceinfo(_pen_register(ptest.ids[0], ptest.guids[0], "pen 0", SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_YTILT),
                       DEVICEINFO_UNCHANGED);
    _pen_setDeviceinfo(_pen_register(ptest.ids[2], ptest.guids[2], "pen 2", SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                       24);
    _pen_trackGCSweep(&ptest);

    _AssertCheck_num_pens(2, "after allocating two pens (pass 3)");
    SDLTest_AssertCheck(ptest.deallocated_id_flags == 0x02, "No unexpected device deallocation (pass 3)");
    SDLTest_AssertCheck(ptest.deallocated_deviceinfo_flags == 0x00100000, "No unexpected deviceinfo deallocation (pass 3)");

    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(0) | ATTACHED(2));
    SDLTest_AssertPass("Validated hotplugging (pass 3): return to state of pass 1");

    /* Introduce pen #1, remove pen #0 */
    SDL_PenGCMark();
    _pen_setDeviceinfo(_pen_register(ptest.ids[1], ptest.guids[1], "pen 1", SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                       20);
    _pen_setDeviceinfo(_pen_register(ptest.ids[2], ptest.guids[2], "pen 2", SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                       DEVICEINFO_UNCHANGED);
    _pen_trackGCSweep(&ptest);

    _AssertCheck_num_pens(2, "after allocating two pens (pass 4)");
    SDLTest_AssertCheck(ptest.deallocated_id_flags == 0x01, "No unexpected device deallocation (pass 4): %x", ptest.deallocated_id_flags);
    SDLTest_AssertCheck(ptest.deallocated_deviceinfo_flags == 0x00010000, "No unexpected deviceinfo deallocation (pass 4): %x", ptest.deallocated_deviceinfo_flags);

    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(1) | ATTACHED(2));
    SDLTest_AssertPass("Validated hotplugging (pass 5)");

    /* Check detached pen */
    SDLTest_AssertCheck(0 == SDL_strcmp("pen 0", SDL_GetPenName(ptest.ids[0])),
                        "Pen #0 name");
    checkguid = SDL_GetPenGUID(ptest.ids[0]);
    SDLTest_AssertCheck(0 == SDL_memcmp(ptest.guids[0].data, checkguid.data, sizeof(ptest.guids[0].data)),
                        "Pen #0 guid");
    SDLTest_AssertCheck((SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_YTILT) == SDL_GetPenCapabilities(ptest.ids[0], NULL),
                        "Pen #0 capabilities");
    SDLTest_AssertPass("Validated that detached pens retained name, GUID, axis info after pass 5");

    /* Individually detach #1 dn #2 */
    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(1) | ATTACHED(2));
    SDL_PenModifyEnd(SDL_PenModifyBegin(ptest.ids[1]), SDL_FALSE);
    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(2));

    SDL_PenModifyEnd(SDL_PenModifyBegin(ptest.ids[2]), SDL_FALSE);
    _expect_pens_attached_or_detached(ptest.ids, 3, 0);

    SDLTest_AssertPass("Validated individual hotplugging (pass 6)");

    /* Individually attach all */
    SDL_PenModifyEnd(SDL_PenModifyBegin(ptest.ids[2]), SDL_TRUE);
    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(2));

    SDL_PenModifyEnd(SDL_PenModifyBegin(ptest.ids[0]), SDL_TRUE);
    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(0) | ATTACHED(2));

    SDL_PenModifyEnd(SDL_PenModifyBegin(ptest.ids[1]), SDL_TRUE);
    _expect_pens_attached_or_detached(ptest.ids, 3, ATTACHED(0) | ATTACHED(1) | ATTACHED(2));
    SDLTest_AssertPass("Validated individual hotplugging (pass 7)");

    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);
    _AssertCheck_num_pens(0, "after hotplugging test (cleanup)");
    SDLTest_AssertCheck(ptest.deallocated_id_flags == 0x06, "No unexpected device deallocation (cleanup): %x", ptest.deallocated_id_flags);
    SDLTest_AssertCheck(ptest.deallocated_deviceinfo_flags == 0x01100000, "No unexpected deviceinfo deallocation (pass 4): %x", ptest.deallocated_deviceinfo_flags);

    _teardown_test_with_gc(&ptest, backup);

    return TEST_COMPLETED;
}

/**
 * @brief Check pen device GUID handling
 *
 * @sa SDL_GetPenGUID
 */
static int
pen_GUIDs(void *arg)
{
    int i;
    char *names[4] = { "pen 0", "pen 1", "pen 2", "pen 3" };
    pen_testdata ptest;
    deviceinfo_backup *backup;

    backup = _setup_test(&ptest, 4);

    /* Define four pens */
    SDL_PenGCMark();
    for (i = 0; i < 4; ++i) {
        _pen_setDeviceinfo(_pen_register(ptest.ids[i], ptest.guids[i], names[i], SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                           20);
    }
    _pen_trackGCSweep(&ptest);

    /* Detach pens 0 and 2 */
    SDL_PenGCMark();
    for (i = 1; i < 4; i += 2) {
        _pen_setDeviceinfo(_pen_register(ptest.ids[i], ptest.guids[i], names[i], SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK),
                           DEVICEINFO_UNCHANGED);
    }
    _pen_trackGCSweep(&ptest);

    for (i = 0; i < 4; ++i) {
        SDLTest_AssertCheck(ptest.ids[i] == SDL_GetPenFromGUID(ptest.guids[i]),
                            "GUID search succeeded for %d", i);
    }

    /* detach all */
    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);

    _teardown_test(&ptest, backup);
    SDLTest_AssertPass("Pen ID lookup by GUID");

    return TEST_COMPLETED;
}

/**
 * @brief Check pen device button reporting
 *
 */
static int
pen_buttonReporting(void *arg)
{
    int i;
    int button_nr, pen_nr;
    pen_testdata ptest;
    SDL_Event event;
    SDL_PenStatusInfo update;
    float axes[SDL_PEN_NUM_AXES + 1];
    const float expected_x[2] = { 10.0f, 20.0f };
    const float expected_y[2] = { 11.0f, 21.0f };
    const Uint32 all_axes = SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_ROTATION_MASK | SDL_PEN_AXIS_SLIDER_MASK;

    /* Register pen */
    deviceinfo_backup *backup = _setup_test(&ptest, 2);
    SDL_PenGCMark();
    _pen_setDeviceinfo(_pen_register(ptest.ids[0], ptest.guids[0], "test pen",
                                     SDL_PEN_INK_MASK | all_axes),
                       20);
    _pen_setDeviceinfo(_pen_register(ptest.ids[1], ptest.guids[1], "test eraser",
                                     SDL_PEN_ERASER_MASK | all_axes),
                       24);
    _pen_trackGCSweep(&ptest);

    /* Position mouse suitably before we start */
    for (i = 0; i <= SDL_PEN_NUM_AXES; ++i) {
        axes[i] = 0.0625f * i; /* initialise with numbers that can be represented precisely in IEEE 754 and
                                  are > 0.0f and <= 1.0f */
    }

    /* Let pens enter the test window */
    SDL_SendPenWindowEvent(0, ptest.ids[0], ptest.window);
    SDL_SendPenWindowEvent(0, ptest.ids[1], ptest.window);

    update.x = expected_x[0];
    update.y = expected_y[0];
    SDL_memcpy(update.axes, axes, sizeof(float) * SDL_PEN_NUM_AXES);
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);
    update.x = expected_x[1];
    update.y = expected_y[1];
    SDL_memcpy(update.axes, axes + 1, sizeof(float) * SDL_PEN_NUM_AXES);
    SDL_SendPenMotion(0, ptest.ids[1], SDL_TRUE, &update);

    while (SDL_PollEvent(&event))
        ; /* Flush event queue */

    /* Trigger pen tip events for PEN_DOWN */
    SDLTest_AssertPass("Touch pens to surface");

    for (pen_nr = 0; pen_nr < 2; ++pen_nr) {
        float *expected_axes = axes + pen_nr;
	SDL_bool found_event = SDL_FALSE;
        Uint16 pen_state = 0x0000 | SDL_PEN_DOWN_MASK;
	Uint8 tip = SDL_PEN_TIP_INK;

        if (pen_nr == 1) {
            pen_state |= SDL_PEN_ERASER_MASK;
	    tip = SDL_PEN_TIP_ERASER;
        }

	SDL_SendPenTipEvent(0, ptest.ids[pen_nr], SDL_PRESSED);

	while (SDL_PollEvent(&event)) {
	    if (event.type == SDL_EVENT_PEN_DOWN) {
		SDLTest_AssertCheck(event.ptip.which == ptest.ids[pen_nr],
			    "Received SDL_EVENT_PEN_DOWN from correct pen");
		SDLTest_AssertCheck(event.ptip.tip == (pen_nr == 0)? SDL_PEN_TIP_INK : SDL_PEN_TIP_ERASER,
				    "Received SDL_EVENT_PEN_DOWN for correct tip");
		SDLTest_AssertCheck(event.ptip.state == SDL_PRESSED,
				    "Received SDL_EVENT_PEN_DOWN but and marked SDL_PRESSED");
		SDLTest_AssertCheck(event.ptip.tip == tip,
				    "Received tip %x but expected %x", event.ptip.tip, tip);
		SDLTest_AssertCheck(event.ptip.pen_state == pen_state,
				    "Received SDL_EVENT_PEN_DOWN, and state %04x == %04x (expected)",
				    event.pbutton.pen_state, pen_state);
		SDLTest_AssertCheck((event.ptip.x == expected_x[pen_nr]) && (event.ptip.y == expected_y[pen_nr]),
				    "Received SDL_EVENT_PEN_DOWN event at correct coordinates: (%f, %f) vs (%f, %f) (expected)",
				    event.pbutton.x, event.pbutton.y, expected_x[pen_nr], expected_y[pen_nr]);
		SDLTest_AssertCheck(0 == SDL_memcmp(expected_axes, event.pbutton.axes, sizeof(float) * SDL_PEN_NUM_AXES),
				    "Received SDL_EVENT_PEN_DOWN event with correct axis values");
		found_event = SDL_TRUE;
	    }
            SDLTest_AssertCheck(found_event,
                                "Received the expected SDL_EVENT_PEN_DOWN event");
	}
    }

    SDLTest_AssertPass("Pen and eraser set up for button testing");

    /* Actual tests start: pen, then eraser */
    for (pen_nr = 0; pen_nr < 2; ++pen_nr) {
        Uint16 pen_state = 0x0000 | SDL_PEN_DOWN_MASK;
        float *expected_axes = axes + pen_nr;

        if (pen_nr == 1) {
            pen_state |= SDL_PEN_ERASER_MASK;
        }
        for (button_nr = 1; button_nr <= 8; ++button_nr) {
            SDL_bool found_event = SDL_FALSE;
            pen_state |= (1 << (button_nr - 1));

            SDL_SendPenButton(0, ptest.ids[pen_nr], SDL_PRESSED, button_nr);
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_PEN_BUTTON_DOWN) {
                    SDLTest_AssertCheck(event.pbutton.which == ptest.ids[pen_nr],
                                        "Received SDL_EVENT_PEN_BUTTON_DOWN from correct pen");
                    SDLTest_AssertCheck(event.pbutton.button == button_nr,
                                        "Received SDL_EVENT_PEN_BUTTON_DOWN from correct button");
                    SDLTest_AssertCheck(event.pbutton.state == SDL_PRESSED,
                                        "Received SDL_EVENT_PEN_BUTTON_DOWN but and marked SDL_PRESSED");
                    SDLTest_AssertCheck(event.pbutton.pen_state == pen_state,
                                        "Received SDL_EVENT_PEN_BUTTON_DOWN, and state %04x == %04x (expected)",
                                        event.pbutton.pen_state, pen_state);
                    SDLTest_AssertCheck((event.pbutton.x == expected_x[pen_nr]) && (event.pbutton.y == expected_y[pen_nr]),
                                        "Received SDL_EVENT_PEN_BUTTON_DOWN event at correct coordinates: (%f, %f) vs (%f, %f) (expected)",
                                        event.pbutton.x, event.pbutton.y, expected_x[pen_nr], expected_y[pen_nr]);
                    SDLTest_AssertCheck(0 == SDL_memcmp(expected_axes, event.pbutton.axes, sizeof(float) * SDL_PEN_NUM_AXES),
                                        "Received SDL_EVENT_PEN_BUTTON_DOWN event with correct axis values");
                    if (0 != SDL_memcmp(expected_axes, event.pbutton.axes, sizeof(float) * SDL_PEN_NUM_AXES)) {
                        int ax;
                        for (ax = 0; ax < SDL_PEN_NUM_AXES; ++ax) {
                            SDL_Log("\tax %d\t%.5f\t%.5f expected (equal=%d)",
                                    ax,
                                    event.pbutton.axes[ax], expected_axes[ax],
                                    event.pbutton.axes[ax] == expected_axes[ax]);
                        }
                    }
                    found_event = SDL_TRUE;
                }
            }
            SDLTest_AssertCheck(found_event,
                                "Received the expected SDL_EVENT_PEN_BUTTON_DOWN event");
        }
    }
    SDLTest_AssertPass("Pressed all buttons");

    /* Release every other button */
    for (pen_nr = 0; pen_nr < 2; ++pen_nr) {
        Uint16 pen_state = 0x00ff | SDL_PEN_DOWN_MASK; /* 8 buttons pressed */
        float *expected_axes = axes + pen_nr;

        if (pen_nr == 1) {
            pen_state |= SDL_PEN_ERASER_MASK;
        }
        for (button_nr = pen_nr + 1; button_nr <= 8; button_nr += 2) {
            SDL_bool found_event = SDL_FALSE;
            pen_state &= ~(1 << (button_nr - 1));

            SDL_SendPenButton(0, ptest.ids[pen_nr], SDL_RELEASED, button_nr);
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_PEN_BUTTON_UP) {
                    SDLTest_AssertCheck(event.pbutton.which == ptest.ids[pen_nr],
                                        "Received SDL_EVENT_PEN_BUTTON_UP from correct pen");
                    SDLTest_AssertCheck(event.pbutton.button == button_nr,
                                        "Received SDL_EVENT_PEN_BUTTON_UP from correct button");
                    SDLTest_AssertCheck(event.pbutton.state == SDL_RELEASED,
                                        "Received SDL_EVENT_PEN_BUTTON_UP and is marked SDL_RELEASED");
                    SDLTest_AssertCheck(event.pbutton.pen_state == pen_state,
                                        "Received SDL_EVENT_PEN_BUTTON_UP, and state %04x == %04x (expected)",
                                        event.pbutton.pen_state, pen_state);
                    SDLTest_AssertCheck((event.pbutton.x == expected_x[pen_nr]) && (event.pbutton.y == expected_y[pen_nr]),
                                        "Received SDL_EVENT_PEN_BUTTON_UP event at correct coordinates");
                    SDLTest_AssertCheck(0 == SDL_memcmp(expected_axes, event.pbutton.axes, sizeof(float) * SDL_PEN_NUM_AXES),
                                        "Received SDL_EVENT_PEN_BUTTON_UP event with correct axis values");
                    found_event = SDL_TRUE;
                }
            }
            SDLTest_AssertCheck(found_event,
                                "Received the expected SDL_EVENT_PEN_BUTTON_UP event");
        }
    }
    SDLTest_AssertPass("Released every other button");

    /* Trigger pen tip events for PEN_UP */
    SDLTest_AssertPass("Remove pens from surface");

    for (pen_nr = 0; pen_nr < 2; ++pen_nr) {
        float *expected_axes = axes + pen_nr;
	SDL_bool found_event = SDL_FALSE;
        Uint16 pen_state = 0x0000;
	Uint8 tip = SDL_PEN_TIP_INK;

        if (pen_nr == 1) {
            pen_state |= SDL_PEN_ERASER_MASK;
	    tip = SDL_PEN_TIP_ERASER;
        }

	SDL_SendPenTipEvent(0, ptest.ids[pen_nr], SDL_RELEASED);

	while (SDL_PollEvent(&event)) {
	    if (event.type == SDL_EVENT_PEN_UP) {
		SDLTest_AssertCheck(event.ptip.which == ptest.ids[pen_nr],
			    "Received SDL_EVENT_PEN_UP from correct pen");
		SDLTest_AssertCheck(event.ptip.tip == (pen_nr == 0)? SDL_PEN_TIP_INK : SDL_PEN_TIP_ERASER,
				    "Received SDL_EVENT_PEN_UP for correct tip");
		SDLTest_AssertCheck(event.ptip.state == SDL_RELEASED,
				    "Received SDL_EVENT_PEN_UP but and marked SDL_RELEASED");
		SDLTest_AssertCheck(event.ptip.tip == tip,
				    "Received tip %x but expected %x", event.ptip.tip, tip);
		SDLTest_AssertCheck((event.ptip.pen_state & 0xff00) == (pen_state & 0xff00),
				    "Received SDL_EVENT_PEN_UP, and state %04x == %04x (expected)",
				    event.pbutton.pen_state, pen_state);
		SDLTest_AssertCheck((event.ptip.x == expected_x[pen_nr]) && (event.ptip.y == expected_y[pen_nr]),
				    "Received SDL_EVENT_PEN_UP event at correct coordinates: (%f, %f) vs (%f, %f) (expected)",
				    event.pbutton.x, event.pbutton.y, expected_x[pen_nr], expected_y[pen_nr]);
		SDLTest_AssertCheck(0 == SDL_memcmp(expected_axes, event.pbutton.axes, sizeof(float) * SDL_PEN_NUM_AXES),
				    "Received SDL_EVENT_PEN_UP event with correct axis values");
		found_event = SDL_TRUE;
	    }
            SDLTest_AssertCheck(found_event,
                                "Received the expected SDL_EVENT_PEN_UP event");
	}
    }

    /* Cleanup */
    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);
    _teardown_test(&ptest, backup);

    return TEST_COMPLETED;
}

/**
 * @brief Check pen device movement and axis update reporting
 *
 * Also tests SDL_GetPenStatus for agreement with the most recently reported events
 *
 * @sa SDL_GetPenStatus
 */
static int
pen_movementAndAxes(void *arg)
{
    pen_testdata ptest;
    SDL_Event event;
#define MAX_STEPS 80
    /* Pen simulation */
    simulated_pen_action steps[MAX_STEPS];
    size_t num_steps = 0;

    SDL_Pen simulated_pens[2];
    int sim_pc = 0;
    simulated_pen_action *last_action;

    /* Register pen */
    deviceinfo_backup *backup = _setup_test(&ptest, 2);

    /* Pen simulation program */
#define STEP steps[num_steps++] =

    /* #1: Check basic reporting */
    /* Hover eraser, tilt axes */
    SIMPEN_MOVE(0, 30.0f, 31.0f);
    SIMPEN_AXIS(0, SDL_PEN_AXIS_PRESSURE, 0.0f);
    SIMPEN_AXIS(0, SDL_PEN_AXIS_XTILT, 22.5f);
    SIMPEN_AXIS(0, SDL_PEN_AXIS_YTILT, 45.0f);
    SIMPEN_EVENT_MOTION(0);

    /* #2: Check that motion events without motion aren't reported */
    SIMPEN_EVENT_MOTION_SUPPRESSED(0);
    SIMPEN_EVENT_MOTION_SUPPRESSED(0);

    /* #3: Check multiple pens being reported */
    /* Move pen and touch surface, don't tilt */
    SIMPEN_MOVE(1, 40.0f, 41.0f);
    SIMPEN_AXIS(1, SDL_PEN_AXIS_PRESSURE, 0.25f);
    SIMPEN_EVENT_MOTION(1);

    /* $4: Multi-buttons */
    /* Press eraser buttons */
    SIMPEN_EVENT_TIP(0, "down", SDL_PEN_TIP_ERASER);
    SIMPEN_EVENT_BUTTON(0, "push", 2);
    SIMPEN_EVENT_BUTTON(0, "push", 1);
    SIMPEN_EVENT_BUTTON(0, 0, 2); /* release again */
    SIMPEN_EVENT_BUTTON(0, "push", 3);

    /* #5: Check move + button actions connecting */
    /* Move and tilt pen, press some pen buttons */
    SIMPEN_MOVE(1, 3.0f, 8.0f);
    SIMPEN_AXIS(1, SDL_PEN_AXIS_PRESSURE, 0.5f);
    SIMPEN_AXIS(1, SDL_PEN_AXIS_XTILT, -21.0f);
    SIMPEN_AXIS(1, SDL_PEN_AXIS_YTILT, -25.0f);
    SIMPEN_EVENT_MOTION(1);
    SIMPEN_EVENT_BUTTON(1, "push", 2);
    SIMPEN_EVENT_TIP(1, "down", SDL_PEN_TIP_INK);

    /* #6: Check nonterference between pens */
    /* Eraser releases buttons */
    SIMPEN_EVENT_BUTTON(0, 0, 1);
    SIMPEN_EVENT_TIP(0, 0, SDL_PEN_TIP_ERASER);

    /* #7: Press-move-release action */
    /* Eraser press-move-release */
    SIMPEN_EVENT_BUTTON(0, "push", 1);
    SIMPEN_MOVE(0, 99.0f, 88.0f);
    SIMPEN_AXIS(0, SDL_PEN_AXIS_PRESSURE, 0.625f);
    SIMPEN_EVENT_MOTION(0);
    SIMPEN_MOVE(0, 44.5f, 42.25f);
    SIMPEN_EVENT_MOTION(0);
    SIMPEN_EVENT_BUTTON(0, 0, 1);

    /* #8: Intertwining button release actions some more */
    /* Pen releases button */
    SIMPEN_EVENT_BUTTON(1, 0, 2);
    SIMPEN_EVENT_TIP(1, 0, SDL_PEN_TIP_INK);

    /* Push one more pen button, then release all ereaser buttons */
    SIMPEN_EVENT_TIP(1, "down", SDL_PEN_TIP_INK);
    SIMPEN_EVENT_BUTTON(0, 0, 2);
    SIMPEN_EVENT_BUTTON(0, 0, 3);

    /* Lift up pen, flip it so it becomes an eraser, and touch it again  */
    SIMPEN_EVENT_TIP(1, 0, SDL_PEN_TIP_INK);
    SIMPEN_SET_ERASER(1, 1);
    SIMPEN_EVENT_TIP(1, "push", SDL_PEN_TIP_ERASER);

    /* And back again */
    SIMPEN_EVENT_TIP(1, 0, SDL_PEN_TIP_ERASER);
    SIMPEN_SET_ERASER(1, 0);
    SIMPEN_EVENT_TIP(1, "push", SDL_PEN_TIP_INK);

    /* #9: Suppress move on unsupported axis */
    SIMPEN_AXIS(1, SDL_PEN_AXIS_DISTANCE, 0.25f);
    SIMPEN_EVENT_MOTION_SUPPRESSED(0);

    SIMPEN_DONE();
#undef STEP
    /* End of pen simulation program */

    SDLTest_AssertCheck(num_steps < MAX_STEPS, "Pen simulation program does not exceed buffer size");
#undef MAX_STEPS

    SDL_PenGCMark();
    _pen_setDeviceinfo(_pen_register(ptest.ids[0], ptest.guids[0], "test eraser",
                                     SDL_PEN_ERASER_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK),
                       20);
    _pen_setDeviceinfo(_pen_register(ptest.ids[1], ptest.guids[1], "test pen",
                                     SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK),
                       24);
    _pen_trackGCSweep(&ptest);
    SDL_SendPenWindowEvent(0, ptest.ids[0], ptest.window);
    SDL_SendPenWindowEvent(0, ptest.ids[1], ptest.window);
    while (SDL_PollEvent(&event))
        ; /* Flush event queue */
    SDLTest_AssertPass("Pen and eraser set up for testing");

    _pen_simulate_init(&ptest, simulated_pens, 2);
    /* Simulate pen movements */
    while ((last_action = _pen_simulate(steps, &sim_pc, &simulated_pens[0], 2))) {
        int attempts = 0;
        SDL_Pen *simpen = &simulated_pens[last_action->pen_index];
        SDL_PenID reported_which = -1;
        float reported_x = -1.0f, reported_y = -1.0f;
        float *reported_axes = NULL;
        Uint32 reported_pen_state = 0;
        Uint32 expected_pen_state = simpen->header.flags & SDL_PEN_ERASER_MASK;
        SDL_bool dump_pens = SDL_FALSE;

        do {
            SDL_PumpEvents();
            SDL_PollEvent(&event);
            if (++attempts > 10000) {
                SDLTest_AssertCheck(0, "Never got the anticipated event");
                return TEST_ABORTED;
            }
        } while (event.type != SDL_EVENT_PEN_DOWN
		 && event.type != SDL_EVENT_PEN_UP
		 && event.type != SDL_EVENT_PEN_MOTION
                 && event.type != SDL_EVENT_PEN_BUTTON_UP
                 && event.type != SDL_EVENT_PEN_BUTTON_DOWN); /* skip boring events */

        expected_pen_state |= simpen->last.buttons;

        SDLTest_AssertCheck(0 != event.type,
                            "Received the anticipated event");

        switch (last_action->type) {
        case SIMPEN_ACTION_MOTION_EVENT:
            SDLTest_AssertCheck(event.type == SDL_EVENT_PEN_MOTION, "Expected pen motion event (but got 0x%lx)", (unsigned long) event.type);
            reported_which = event.pmotion.which;
            reported_x = event.pmotion.x;
            reported_y = event.pmotion.y;
            reported_pen_state = event.pmotion.pen_state;
            reported_axes = &event.pmotion.axes[0];
            break;

        case SIMPEN_ACTION_PRESS:
            SDLTest_AssertCheck(event.type == SDL_EVENT_PEN_BUTTON_DOWN, "Expected PENBUTTONDOWN event (but got 0x%lx)", (unsigned long) event.type);
            SDLTest_AssertCheck(event.pbutton.state == SDL_PRESSED, "Expected PRESSED button");
            /* Fall through */
        case SIMPEN_ACTION_RELEASE:
            if (last_action->type == SIMPEN_ACTION_RELEASE) {
                SDLTest_AssertCheck(event.type == SDL_EVENT_PEN_BUTTON_UP, "Expected PENBUTTONUP event (but got 0x%lx)", (unsigned long) event.type);
                SDLTest_AssertCheck(event.pbutton.state == SDL_RELEASED, "Expected RELEASED button");
            }
            SDLTest_AssertCheck(event.pbutton.button == last_action->index, "Expected button %d, but got %d",
                                last_action->index, event.pbutton.button);
            reported_which = event.pbutton.which;
            reported_x = event.pbutton.x;
            reported_y = event.pbutton.y;
            reported_pen_state = event.pbutton.pen_state;
            reported_axes = &event.pbutton.axes[0];
            break;

        case SIMPEN_ACTION_DOWN:
            SDLTest_AssertCheck(event.type == SDL_EVENT_PEN_DOWN, "Expected PENBUTTONDOWN event (but got 0x%lx)", (unsigned long) event.type);
            SDLTest_AssertCheck(event.ptip.state == SDL_PRESSED, "Expected PRESSED button");
            /* Fall through */
        case SIMPEN_ACTION_UP:
            if (last_action->type == SIMPEN_ACTION_UP) {
                SDLTest_AssertCheck(event.type == SDL_EVENT_PEN_UP, "Expected PENBUTTONUP event (but got 0x%lx)", (unsigned long) event.type);
                SDLTest_AssertCheck(event.ptip.state == SDL_RELEASED, "Expected RELEASED button");
            }
            SDLTest_AssertCheck(event.ptip.tip == last_action->index, "Expected tip %d, but got %d",
                                last_action->index, event.ptip.tip);
            reported_which = event.ptip.which;
            reported_x = event.ptip.x;
            reported_y = event.ptip.y;
            reported_pen_state = event.ptip.pen_state;
            reported_axes = &event.ptip.axes[0];
            break;

        case SIMPEN_ACTION_ERASER_MODE:
	    break;

        default:
            SDLTest_AssertCheck(0, "Error in pen simulator: unexpected action %d", last_action->type);
            return TEST_ABORTED;
        }

        if (reported_which != simpen->header.id) {
            dump_pens = SDL_TRUE;
            SDLTest_AssertCheck(0, "Expected report for pen %lu but got report for pen %lu",
                                (unsigned long) simpen->header.id,
                                (unsigned long) reported_which);
        }
        if (reported_x != simpen->last.x || reported_y != simpen->last.y) {
            dump_pens = SDL_TRUE;
            SDLTest_AssertCheck(0, "Mismatch in pen coordinates");
        }
        if (reported_x != simpen->last.x || reported_y != simpen->last.y) {
            dump_pens = SDL_TRUE;
            SDLTest_AssertCheck(0, "Mismatch in pen coordinates");
        }
        if (reported_pen_state != expected_pen_state) {
            dump_pens = SDL_TRUE;
            SDLTest_AssertCheck(0, "Mismatch in pen state: %lx vs %lx (expected)",
                                (unsigned long) reported_pen_state,
                                (unsigned long) expected_pen_state);
        }
        if (0 != SDL_memcmp(reported_axes, simpen->last.axes, sizeof(float) * SDL_PEN_NUM_AXES)) {
            dump_pens = SDL_TRUE;
            SDLTest_AssertCheck(0, "Mismatch in axes");
        }

        if (dump_pens) {
            SDL_Log("----- Pen #%d:", last_action->pen_index);
            _pen_dump("expect", simpen);
            _pen_dump("actual", SDL_GetPenPtr(simpen->header.id));
        }
    }
    SDLTest_AssertPass("Pen and eraser move and report events correctly and independently");

    /* Cleanup */
    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);
    _teardown_test(&ptest, backup);
    return TEST_COMPLETED;
}

static void
_expect_pen_config(SDL_PenID penid,
                   SDL_GUID expected_guid,
                   SDL_bool expected_attached,
                   char *expected_name,
                   int expected_type,
                   int expected_num_buttons,
                   float expected_max_tilt,
                   int expected_axes)
{
    SDL_PenCapabilityInfo actual_info = { 0 };
    const char *actual_name = SDL_GetPenName(penid);

    if (penid == SDL_PEN_INVALID) {
        SDLTest_Assert(0, "Invalid pen ID");
        return;
    }

    SDLTest_AssertEq1(int, "%d", 0, SDL_GUIDCompare(expected_guid, SDL_GetPenGUID(penid)),
                      "Pen %lu guid equality", (unsigned long) penid);

    SDLTest_AssertCheck(0 == SDL_strcmp(expected_name, actual_name),
                        "Expected name='%s' vs actual='%s'", expected_name, actual_name);

    SDLTest_AssertEq1(int, "%d", expected_attached, SDL_PenConnected(penid),
                      "Pen %lu is attached", (unsigned long) penid);
    SDLTest_AssertEq1(int, "%d", expected_type, SDL_GetPenType(penid),
                      "Pen %lu type", (unsigned long) penid);
    SDLTest_AssertEq1(int, "%x", expected_axes, SDL_GetPenCapabilities(penid, &actual_info),
                      "Pen %lu axis flags", (unsigned long) penid);
    SDLTest_AssertEq1(int, "%d", expected_num_buttons, actual_info.num_buttons,
                      "Pen %lu number of buttons", (unsigned long) penid);
    SDLTest_AssertEq1(float, "%f", expected_max_tilt, actual_info.max_tilt,
                      "Pen %lu max tilt", (unsigned long) penid);
}

/**
 * @brief Check backend pen iniitalisation and pen meta-information
 *
 * @sa SDL_GetPenCapabilities, SDL_PenAxisInfo
 */
static int
pen_initAndInfo(void *arg)
{
    pen_testdata ptest;
    SDL_Pen *pen;
    Uint32 mask;
    char strbuf[SDL_PEN_MAX_NAME];

    /* Init */
    deviceinfo_backup *backup = _setup_test(&ptest, 7);

    /* Register default pen */
    _expect_pens_attached_or_detached(ptest.ids, 7, 0);

    /* Register completely default pen */
    pen = SDL_PenModifyBegin(ptest.ids[0]);
    SDL_memcpy(pen->guid.data, ptest.guids[0].data, sizeof(ptest.guids[0].data));
    SDL_PenModifyEnd(pen, SDL_TRUE);

    SDL_snprintf(strbuf, sizeof(strbuf),
                 "Pen %lu", (unsigned long) ptest.ids[0]);
    _expect_pen_config(ptest.ids[0], ptest.guids[0], SDL_TRUE,
                       strbuf, SDL_PEN_TYPE_PEN, SDL_PEN_INFO_UNKNOWN, 0.0f,
                       SDL_PEN_INK_MASK);
    _expect_pens_attached_or_detached(ptest.ids, 7, ATTACHED(0));
    SDLTest_AssertPass("Pass #1: default pen");

    /* Register mostly-default pen with buttons and custom name */
    pen = SDL_PenModifyBegin(ptest.ids[1]);
    SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_PRESSURE_MASK);
    SDL_memcpy(pen->guid.data, ptest.guids[1].data, sizeof(ptest.guids[1].data));
    SDL_strlcpy(strbuf, "My special test pen", SDL_PEN_MAX_NAME);
    SDL_strlcpy(pen->name, strbuf, SDL_PEN_MAX_NAME);
    pen->info.num_buttons = 7;
    SDL_PenModifyEnd(pen, SDL_TRUE);

    _expect_pen_config(ptest.ids[1], ptest.guids[1], SDL_TRUE,
                       strbuf, SDL_PEN_TYPE_PEN, 7, 0.0f,
                       SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK);
    _expect_pens_attached_or_detached(ptest.ids, 7, ATTACHED(0) | ATTACHED(1));
    SDLTest_AssertPass("Pass #2: default pen with button and name info");

    /* Register eraser with default name, but keep initially detached */
    pen = SDL_PenModifyBegin(ptest.ids[2]);
    SDL_memcpy(pen->guid.data, ptest.guids[2].data, sizeof(ptest.guids[2].data));
    pen->type = SDL_PEN_TYPE_ERASER;
    SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK);
    SDL_PenModifyEnd(pen, SDL_FALSE);

    SDL_snprintf(strbuf, sizeof(strbuf),
                 "Eraser %lu", (unsigned long) ptest.ids[2]);
    _expect_pen_config(ptest.ids[2], ptest.guids[2], SDL_FALSE,
                       strbuf, SDL_PEN_TYPE_ERASER, SDL_PEN_INFO_UNKNOWN, SDL_PEN_INFO_UNKNOWN,
                       SDL_PEN_ERASER_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK);
    _expect_pens_attached_or_detached(ptest.ids, 7, ATTACHED(0) | ATTACHED(1));
    /* now make available */
    SDL_PenModifyEnd(SDL_PenModifyBegin(ptest.ids[2]), SDL_TRUE);
    _expect_pen_config(ptest.ids[2], ptest.guids[2], SDL_TRUE,
                       strbuf, SDL_PEN_TYPE_ERASER, SDL_PEN_INFO_UNKNOWN, SDL_PEN_INFO_UNKNOWN,
                       SDL_PEN_ERASER_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK);
    _expect_pens_attached_or_detached(ptest.ids, 7, ATTACHED(0) | ATTACHED(1) | ATTACHED(2));
    SDLTest_AssertPass("Pass #3: eraser-type pen initially detached, then attached");

    /* Abort pen registration */
    pen = SDL_PenModifyBegin(ptest.ids[3]);
    SDL_memcpy(pen->guid.data, ptest.guids[3].data, sizeof(ptest.guids[3].data));
    SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK);
    pen->type = SDL_PEN_TYPE_NONE;
    SDL_PenModifyEnd(pen, SDL_TRUE);
    _expect_pens_attached_or_detached(ptest.ids, 7, ATTACHED(0) | ATTACHED(1) | ATTACHED(2));
    SDLTest_AssertCheck(NULL == SDL_GetPenName(ptest.ids[3]), "Pen with aborted registration remains unknown");
    SDLTest_AssertPass("Pass #4: aborted pen registration");

    /* Brush with custom axes */
    pen = SDL_PenModifyBegin(ptest.ids[4]);
    SDL_memcpy(pen->guid.data, ptest.guids[4].data, sizeof(ptest.guids[4].data));
    SDL_strlcpy(pen->name, "Testish Brush", SDL_PEN_MAX_NAME);
    pen->type = SDL_PEN_TYPE_BRUSH;
    pen->info.num_buttons = 1;
    SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_ROTATION_MASK);
    pen->info.max_tilt = 72.5f;
    SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_XTILT_MASK);
    SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_PRESSURE_MASK);
    SDL_PenModifyEnd(pen, SDL_TRUE);
    _expect_pen_config(ptest.ids[4], ptest.guids[4], SDL_TRUE,
                       "Testish Brush", SDL_PEN_TYPE_BRUSH, 1, 72.5f,
                       SDL_PEN_INK_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_ROTATION_MASK | SDL_PEN_AXIS_PRESSURE_MASK);
    _expect_pens_attached_or_detached(ptest.ids, 7, ATTACHED(0) | ATTACHED(1) | ATTACHED(2) | ATTACHED(4));
    SDLTest_AssertPass("Pass #5: brush-type pen with unusual axis layout");

    /* Wacom airbrush pen */
    {
        const Uint32 wacom_type_id = 0x0912;
        const Uint32 wacom_serial_id = 0xa0b1c2d3;
        SDL_GUID guid = {
            { 0, 0, 0, 0,
              0, 0, 0, 0,
              0, 0, 0, 0,
              0, 0, 0, 0 }
        };
        guid.data[0] = (wacom_serial_id >> 0) & 0xff;
        guid.data[1] = (wacom_serial_id >> 8) & 0xff;
        guid.data[2] = (wacom_serial_id >> 16) & 0xff;
        guid.data[3] = (wacom_serial_id >> 24) & 0xff;
        guid.data[4] = (wacom_type_id >> 0) & 0xff;
        guid.data[5] = (wacom_type_id >> 8) & 0xff;
        guid.data[6] = (wacom_type_id >> 16) & 0xff;
        guid.data[7] = (wacom_type_id >> 24) & 0xff;

        pen = SDL_PenModifyBegin(ptest.ids[5]);
        SDL_PenModifyForWacomID(pen, wacom_type_id, &mask);
        SDL_PenUpdateGUIDForWacom(&pen->guid, wacom_type_id, wacom_serial_id);
        SDL_PenModifyAddCapabilities(pen, mask);
        SDL_PenModifyEnd(pen, SDL_TRUE);
        _expect_pen_config(ptest.ids[5], guid, SDL_TRUE,
                           "Wacom Airbrush Pen", SDL_PEN_TYPE_AIRBRUSH, 1, 64.0f, /* Max tilt angle */
                           SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_SLIDER_MASK);
        _expect_pens_attached_or_detached(ptest.ids, 7, ATTACHED(0) | ATTACHED(1) | ATTACHED(2) | ATTACHED(4) | ATTACHED(5));
    }
    SDLTest_AssertPass("Pass #6: wacom airbrush pen");

    /* Cleanup */
    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);
    _teardown_test(&ptest, backup);
    return TEST_COMPLETED;
}

#define SET_POS(update, xpos, ypos) \
    (update).x = (xpos);            \
    (update).y = (ypos);

static void
_penmouse_expect_button(int type, int button)
{
    SDL_bool press = type == SDL_PRESSED;
    SDLTest_AssertCheck((press ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP) == _mouseemu_last_event,
                        "Mouse button %s: %x",
                        (press ? "press" : "release"), _mouseemu_last_event);
    SDLTest_AssertCheck(button == _mouseemu_last_button,
                        "Observed the expected simulated button: %d", _mouseemu_last_button);
    SDLTest_AssertCheck(SDL_PEN_MOUSEID == _mouseemu_last_mouseid,
                        "Observed the expected mouse ID: 0x%x", _mouseemu_last_mouseid);

    _mouseemu_last_event = 0;
}

/**
 * @brief Check pen device mouse emulation and event suppression without SDL_HINT_PEN_DELAY_MOUSE_BUTTON
 *
 * Since we include SDL_pen.c, we link it against our own mock implementations of SDL_PSendMouseButton
 * and SDL_SendMouseMotion; see tehere for details.
 */
static int
pen_mouseEmulation(void *arg)
{
    pen_testdata ptest;
    SDL_Event event;
    int i;
    SDL_PenStatusInfo update;
    deviceinfo_backup *backup;

    pen_delay_mouse_button_mode = 0;
    pen_mouse_emulation_mode = PEN_MOUSE_EMULATE; /* to trigger our own SDL_SendMouseButton */

    /* Register pen */
    backup = _setup_test(&ptest, 1);
    SDL_PenGCMark();
    _pen_setDeviceinfo(_pen_register(ptest.ids[0], ptest.guids[0], "testpen",
                                     SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT | SDL_PEN_AXIS_YTILT),
                       20);
    _pen_trackGCSweep(&ptest);

    /* Move pen into window */
    SDL_SendPenWindowEvent(0, ptest.ids[0], ptest.window);

    /* Initialise pen location */
    SDL_memset(update.axes, 0, sizeof(update.axes));
    SET_POS(update, 100.0f, 100.0f);
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);
    while (SDL_PollEvent(&event))
        ; /* Flush event queue */

    /* Test motion forwarding */
    _mouseemu_last_event = 0;
    SET_POS(update, 121.25f, 110.75f);
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);
    SDLTest_AssertCheck(SDL_EVENT_MOUSE_MOTION == _mouseemu_last_event,
                        "Mouse motion event: %d", _mouseemu_last_event);
    SDLTest_AssertCheck(121.25f == _mouseemu_last_x && 110.75f == _mouseemu_last_y,
                        "Motion to correct position: %f,%f", _mouseemu_last_x, _mouseemu_last_y);
    SDLTest_AssertCheck(SDL_PEN_MOUSEID == _mouseemu_last_mouseid,
                        "Observed the expected mouse ID: 0x%x", _mouseemu_last_mouseid);
    SDLTest_AssertCheck(0 == _mouseemu_last_relative,
                        "Absolute motion event");
    SDLTest_AssertPass("Motion emulation");

    /* Test redundant motion report suppression */
    _mouseemu_last_event = 0;

    SET_POS(update, 121.25f, 110.75f);
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);

    SET_POS(update, 121.25f, 110.75f);
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);

    update.axes[0] = 1.0f;
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);

    SET_POS(update, 121.25f, 110.75f);
    update.axes[0] = 0.0f;
    update.axes[1] = 0.75f;
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);

    SDLTest_AssertCheck(0 == _mouseemu_last_event,
                        "Redundant mouse motion suppressed: %d", _mouseemu_last_event);
    SDLTest_AssertPass("Redundant motion suppression");

    /* Test button press reporting */
    SDL_SendPenTipEvent(0, ptest.ids[0], SDL_PRESSED);
    _penmouse_expect_button(SDL_PRESSED, 1);

    for (i = 1; i <= 3; ++i) {
        SDL_SendPenButton(0, ptest.ids[0], SDL_PRESSED, i);
        _penmouse_expect_button(SDL_PRESSED, i + 1);
    }
    SDLTest_AssertPass("Button press mouse emulation");

    /* Test button release reporting */
    SDL_SendPenTipEvent(0, ptest.ids[0], SDL_RELEASED);
    _penmouse_expect_button(SDL_RELEASED, 1);

    for (i = 1; i <= 3; ++i) {
        SDL_SendPenButton(0, ptest.ids[0], SDL_RELEASED, i);
        _penmouse_expect_button(SDL_RELEASED, i + 1);
    }
    SDLTest_AssertPass("Button release mouse emulation");

    /* Cleanup */
    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);
    _teardown_test(&ptest, backup);
    return TEST_COMPLETED;
}

/**
 * @brief Check pen device mouse emulation when SDL_HINT_PEN_DELAY_MOUSE_BUTTON is enabled (default)
 */
static int
pen_mouseEmulationDelayed(void *arg)
{
    pen_testdata ptest;
    SDL_Event event;
    int i;
    SDL_PenStatusInfo update;
    deviceinfo_backup *backup;

    pen_delay_mouse_button_mode = 1;
    pen_mouse_emulation_mode = PEN_MOUSE_EMULATE; /* to trigger our own SDL_SendMouseButton */

    /* Register pen */
    backup = _setup_test(&ptest, 1);
    SDL_PenGCMark();
    _pen_setDeviceinfo(_pen_register(ptest.ids[0], ptest.guids[0], "testpen",
                                     SDL_PEN_INK_MASK | SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT | SDL_PEN_AXIS_YTILT),
                       20);
    _pen_trackGCSweep(&ptest);

    /* Move pen into window */
    SDL_SendPenWindowEvent(0, ptest.ids[0], ptest.window);

    /* Initialise pen location */
    SDL_memset(update.axes, 0, sizeof(update.axes));
    SET_POS(update, 100.0f, 100.0f);
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);
    while (SDL_PollEvent(&event))
        ; /* Flush event queue */

    /* Test motion forwarding */
    _mouseemu_last_event = 0;
    SET_POS(update, 121.25f, 110.75f);
    SDL_SendPenMotion(0, ptest.ids[0], SDL_TRUE, &update);
    SDLTest_AssertCheck(SDL_EVENT_MOUSE_MOTION == _mouseemu_last_event,
                        "Mouse motion event: %d", _mouseemu_last_event);
    SDLTest_AssertCheck(121.25f == _mouseemu_last_x && 110.75f == _mouseemu_last_y,
                        "Motion to correct position: %f,%f", _mouseemu_last_x, _mouseemu_last_y);
    SDLTest_AssertCheck(SDL_PEN_MOUSEID == _mouseemu_last_mouseid,
                        "Observed the expected mouse ID: 0x%x", _mouseemu_last_mouseid);
    SDLTest_AssertCheck(0 == _mouseemu_last_relative,
                        "Absolute motion event");
    SDLTest_AssertPass("Motion emulation");
    _mouseemu_last_event = 0;

    /* Test button press reporting */
    for (i = 1; i <= 2; ++i) {
        SDL_SendPenButton(0, ptest.ids[0], SDL_PRESSED, i);
        SDLTest_AssertCheck(0 == _mouseemu_last_event,
                            "Non-touching button press suppressed: %d", _mouseemu_last_event);
        SDL_SendPenButton(0, ptest.ids[0], SDL_RELEASED, i);
        SDLTest_AssertCheck(0 == _mouseemu_last_event,
                            "Non-touching button release suppressed: %d", _mouseemu_last_event);
    }

    /* Touch surface */
    SDL_SendPenTipEvent(0, ptest.ids[0], SDL_PRESSED);
    _penmouse_expect_button(SDL_PRESSED, 1);
    SDL_SendPenTipEvent(0, ptest.ids[0], SDL_RELEASED);
    _penmouse_expect_button(SDL_RELEASED, 1);

    /* Test button press reporting, releasing extra button AFTER lifting pen */
    for (i = 1; i <= 2; ++i) {
        SDL_SendPenButton(0, ptest.ids[0], SDL_PRESSED, i);
        SDLTest_AssertCheck(0 == _mouseemu_last_event,
                            "Non-touching button press suppressed (A.1): %d", _mouseemu_last_event);
	SDL_SendPenTipEvent(0, ptest.ids[0], SDL_PRESSED);
        _penmouse_expect_button(SDL_PRESSED, i + 1);

	SDL_SendPenTipEvent(0, ptest.ids[0], SDL_RELEASED);
        _penmouse_expect_button(SDL_RELEASED, i + 1);

        SDL_SendPenButton(0, ptest.ids[0], SDL_RELEASED, i);
        SDLTest_AssertCheck(0 == _mouseemu_last_event,
                            "Non-touching button press suppressed (A.2): %d", _mouseemu_last_event);
    }
    SDLTest_AssertPass("Delayed button press mouse emulation, touching without releasing button");

    /* Test button press reporting, releasing extra button BEFORE lifting pen */
    for (i = 1; i <= 2; ++i) {
        SDL_SendPenButton(0, ptest.ids[0], SDL_PRESSED, i);
        SDLTest_AssertCheck(0 == _mouseemu_last_event,
                            "Non-touching button press suppressed (B.1): %d", _mouseemu_last_event);
	SDL_SendPenTipEvent(0, ptest.ids[0], SDL_PRESSED);
        _penmouse_expect_button(SDL_PRESSED, i + 1);

        SDL_SendPenButton(0, ptest.ids[0], SDL_RELEASED, i);
        SDLTest_AssertCheck(0 == _mouseemu_last_event,
                            "Non-touching button press suppressed (B.2): %d", _mouseemu_last_event);
	SDL_SendPenTipEvent(0, ptest.ids[0], SDL_RELEASED);
        _penmouse_expect_button(SDL_RELEASED, i + 1);
    }
    SDLTest_AssertPass("Delayed button press mouse emulation, touching and then releasing button");

    /* Cleanup */
    SDL_PenGCMark();
    _pen_trackGCSweep(&ptest);
    _teardown_test(&ptest, backup);
    return TEST_COMPLETED;
}

/**
 * @brief Ensure that all SDL_Pen*Event structures have compatible memory layout, as expected by SDL_pen.c
 */
static int
pen_memoryLayout(void *arg)
{
#define LAYOUT_COMPATIBLE(field)					\
    SDLTest_AssertCheck(offsetof(SDL_PenTipEvent, field) == offsetof(SDL_PenMotionEvent, field), \
			"Memory layout SDL_PenTipEvent and SDL_PenMotionEvent compatibility: '" #field "'"); \
    SDLTest_AssertCheck(offsetof(SDL_PenTipEvent, field) == offsetof(SDL_PenButtonEvent, field), \
			"Memory layout SDL_PenTipEvent and SDL_PenBUttonEvent compatibility: '" #field "'");

    LAYOUT_COMPATIBLE(which);
    LAYOUT_COMPATIBLE(x);
    LAYOUT_COMPATIBLE(y);
    LAYOUT_COMPATIBLE(axes);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Mouse test cases */
static const SDLTest_TestCaseReference penTest1 = { (SDLTest_TestCaseFp)pen_iteration, "pen_iteration", "Iterate over all pens with SDL_PenIDForIndex", TEST_ENABLED };

static const SDLTest_TestCaseReference penTest2 = { (SDLTest_TestCaseFp)pen_hotplugging, "pen_hotplugging", "Hotplug pens and validate their status, including SDL_PenConnected", TEST_ENABLED };

static const SDLTest_TestCaseReference penTest3 = { (SDLTest_TestCaseFp)pen_GUIDs, "pen_GUIDs", "Check Pen SDL_GUID operations", TEST_ENABLED };

static const SDLTest_TestCaseReference penTest4 = { (SDLTest_TestCaseFp)pen_buttonReporting, "pen_buttonReporting", "Check pen button presses", TEST_ENABLED };

static const SDLTest_TestCaseReference penTest5 = { (SDLTest_TestCaseFp)pen_movementAndAxes, "pen_movementAndAxes", "Check pen movement and axis update reporting", TEST_ENABLED };

static const SDLTest_TestCaseReference penTest6 = { (SDLTest_TestCaseFp)pen_initAndInfo, "pen_info", "Check pen self-description and initialisation", TEST_ENABLED };

static const SDLTest_TestCaseReference penTest7 = { (SDLTest_TestCaseFp)pen_mouseEmulation, "pen_mouseEmulation", "Check pen-as-mouse event forwarding (direct)", TEST_ENABLED };

static const SDLTest_TestCaseReference penTest8 = { (SDLTest_TestCaseFp)pen_mouseEmulationDelayed, "pen_mouseEmulationDelayed", "Check pen-as-mouse event forwarding (delayed)", TEST_ENABLED };

static const SDLTest_TestCaseReference penTest9 = { (SDLTest_TestCaseFp)pen_memoryLayout, "pen_memoryLayout", "Check that all pen events have compatible layout (required by SDL_pen.c)", TEST_ENABLED };

/* Sequence of Mouse test cases */
static const SDLTest_TestCaseReference *penTests[] = {
    &penTest1, &penTest2, &penTest3, &penTest4, &penTest5, &penTest6, &penTest7, &penTest8, &penTest9, NULL
};

/* Mouse test suite (global) */
SDLTest_TestSuiteReference penTestSuite = {
    "Pen",
    NULL,
    penTests,
    NULL
};
