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

SDL_MenuItem* checkable = NULL;
SDL_MenuItem* new_window = NULL;

SDL_MenuItem *menu_bar;

typedef enum SDL_EventType_MenuExt
{
  MENU_BAR_FILE,
  MENU_BAR_FILE_NEW_WINDOW,
  MENU_BAR_FILE_DISABLE_NEW_WINDOW,
  MENU_BAR_BOOKMARKS,
  MENU_BAR_BOOKMARKS_TOOLBAR,
  MENU_BAR_BOOKMARKS_TOOLBAR_GITHUB,
  MENU_BAR_BOOKMARKS_TOOLBAR_WIKI,
  MENU_BAR_BOOKMARKS_TOOLBAR_DISCORD,
  MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS,
  MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS_STACKOVERFLOW,
  MENU_BAR_INCOGNITO,
  MENU_BAR_TOP_LEVEL_BUTTON,
  MENU_BAR_EXIT,

  MENU_BAR_LAST
} SDL_EventType_MenuExt;

static SDL_EventType_MenuExt EVENT_START = (SDL_EventType_MenuExt)0;

void PrintMenuItems(SDL_MenuItem *menu_item, int indent, int *total_index)
{
    if (!menu_item) {
        return;
    }

    const char* label = SDL_GetMenuItemLabel(menu_item);

    if (!label) {
        label = "no label given";
    }

    SDL_RenderDebugText(renderer, (float)(8 * indent * 2), (float)(*total_index * 8), label);

    ++(*total_index);

    size_t item_count = SDL_GetMenuChildItems(menu_item);

    for (size_t i = 0; i < item_count; ++i) {
        PrintMenuItems(SDL_GetMenuChildItem(menu_item, i), indent + 1, total_index);
    }
}


void CreateMenuBar()
{
    menu_bar = SDL_CreateMenuBar(window);

  {
    SDL_MenuItem* menu = SDL_CreateMenuItem(menu_bar, "File", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
    new_window = SDL_CreateMenuItem(menu, "New Window", SDL_MENUITEM_BUTTON, MENU_BAR_FILE_NEW_WINDOW);
    checkable = SDL_CreateMenuItem(menu, "Enable New Window", SDL_MENUITEM_CHECKABLE, MENU_BAR_FILE_DISABLE_NEW_WINDOW);

    SDL_SetMenuItemChecked(checkable, true);
  }

  {
    SDL_MenuItem* menu = SDL_CreateMenuItem(menu_bar, "Bookmarks", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
    SDL_MenuItem* main_bookmarks = SDL_CreateMenuItem(menu, "Bookmarks Toolbar", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
    SDL_MenuItem* discord = SDL_CreateMenuItem(main_bookmarks, "SDL Discord", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_DISCORD);
    SDL_CreateMenuItem(main_bookmarks, "SDL GitHub", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_GITHUB);
    SDL_CreateMenuItemAt(main_bookmarks, 0, "SDL Wiki", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_WIKI);

    SDL_MenuItem *other_bookmarks = SDL_CreateMenuItem(main_bookmarks, "Other Bookmarks", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
    SDL_MenuItem *stack_overflow = SDL_CreateMenuItem(other_bookmarks, "Stack Overflow-test", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS_STACKOVERFLOW);
    SDL_SetMenuItemLabel(stack_overflow, "Stack Overflow");
      
    SDL_DestroyMenuItem(discord);


    SDL_SetMenuItemChecked(other_bookmarks, false);
  }

  {
    // We can't create a top level checkable .
    SDL_assert(!SDL_CreateMenuItem(menu_bar, "Incognito", SDL_MENUITEM_CHECKABLE, MENU_BAR_INCOGNITO));
      
    SDL_CreateMenuItem(menu_bar, "Exit", SDL_MENUITEM_BUTTON, MENU_BAR_EXIT);
  }

  SDL_SetWindowMenuBar(window, menu_bar);

  EVENT_START = (SDL_EventType_MenuExt)SDL_RegisterEvents(MENU_BAR_LAST);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  SDL_CreateWindowAndRenderer("menu bar test", 640, 480, 0, &window, &renderer);

  CreateMenuBar();

  //return SDL_APP_SUCCESS;
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);

  
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  int total_index = 0;
  PrintMenuItems(menu_bar, 0, &total_index);
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
        switch (event->menu.user_event_type) {
          case MENU_BAR_BOOKMARKS_TOOLBAR_GITHUB:
          {
              SDL_OpenURL("https://github.com/libsdl-org/SDL");
              break;
          }
          case MENU_BAR_BOOKMARKS_TOOLBAR_WIKI:
          {
              SDL_OpenURL("https://wiki.libsdl.org/SDL3/FrontPage");
              break;
          }
          case MENU_BAR_BOOKMARKS_TOOLBAR_DISCORD:
          {
              SDL_OpenURL("https://discord.gg/BwpFGBWsv8");
              break;
          }
          case MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS_STACKOVERFLOW:
          {
              SDL_OpenURL("https://stackoverflow.com/questions");
              break;
          }
          case MENU_BAR_FILE_DISABLE_NEW_WINDOW:
          {
              bool is_checked = false;
              SDL_GetMenuItemChecked(checkable, &is_checked);
              SDL_SetMenuItemChecked(checkable, !is_checked);
              
              bool is_enabled = false;
              SDL_GetMenuItemEnabled(new_window, &is_enabled);
              SDL_SetMenuItemEnabled(new_window, !is_enabled);

              break;
          }
          case MENU_BAR_EXIT:
          {
              return SDL_APP_SUCCESS;
          }
        }
        SDL_Log("%d\n", event->menu.user_event_type);
    }
  }


  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {

}


