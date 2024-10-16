#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

void SDLCALL trayprint(void *ptr, SDL_TrayEntry *entry)
{
    SDL_Log("%s\n", (const char *) ptr);
}

void SDLCALL trayhello(void *ptr, SDL_TrayEntry *entry)
{
    SDL_SetTrayEntryChecked(entry, !SDL_GetTrayEntryChecked(entry));
}

void SDLCALL trayexit(void *ptr, SDL_TrayEntry *entry)
{
    SDL_Event e;
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

int main(int argc, char **argv)
{
  SDL_Init(SDL_INIT_VIDEO);

  SDL_Surface *icon = SDL_LoadBMP("../test/trashcan.bmp");

  SDL_Tray *tray = SDL_CreateTray(icon, "Hello!");
  SDL_TrayMenu *menu = SDL_CreateTrayMenu(tray);
  SDL_TrayEntry *entry = SDL_AppendTrayEntry(menu, "Hello!", SDL_TRAYENTRY_CHECKBOX | SDL_TRAYENTRY_CHECKED);
  SDL_TrayEntry *entrysm = SDL_AppendTrayEntry(menu, "Submenu", SDL_TRAYENTRY_SUBMENU);
  /* SDL_TrayEntry *entryd = */ SDL_AppendTrayEntry(menu, "Disabled", SDL_TRAYENTRY_BUTTON | SDL_TRAYENTRY_DISABLED);
  SDL_AppendTraySeparator(menu);
  SDL_TrayEntry *entry2 = SDL_AppendTrayEntry(menu, "Exit", SDL_TRAYENTRY_BUTTON);
  SDL_SetTrayEntryCallback(entry, trayhello, NULL);
  SDL_SetTrayEntryCallback(entry2, trayexit, NULL);

  SDL_TrayMenu *sm = SDL_CreateTraySubmenu(entrysm);
  SDL_TrayEntry *sme1 = SDL_AppendTrayEntry(sm, "Hello", SDL_TRAYENTRY_BUTTON);
  SDL_TrayEntry *sme2 = SDL_AppendTrayEntry(sm, "World", SDL_TRAYENTRY_BUTTON);
  SDL_SetTrayEntryCallback(sme1, trayprint, "Hello,");
  SDL_SetTrayEntryCallback(sme2, trayprint, "world!");

  /* SDL_TrayEntry *smex1 = */ SDL_InsertTrayEntryAt(sm, 1, "Inserted at pos 1 (#1)", SDL_TRAYENTRY_BUTTON);
  /* SDL_TrayEntry *smex2 = */ SDL_InsertTrayEntryAt(sm, 1, "Inserted at pos 1 (#2)", SDL_TRAYENTRY_BUTTON);
  SDL_TrayEntry *smex3 = SDL_InsertTrayEntryAt(sm, 2, "Inserted at pos 2 (#3)", SDL_TRAYENTRY_BUTTON);

  SDL_RemoveTrayEntryAt(sm, 1);
  SDL_RemoveTrayEntry(smex3);

  SDL_DestroySurface(icon);

  SDL_Event e;
  while (SDL_WaitEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT) {
      break;
    }
  }

  SDL_DestroyTray(tray);

  SDL_Quit();
  return 0;
}
