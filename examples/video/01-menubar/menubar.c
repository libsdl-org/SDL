/*
 * This example code $WHAT_IT_DOES.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;


typedef enum SDL_EventType_MenuExt
{
  MENU_BAR_FILE,
  MENU_BAR_FILE_NEW_WINDOW,
  MENU_BAR_FILE_AUTOSAVE_TABS_ON_CLOSE,
  MENU_BAR_BOOKMARKS,
  MENU_BAR_BOOKMARKS_TOOLBAR,
  MENU_BAR_BOOKMARKS_TOOLBAR_GITHUB,
  MENU_BAR_BOOKMARKS_TOOLBAR_WIKI,
  MENU_BAR_BOOKMARKS_TOOLBAR_DISCORD,
  MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS,
  MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS_TWITTER,
  MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS_REDDIT,
  MENU_BAR_INCOGNITO,
  MENU_BAR_TOP_LEVEL_BUTTON,
  MENU_BAR_EXIT,

  MENU_BAR_LAST
} SDL_EventType_MenuExt;

static SDL_EventType_MenuExt EVENT_START = (SDL_EventType_MenuExt)0;

void CreateMenuBar()
{
  SDL_MenuBar* bar = SDL_CreateMenuBar(window);

  {
    SDL_MenuItem* menu = SDL_CreateMenuBarItem(bar, "File", SDL_MENU, MENU_BAR_LAST);
    SDL_CreateMenuItem((SDL_Menu*)menu, "New Window", SDL_MENU_BUTTON, MENU_BAR_FILE_NEW_WINDOW);
    SDL_MenuItem* checkable = SDL_CreateMenuItem((SDL_Menu*)menu, "Autosave Tabs on Close", SDL_MENU_CHECKABLE, MENU_BAR_FILE_AUTOSAVE_TABS_ON_CLOSE);

    SDL_CheckMenuItem(checkable, true);
  }

  {
    SDL_MenuItem* menu = SDL_CreateMenuBarItem(bar, "Bookmarks", SDL_MENU, MENU_BAR_LAST);
    SDL_MenuItem* main_bookmarks = SDL_CreateMenuItem((SDL_Menu*)menu, "Bookmarks Toolbar", SDL_MENU, MENU_BAR_LAST);
    SDL_CreateMenuItem((SDL_Menu*)main_bookmarks, "SDL GitHub", SDL_MENU_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_GITHUB);
    SDL_CreateMenuItem((SDL_Menu*)main_bookmarks, "SDL Wiki", SDL_MENU_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_WIKI);
    SDL_CreateMenuItem((SDL_Menu*)main_bookmarks, "SDL Discord", SDL_MENU_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_DISCORD);

    SDL_MenuItem* other_bookmarks = SDL_CreateMenuItem((SDL_Menu*)menu, "Other Bookmarks", SDL_MENU, MENU_BAR_LAST);
    SDL_CreateMenuItem((SDL_Menu*)other_bookmarks, "Stack Overflow", SDL_MENU_BUTTON, MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS);

    SDL_EnableMenuItem(other_bookmarks, false);
  }

  {
    // We can't create a top level checkable .
    SDL_MenuItem* checkable = SDL_CreateMenuBarItem(bar, "Incognito", SDL_MENU_CHECKABLE, MENU_BAR_INCOGNITO);
    
    SDL_MenuItem* disabled = SDL_CreateMenuBarItem(bar, "Disabled Top-Level Button", SDL_MENU_BUTTON, MENU_BAR_TOP_LEVEL_BUTTON);
    SDL_EnableMenuItem(disabled, false);

    SDL_CreateMenuBarItem(bar, "Exit", SDL_MENU_BUTTON, MENU_BAR_EXIT);
  }

  EVENT_START = (SDL_EventType_MenuExt)SDL_RegisterEvents(MENU_BAR_LAST);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  SDL_CreateWindowAndRenderer("menu bar test", 640, 480, 0, &window, &renderer);

  CreateMenuBar();

  //return SDL_APP_SUCCESS;
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {

  SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {

  switch (event->common.type)
  {
    case SDL_EVENT_QUIT:
    {
      return SDL_APP_SUCCESS;
    }
    case SDL_EVENT_MENU_BUTTON_CLICKED:
    case SDL_EVENT_MENU_CHECKABLE_CLICKED:
    {
        SDL_Log("%d\n", event->menu.user_event_type);
    }
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {

}


