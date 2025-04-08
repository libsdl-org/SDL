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

#define VELOCITY 10
#define MAX_BOUNCE 5

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Point velocity;
  SDL_Color color;
  Uint32 bounces;
} TestWindow;

static bool UpdateTestWindow(TestWindow *testWindow, SDL_Rect bounds) {
  SDL_Window *window = testWindow->window;

  int x, y;
  SDL_GetWindowPosition(window, &x, &y);
  int w, h;
  SDL_GetWindowSize(window, &w, &h);

  x += testWindow->velocity.x;
  y += testWindow->velocity.y;

  if (x < bounds.x) {
    testWindow->velocity.x = VELOCITY;
    ++testWindow->bounces;
  } else if (x + w > bounds.x + bounds.w) {
    testWindow->velocity.x = -VELOCITY;
    ++testWindow->bounces;
  }
  x = SDL_clamp(x, bounds.x, bounds.x + bounds.w - w);

  if (y < bounds.y) {
    testWindow->velocity.y = VELOCITY;
    ++testWindow->bounces;
  } else if (y + h > bounds.y + bounds.h) {
    testWindow->velocity.y = -VELOCITY;
    ++testWindow->bounces;
  }
  y = SDL_clamp(y, bounds.y, bounds.y + bounds.h - h);

  SDL_SetWindowPosition(window, x, y);
  return testWindow->bounces < MAX_BOUNCE;
}

static void DestroyTestWindow(TestWindow *testWindow)
{
  SDL_DestroyRenderer(testWindow->renderer);
  SDL_DestroyWindow(testWindow->window);
}

static void RenderTestWindow(TestWindow *testWindow)
{
  SDL_Renderer *renderer = testWindow->renderer;
  SDL_Color color = testWindow->color;

  if (renderer) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
  }
}

#define MAX_WINDOWS 8

typedef struct {
  SDLTest_CommonState *state;
  TestWindow *testWindows[MAX_WINDOWS];
  Uint64 lastCreate;
  Uint64 lastUpdate;
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

  rect.w = SDL_rand(state->window_w);
  rect.w = SDL_clamp(rect.w, 320, rect.w);
  rect.h = SDL_rand(state->window_h);
  rect.h = SDL_clamp(rect.w, 240, rect.h);
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

  testWindow->velocity.x = VELOCITY * (SDL_rand(2) == 0 ? -1 : 1);
  testWindow->velocity.y = VELOCITY * (SDL_rand(2) == 0 ? -1 : 1);

  SDL_ShowWindow(testWindow->window);
  return testWindow;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
#ifdef SDL_PLATFORM_EMSCRIPTEN
  SDL_SetHint(SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS, "1");
#endif

  TestState *testState = SDL_calloc(1, sizeof(TestState));
  if (!testState) {
    return SDL_APP_FAILURE;
  }

  testState->state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
  if (!testState->state) {
    return SDL_APP_FAILURE;
  }

  testState->state->num_windows = 1;

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
      return SDL_APP_SUCCESS;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:{
      SDL_Window *window = SDL_GetWindowFromEvent(event);
      if (window == testState->state->windows[0]) {
        return SDL_APP_SUCCESS;
      }
    }break;
    default:
        break;
  }

  return SDLTest_CommonEventMainCallbacks(testState->state, event);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
  TestState *testState = appstate;
  SDL_Renderer *renderer = testState->state->renderers[0];
  const Uint64 now = SDL_GetTicks();
  int i, textY;

  for (i = 0; i < MAX_WINDOWS; ++i) {
    if (testState->testWindows[i]) {
      RenderTestWindow(testState->testWindows[i]);
    }
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  textY = 0;
  for (i = 0; i < MAX_WINDOWS; ++i) {
    if (!testState->testWindows[i]) {
      continue;
    }
    int x, y;
    SDL_GetWindowPosition(testState->testWindows[i]->window, &x, &y);
    int w, h;
    SDL_GetWindowSize(testState->testWindows[i]->window, &w, &h);
    SDL_RenderDebugTextFormat(renderer, 0, textY, "#%d Window's Position: %d %d; Size: %d %d", i+1, x, y, w, h);
    textY += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
  }
  SDL_RenderPresent(renderer);

  if (now - testState->lastUpdate > 16) {
    SDL_Rect bounds;
    SDL_GetDisplayUsableBounds(testState->state->displayID, &bounds);
    for (i = 0; i < MAX_WINDOWS; ++i) {
      if (testState->testWindows[i] && !UpdateTestWindow(testState->testWindows[i], bounds)) {
        DestroyTestWindow(testState->testWindows[i]);
        testState->testWindows[i] = NULL;
      }
    }
    testState->lastUpdate = now;
  }

  if (now - testState->lastCreate > 1000) {
    for (i = 0; i < MAX_WINDOWS; ++i) {
      if (!testState->testWindows[i]) {
        testState->testWindows[i] = createTestWindowAtMousePosition(testState->state);
        break;
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