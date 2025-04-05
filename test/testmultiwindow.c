/*
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Program that creates a window at the global position of the mouse */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Color color;
} TestWindow;

static void DestroyTestWindow(TestWindow *testWindow)
{
  SDL_DestroyRenderer(testWindow->renderer);
  SDL_DestroyWindow(testWindow->window);
}

static bool ColorIsBright(SDL_Color color)
{
  return (color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722) > 128.0;
}

static void RenderTestWindow(TestWindow *testWindow)
{
  SDL_Window *window = testWindow->window;
  SDL_Renderer *renderer = testWindow->renderer;
  SDL_Color color = testWindow->color;

  if (renderer) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer);

    if (ColorIsBright(color)) {
      color.r = 0;
      color.g = 0;
      color.b = 0;
    }
    else {
      color.r = 255;
      color.g = 255;
      color.b = 255;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int x, y;
    SDL_GetWindowPosition(window, &x, &y);
    SDL_RenderDebugTextFormat(renderer, 0, 0, "Global Window Position: %d %d", x, y);
    SDL_RenderPresent(renderer);
  }
}

#define MAX_WINDOWS 8

typedef struct {
  SDLTest_CommonState *state;
  TestWindow *testWindows[MAX_WINDOWS];
  Uint64 lastCreate;
} TestState;

static void DestroyTestState(TestState *testState)
{
  int i;
  SDLTest_CommonDestroyState(testState->state);
  for (i = 0; i < MAX_WINDOWS; ++i) {
    if (testState->testWindows[i]) {
      DestroyTestWindow(testState->testWindows[i]);
    }
  }
}

static TestWindow *createTestWindowAtMousePosition(SDLTest_CommonState *state)
{
  static int windowId = 0;

  TestWindow *testWindow;

  char title[1024];
  SDL_Rect rect;
  SDL_Rect bounds;
  SDL_PropertiesID props;
  float x, y;

  SDL_GetDisplayUsableBounds(state->displayID, &bounds);

  SDL_snprintf(title, SDL_arraysize(title), "#window%d", ++windowId);

  SDL_GetGlobalMouseState(&x, &y);
  rect.x = (int)SDL_ceilf(x);
  rect.y = (int)SDL_ceilf(y);

  rect.w = state->window_w;
  rect.h = state->window_h;
  if (state->auto_scale_content) {
    float scale = SDL_GetDisplayContentScale(state->displayID);
    rect.w = (int)SDL_ceilf(rect.w * scale);
    rect.h = (int)SDL_ceilf(rect.h * scale);
  }

  /* Don't make a window if it wouldn't be visible in the display */
  if (!SDL_HasRectIntersection(&rect, &bounds)) {
    return NULL;
  }

  props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_EMSCRIPTEN_CANVAS_ID_STRING, title);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, rect.x);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, rect.y);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, rect.w);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, rect.h);
  SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, state->window_flags);

  testWindow = SDL_calloc(1, sizeof(TestWindow));
  if (!testWindow) {
    return NULL;
  }
  testWindow->window = SDL_CreateWindowWithProperties(props);
  SDL_DestroyProperties(props);
  if (!testWindow->window) {
      SDL_Log("Couldn't create window: %s",SDL_GetError());
      return NULL;
  }
  if (state->window_minW || state->window_minH) {
      SDL_SetWindowMinimumSize(testWindow->window, state->window_minW, state->window_minH);
  }
  if (state->window_maxW || state->window_maxH) {
      SDL_SetWindowMaximumSize(testWindow->window, state->window_maxW, state->window_maxH);
  }
  if (state->window_min_aspect != 0.f || state->window_max_aspect != 0.f) {
      SDL_SetWindowAspectRatio(testWindow->window, state->window_min_aspect, state->window_max_aspect);
  }
  if (!SDL_RectEmpty(&state->confine)) {
    SDL_SetWindowMouseRect(testWindow->window, &state->confine);
  }

  testWindow->renderer = SDL_CreateRenderer(testWindow->window, state->renderdriver);
  if (!testWindow->renderer) {
      SDL_Log("Couldn't create renderer: %s", SDL_GetError());
      return NULL;
  }
  if (state->logical_w == 0 || state->logical_h == 0) {
      state->logical_w = state->window_w;
      state->logical_h = state->window_h;
  }
  if (state->render_vsync) {
      SDL_SetRenderVSync(testWindow->renderer, state->render_vsync);
  }
  if (!SDL_SetRenderLogicalPresentation(testWindow->renderer, state->logical_w, state->logical_h, state->logical_presentation)) {
      SDL_Log("Couldn't set logical presentation: %s", SDL_GetError());
      return NULL;
  }
  if (state->scale != 0.0f) {
      SDL_SetRenderScale(testWindow->renderer, state->scale, state->scale);
  }

  testWindow->color.r = SDL_rand(256);
  testWindow->color.g = SDL_rand(256);
  testWindow->color.b = SDL_rand(256);
  testWindow->color.a = 255;

  SDL_ShowWindow(testWindow->window);
  return testWindow;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
  TestState *testState = SDL_calloc(1, sizeof(TestState));
  if (!testState) {
    return SDL_APP_FAILURE;
  }

  testState->state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
  if (!testState->state) {
    return SDL_APP_FAILURE;
  }

  testState->state->num_windows = 0;

  if (!SDLTest_CommonInit(testState->state)) {
    return SDL_APP_FAILURE;
  }

  testState->testWindows[0] = createTestWindowAtMousePosition(testState->state);
  if (!testState->testWindows[0]) {
    return SDL_APP_FAILURE;
  }
  testState->lastCreate = SDL_GetTicks();

  *appstate = testState;
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
  TestState *testState = appstate;

  switch (event->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      return SDL_APP_SUCCESS;
    default:
        break;
  }

  return SDLTest_CommonEventMainCallbacks(testState->state, event);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
  TestState *testState = appstate;
  const Uint64 now = SDL_GetTicks();
  int i;

  for (i = 0; i < MAX_WINDOWS; ++i) {
    if (testState->testWindows[i]) {
      RenderTestWindow(testState->testWindows[i]);
    }
  }

  if (now - testState->lastCreate > 1000) {
    bool added = false;
    for (i = 0; i < MAX_WINDOWS; ++i) {
      if (!testState->testWindows[i]) {
        testState->testWindows[i] = createTestWindowAtMousePosition(testState->state);
        added = true;
        break;
      }
    }
    /* Remove all windows if full */
    if (!added) {
      for (i = 0; i < MAX_WINDOWS; ++i) {
        DestroyTestWindow(testState->testWindows[i]);
        SDL_zero(testState->testWindows[i]);
      }
    }
    testState->lastCreate = now;
  }
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
  TestState *testState = appstate;

  if (testState) {
    DestroyTestState(testState);
  }
}