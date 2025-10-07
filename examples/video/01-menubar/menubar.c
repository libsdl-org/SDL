/*
 * This example code $WHAT_IT_DOES.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_Window* window_1 = NULL;
SDL_Renderer *renderer_1 = NULL;

SDL_Window *window_2 = NULL;
SDL_Renderer *renderer_2 = NULL;

SDL_MenuItem *checkable[2] = {
    NULL,
    NULL
};

SDL_MenuItem *null_out_button[2] = {
    NULL,
    NULL
};

SDL_MenuItem *menu_bar_1;
SDL_MenuItem *menu_bar_2;

typedef enum SDL_EventType_MenuExt
{
    MENU_BAR_FILE,
    MENU_BAR_FILE_SWAP_BARS,
    MENU_BAR_FILE_NULL_OUT_BAR,
    MENU_BAR_FILE_DISABLE_NULL_OUT_BAR,
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

void PrintMenuItems(SDL_Renderer* renderer, SDL_MenuItem *menu_item, int indent, int *total_index)
{
    if (!menu_item) {
        return;
    }

    SDL_MenuItem *app_menu = SDL_GetMenuBarAppMenu(menu_item);
    if (app_menu) {
        SDL_RenderDebugText(renderer, (float)(8 * indent * 2), (float)(*total_index * 8), " -> AppMenu");
        ++(*total_index);
    }

    const char* label = SDL_GetMenuItemLabel(menu_item);

    if (!label) {
        label = "no label given";
    }

    SDL_RenderDebugText(renderer, (float)(8 * indent * 2), (float)(*total_index * 8), label);

    ++(*total_index);

    size_t item_count = SDL_GetMenuChildItems(menu_item);

    for (size_t i = 0; i < item_count; ++i) {
        PrintMenuItems(renderer, SDL_GetMenuChildItem(menu_item, i), indent + 1, total_index);
    }
}


void CreateMenuBar_1()
{
    menu_bar_1 = SDL_CreateMenuBar();

    {
        SDL_MenuItem *menu = SDL_CreateMenuItem(menu_bar_1, "File_1", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
        SDL_CreateMenuItem(menu, "Swap Bars", SDL_MENUITEM_BUTTON, MENU_BAR_FILE_SWAP_BARS);
        null_out_button[0] = SDL_CreateMenuItem(menu, "Null Out Bar", SDL_MENUITEM_BUTTON, MENU_BAR_FILE_NULL_OUT_BAR);
        SDL_SetMenuItemEnabled(null_out_button[0], false);
        checkable[0] = SDL_CreateMenuItem(menu, "Enable Null Out Bar", SDL_MENUITEM_CHECKABLE, MENU_BAR_FILE_DISABLE_NULL_OUT_BAR);
        SDL_SetMenuItemChecked(checkable[0], false);
    }

    {
        SDL_MenuItem *menu = SDL_CreateMenuItem(menu_bar_1, "Bookmarks_1", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
        SDL_MenuItem *main_bookmarks = SDL_CreateMenuItem(menu, "Bookmarks Toolbar_1", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
        SDL_MenuItem *discord = SDL_CreateMenuItem(main_bookmarks, "SDL Discord_1", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_DISCORD);
        SDL_CreateMenuItem(main_bookmarks, "SDL GitHub_1", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_GITHUB);
        SDL_CreateMenuItemAt(main_bookmarks, 0, "SDL Wiki_1", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_WIKI);

        SDL_MenuItem *other_bookmarks = SDL_CreateMenuItem(main_bookmarks, "Other Bookmarks_1", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
        SDL_MenuItem *stack_overflow = SDL_CreateMenuItem(other_bookmarks, "Stack Overflow-test", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS_STACKOVERFLOW);
        SDL_SetMenuItemLabel(stack_overflow, "Stack Overflow_1");

        SDL_DestroyMenuItem(discord);

        SDL_SetMenuItemChecked(other_bookmarks, false);
    }

    {
        // We can't create a top level checkable .
        SDL_assert(!SDL_CreateMenuItem(menu_bar_1, "Incognito", SDL_MENUITEM_CHECKABLE, MENU_BAR_INCOGNITO));

        SDL_MenuItem* app_menu = SDL_GetMenuBarAppMenu(menu_bar_1);
        if (app_menu) {
            SDL_assert(!SDL_CreateMenuItem(menu_bar_1, "Exit", SDL_MENUITEM_BUTTON, MENU_BAR_EXIT));
            SDL_CreateMenuItem(app_menu, "Exit", SDL_MENUITEM_BUTTON, MENU_BAR_EXIT);
        } else {
            SDL_CreateMenuItem(menu_bar_1, "Exit", SDL_MENUITEM_BUTTON, MENU_BAR_EXIT);
        }
    }

    SDL_SetWindowMenuBar(window_1, menu_bar_1);

    EVENT_START = (SDL_EventType_MenuExt)SDL_RegisterEvents(MENU_BAR_LAST);
}

void CreateMenuBar_2()
{
    menu_bar_2 = SDL_CreateMenuBar();

    {
        SDL_MenuItem *menu = SDL_CreateMenuItem(menu_bar_2, "File_2", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
        SDL_CreateMenuItem(menu, "Swap Bars", SDL_MENUITEM_BUTTON, MENU_BAR_FILE_SWAP_BARS);
        null_out_button[1] = SDL_CreateMenuItem(menu, "Null Out Bar", SDL_MENUITEM_BUTTON, MENU_BAR_FILE_NULL_OUT_BAR);
        SDL_SetMenuItemEnabled(null_out_button[1], false);
        checkable[1] = SDL_CreateMenuItem(menu, "Enable Null Out Bar", SDL_MENUITEM_CHECKABLE, MENU_BAR_FILE_DISABLE_NULL_OUT_BAR);
        SDL_SetMenuItemChecked(checkable[1], false);
    }

    {
        SDL_MenuItem *menu = SDL_CreateMenuItem(menu_bar_2, "Bookmarks_2", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
        SDL_MenuItem *main_bookmarks = SDL_CreateMenuItem(menu, "Bookmarks Toolbar_2", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
        SDL_MenuItem *discord = SDL_CreateMenuItem(main_bookmarks, "SDL Discord_2", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_DISCORD);
        SDL_CreateMenuItem(main_bookmarks, "SDL GitHub_2", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_GITHUB);
        SDL_CreateMenuItemAt(main_bookmarks, 0, "SDL Wiki_2", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_TOOLBAR_WIKI);

        SDL_MenuItem *other_bookmarks = SDL_CreateMenuItem(main_bookmarks, "Other Bookmarks_2", SDL_MENUITEM_SUBMENU, MENU_BAR_LAST);
        SDL_MenuItem *stack_overflow = SDL_CreateMenuItem(other_bookmarks, "Stack Overflow-test_2", SDL_MENUITEM_BUTTON, MENU_BAR_BOOKMARKS_OTHER_BOOKMARKS_STACKOVERFLOW);
        SDL_SetMenuItemLabel(stack_overflow, "Stack Overflow_2");

        SDL_DestroyMenuItem(discord);

        SDL_SetMenuItemChecked(other_bookmarks, false);
    }

    {
        // We can't create a top level checkable .
        SDL_assert(!SDL_CreateMenuItem(menu_bar_2, "Incognito_2", SDL_MENUITEM_CHECKABLE, MENU_BAR_INCOGNITO));

        SDL_MenuItem* app_menu = SDL_GetMenuBarAppMenu(menu_bar_2);
        if (app_menu) {
            SDL_assert(!SDL_CreateMenuItem(menu_bar_2, "Exit", SDL_MENUITEM_BUTTON, MENU_BAR_EXIT));
            SDL_CreateMenuItem(app_menu, "Exit", SDL_MENUITEM_BUTTON, MENU_BAR_EXIT);
        } else {
            SDL_CreateMenuItem(menu_bar_2, "Exit", SDL_MENUITEM_BUTTON, MENU_BAR_EXIT);
        }
    }

    SDL_SetWindowMenuBar(window_2, menu_bar_2);

    EVENT_START = (SDL_EventType_MenuExt)SDL_RegisterEvents(MENU_BAR_LAST);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    SDL_CreateWindowAndRenderer("Window 1", 640, 480, 0, &window_1, &renderer_1);
    SDL_CreateWindowAndRenderer("Window 2", 640, 480, 0, &window_2, &renderer_2);

    CreateMenuBar_1();
    CreateMenuBar_2();

    //return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {

    // Window 1
    SDL_SetRenderDrawColor(renderer_1, 180, 180, 180, 255);
    SDL_RenderClear(renderer_1);
  
    SDL_SetRenderDrawColor(renderer_1, 0, 0, 0, 255);
    int total_index = 0;
    PrintMenuItems(renderer_1, SDL_GetWindowMenuBar(window_1), 0, &total_index);
    SDL_RenderPresent(renderer_1);

    // Window 2
    SDL_SetRenderDrawColor(renderer_2, 255, 255, 255, 255);
    SDL_RenderClear(renderer_2);

    SDL_SetRenderDrawColor(renderer_2, 0, 0, 0, 255);
    total_index = 0;
    PrintMenuItems(renderer_2, SDL_GetWindowMenuBar(window_2), 0, &total_index);
    SDL_RenderPresent(renderer_2);

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
                case MENU_BAR_FILE_DISABLE_NULL_OUT_BAR:
                {
                    bool is_checked = false;
                    SDL_GetMenuItemChecked(checkable[0], &is_checked);
                    SDL_SetMenuItemChecked(checkable[0], !is_checked);
                    SDL_SetMenuItemChecked(checkable[1], !is_checked);
                
                    bool is_enabled = false;
                    SDL_GetMenuItemEnabled(null_out_button[0], &is_enabled);
                    SDL_SetMenuItemEnabled(null_out_button[0], !is_enabled);
                    SDL_SetMenuItemEnabled(null_out_button[1], !is_enabled);
                
                    break;
                }
                case MENU_BAR_FILE_SWAP_BARS:
                {
                    SDL_MenuItem *menu_bar1 = SDL_GetWindowMenuBar(window_1);
                    SDL_MenuItem *menu_bar2 = SDL_GetWindowMenuBar(window_2);
                    SDL_SetWindowMenuBar(window_1, menu_bar2);
                    SDL_SetWindowMenuBar(window_2, menu_bar1);
                    break;
                }
                case MENU_BAR_FILE_NULL_OUT_BAR:
                {
                    SDL_Window *window = SDL_GetWindowFromID(event->menu.windowID);
                    SDL_SetWindowMenuBar(window, NULL);
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


