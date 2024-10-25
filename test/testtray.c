#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static void SDLCALL tray_quit(void *ptr, SDL_TrayEntry *entry)
{
    SDL_Event e;
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

static void SDLCALL apply_icon(void *ptr, const char * const *filelist, int filter)
{
    if (!*filelist) {
        return;
    }

    SDL_Surface *icon = SDL_LoadBMP(*filelist);

    if (!icon) {
        SDL_Log("Couldn't load icon '%s': %s", *filelist, SDL_GetError());
        return;
    }

    SDL_Tray *tray = (SDL_Tray *) ptr;
    SDL_SetTrayIcon(tray, icon);

    SDL_DestroySurface(icon);
}

static void SDLCALL change_icon(void *ptr, SDL_TrayEntry *entry)
{
    SDL_DialogFileFilter filters[] = {
        { "BMP image files", "bmp" },
        { "All files", "*" },
    };

    SDL_ShowOpenFileDialog(apply_icon, ptr, NULL, filters, 2, NULL, 0);
}

static void SDLCALL print_entry(void *ptr, SDL_TrayEntry *entry)
{
    SDL_Log("Clicked on button '%s'\n", SDL_GetTrayEntryLabel(entry));
}

static void SDLCALL set_entry_enabled(void *ptr, SDL_TrayEntry *entry)
{
    SDL_TrayEntry *target = (SDL_TrayEntry *) ptr;
    SDL_SetTrayEntryEnabled(target, true);
}

static void SDLCALL set_entry_disabled(void *ptr, SDL_TrayEntry *entry)
{
    SDL_TrayEntry *target = (SDL_TrayEntry *) ptr;
    SDL_SetTrayEntryEnabled(target, false);
}

static void SDLCALL set_entry_checked(void *ptr, SDL_TrayEntry *entry)
{
    SDL_TrayEntry *target = (SDL_TrayEntry *) ptr;
    SDL_SetTrayEntryChecked(target, true);
}

static void SDLCALL set_entry_unchecked(void *ptr, SDL_TrayEntry *entry)
{
    SDL_TrayEntry *target = (SDL_TrayEntry *) ptr;
    SDL_SetTrayEntryChecked(target, false);
}

static void SDLCALL remove_entry(void *ptr, SDL_TrayEntry *entry)
{
    SDL_TrayEntry *target = (SDL_TrayEntry *) ptr;
    SDL_RemoveTrayEntry(target);

    SDL_TrayMenu *ctrl_submenu = SDL_GetTrayEntryParent(entry);
    SDL_TrayEntry *ctrl_entry = SDL_GetTrayMenuParentEntry(ctrl_submenu);

    if (!ctrl_entry) {
        SDL_Log("Attempt to remove a menu that isn't a submenu. This shouldn't happen.\n");
        return;
    }

    SDL_RemoveTrayEntry(ctrl_entry);
}

static void SDLCALL append_button_to(void *ptr, SDL_TrayEntry *entry)
{
    SDL_TrayMenu *menu = (SDL_TrayMenu *) ptr;
    SDL_TrayMenu *submenu;
    SDL_TrayEntry *new_ctrl;
    SDL_TrayEntry *new_ctrl_remove;
    SDL_TrayEntry *new_ctrl_enabled;
    SDL_TrayEntry *new_ctrl_disabled;
    SDL_TrayEntry *new_example;

    new_ctrl = SDL_InsertTrayEntryAt(SDL_GetTrayEntryParent(entry), -1, "New button", SDL_TRAYENTRY_SUBMENU);

    if (!new_ctrl) {
        SDL_Log("Couldn't insert entry in control tray: %s\n", SDL_GetError());
        return;
    }

    /* ---------- */

    submenu = SDL_CreateTraySubmenu(new_ctrl);

    if (!new_ctrl) {
        SDL_Log("Couldn't create control tray entry submenu: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        return;
    }

    /* ---------- */

    new_example = SDL_InsertTrayEntryAt(menu, -1, "New button", SDL_TRAYENTRY_BUTTON);

    if (new_example == NULL) {
        SDL_Log("Couldn't insert entry in example tray: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        return;
    }

    SDL_SetTrayEntryCallback(new_example, print_entry, NULL);

    /* ---------- */

    new_ctrl_remove = SDL_InsertTrayEntryAt(submenu, -1, "Remove", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_remove == NULL) {
        SDL_Log("Couldn't insert new_ctrl_remove: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_remove, remove_entry, new_example);

    /* ---------- */

    new_ctrl_enabled = SDL_InsertTrayEntryAt(submenu, -1, "Enable", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_enabled == NULL) {
        SDL_Log("Couldn't insert new_ctrl_enabled: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_enabled, set_entry_enabled, new_example);

    /* ---------- */

    new_ctrl_disabled = SDL_InsertTrayEntryAt(submenu, -1, "Disable", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_disabled == NULL) {
        SDL_Log("Couldn't insert new_ctrl_disabled: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_disabled, set_entry_disabled, new_example);
}

static void SDLCALL append_checkbox_to(void *ptr, SDL_TrayEntry *entry)
{
    SDL_TrayMenu *menu = (SDL_TrayMenu *) ptr;
    SDL_TrayMenu *submenu;
    SDL_TrayEntry *new_ctrl;
    SDL_TrayEntry *new_ctrl_remove;
    SDL_TrayEntry *new_ctrl_enabled;
    SDL_TrayEntry *new_ctrl_disabled;
    SDL_TrayEntry *new_ctrl_checked;
    SDL_TrayEntry *new_ctrl_unchecked;
    SDL_TrayEntry *new_example;

    new_ctrl = SDL_InsertTrayEntryAt(SDL_GetTrayEntryParent(entry), -1, "New checkbox", SDL_TRAYENTRY_SUBMENU);

    if (!new_ctrl) {
        SDL_Log("Couldn't insert entry in control tray: %s\n", SDL_GetError());
        return;
    }

    /* ---------- */

    submenu = SDL_CreateTraySubmenu(new_ctrl);

    if (!new_ctrl) {
        SDL_Log("Couldn't create control tray entry submenu: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        return;
    }

    /* ---------- */

    new_example = SDL_InsertTrayEntryAt(menu, -1, "New checkbox", SDL_TRAYENTRY_CHECKBOX);

    if (new_example == NULL) {
        SDL_Log("Couldn't insert entry in example tray: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        return;
    }

    SDL_SetTrayEntryCallback(new_example, print_entry, NULL);

    /* ---------- */

    new_ctrl_remove = SDL_InsertTrayEntryAt(submenu, -1, "Remove", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_remove == NULL) {
        SDL_Log("Couldn't insert new_ctrl_remove: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_remove, remove_entry, new_example);

    /* ---------- */

    new_ctrl_enabled = SDL_InsertTrayEntryAt(submenu, -1, "Enable", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_enabled == NULL) {
        SDL_Log("Couldn't insert new_ctrl_enabled: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_enabled, set_entry_enabled, new_example);

    /* ---------- */

    new_ctrl_disabled = SDL_InsertTrayEntryAt(submenu, -1, "Disable", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_disabled == NULL) {
        SDL_Log("Couldn't insert new_ctrl_disabled: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_disabled, set_entry_disabled, new_example);

    /* ---------- */

    new_ctrl_checked = SDL_InsertTrayEntryAt(submenu, -1, "Check", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_checked == NULL) {
        SDL_Log("Couldn't insert new_ctrl_checked: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_checked, set_entry_checked, new_example);

    /* ---------- */

    new_ctrl_unchecked = SDL_InsertTrayEntryAt(submenu, -1, "Uncheck", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_unchecked == NULL) {
        SDL_Log("Couldn't insert new_ctrl_unchecked: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_unchecked, set_entry_unchecked, new_example);
}

static void SDLCALL append_submenu_to(void *ptr, SDL_TrayEntry *entry)
{
    SDL_TrayMenu *menu = (SDL_TrayMenu *) ptr;
    SDL_TrayMenu *submenu;
    SDL_TrayMenu *entry_submenu;
    SDL_TrayEntry *new_ctrl;
    SDL_TrayEntry *new_ctrl_remove;
    SDL_TrayEntry *new_ctrl_enabled;
    SDL_TrayEntry *new_ctrl_disabled;
    SDL_TrayEntry *new_example;

    new_ctrl = SDL_InsertTrayEntryAt(SDL_GetTrayEntryParent(entry), -1, "New submenu", SDL_TRAYENTRY_SUBMENU);

    if (!new_ctrl) {
        SDL_Log("Couldn't insert entry in control tray: %s\n", SDL_GetError());
        return;
    }

    /* ---------- */

    submenu = SDL_CreateTraySubmenu(new_ctrl);

    if (!new_ctrl) {
        SDL_Log("Couldn't create control tray entry submenu: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        return;
    }

    /* ---------- */

    new_example = SDL_InsertTrayEntryAt(menu, -1, "New submenu", SDL_TRAYENTRY_SUBMENU);

    if (new_example == NULL) {
        SDL_Log("Couldn't insert entry in example tray: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        return;
    }

    SDL_SetTrayEntryCallback(new_example, print_entry, NULL);

    /* ---------- */

    entry_submenu = SDL_CreateTraySubmenu(new_example);

    if (entry_submenu == NULL) {
        SDL_Log("Couldn't create new entry submenu: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    /* ---------- */

    new_ctrl_remove = SDL_InsertTrayEntryAt(submenu, -1, "Remove", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_remove == NULL) {
        SDL_Log("Couldn't insert new_ctrl_remove: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_remove, remove_entry, new_example);

    /* ---------- */

    new_ctrl_enabled = SDL_InsertTrayEntryAt(submenu, -1, "Enable", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_enabled == NULL) {
        SDL_Log("Couldn't insert new_ctrl_enabled: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_enabled, set_entry_enabled, new_example);

    /* ---------- */

    new_ctrl_disabled = SDL_InsertTrayEntryAt(submenu, -1, "Disable", SDL_TRAYENTRY_BUTTON);

    if (new_ctrl_disabled == NULL) {
        SDL_Log("Couldn't insert new_ctrl_disabled: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(new_ctrl_disabled, set_entry_disabled, new_example);

    /* ---------- */

    SDL_InsertTrayEntryAt(submenu, -1, NULL, 0);

    /* ---------- */

    SDL_TrayEntry *entry_newbtn = SDL_InsertTrayEntryAt(submenu, -1, "Create button", SDL_TRAYENTRY_BUTTON);

    if (entry_newbtn == NULL) {
        SDL_Log("Couldn't insert entry_newbtn: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(entry_newbtn, append_button_to, entry_submenu);

    /* ---------- */

    SDL_TrayEntry *entry_newchk = SDL_InsertTrayEntryAt(submenu, -1, "Create checkbox", SDL_TRAYENTRY_BUTTON);

    if (entry_newchk == NULL) {
        SDL_Log("Couldn't insert entry_newchk: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(entry_newchk, append_checkbox_to, entry_submenu);

    /* ---------- */

    SDL_TrayEntry *entry_newsub = SDL_InsertTrayEntryAt(submenu, -1, "Create submenu", SDL_TRAYENTRY_BUTTON);

    if (entry_newsub == NULL) {
        SDL_Log("Couldn't insert entry_newsub: %s\n", SDL_GetError());
        SDL_RemoveTrayEntry(new_ctrl);
        SDL_RemoveTrayEntry(new_example);
        return;
    }

    SDL_SetTrayEntryCallback(entry_newsub, append_submenu_to, entry_submenu);

    /* ---------- */

    SDL_InsertTrayEntryAt(submenu, -1, NULL, 0);
}

int main(int argc, char **argv)
{
    SDLTest_CommonState *state;
    int i;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);

        if (consumed <= 0) {
            static const char *options[] = { NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    /* TODO: Resource paths? */
    SDL_Surface *icon = SDL_LoadBMP("../test/sdl-test_round.bmp");
    SDL_Surface *icon2 = SDL_LoadBMP("../test/speaker.bmp");

    SDL_Tray *tray = SDL_CreateTray(icon, "SDL Tray control menu");
    SDL_Tray *tray2 = SDL_CreateTray(icon2, "SDL Tray example");

    SDL_DestroySurface(icon);
    SDL_DestroySurface(icon2);

    SDL_TrayMenu *menu = SDL_CreateTrayMenu(tray);
    SDL_TrayMenu *menu2 = SDL_CreateTrayMenu(tray2);

    SDL_TrayEntry *entry_quit = SDL_InsertTrayEntryAt(menu, -1, "Quit", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(entry_quit, tray_quit, NULL);

    SDL_InsertTrayEntryAt(menu, -1, NULL, 0);

    SDL_TrayEntry *entry_icon = SDL_InsertTrayEntryAt(menu, -1, "Change icon", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(entry_icon, change_icon, tray2);

    SDL_InsertTrayEntryAt(menu, -1, NULL, 0);

    SDL_TrayEntry *entry_newbtn = SDL_InsertTrayEntryAt(menu, -1, "Create button", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(entry_newbtn, append_button_to, menu2);
    SDL_TrayEntry *entry_newchk = SDL_InsertTrayEntryAt(menu, -1, "Create checkbox", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(entry_newchk, append_checkbox_to, menu2);
    SDL_TrayEntry *entry_newsub = SDL_InsertTrayEntryAt(menu, -1, "Create submenu", SDL_TRAYENTRY_BUTTON);
    SDL_SetTrayEntryCallback(entry_newsub, append_submenu_to, menu2);

    SDL_InsertTrayEntryAt(menu, -1, NULL, 0);

    SDL_Event e;
    while (SDL_WaitEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            break;
        }
    }

    SDL_DestroyTray(tray);
    SDL_DestroyTray(tray2);

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
