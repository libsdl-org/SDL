/*
 * This example code creates an SDL window and renderer, and waits for the user
 * to click on the window. By default, the window color is blue; When storage
 * succeeds, the window turns green, if it fails the window turns red and the
 * error message is logged. Left clicking will save a game, all other clicks
 * will load a game.
 *
 * The primary goal is to show how to handle save data without blocking the main
 * thread, while also making sure to keep the storage handle open for as little
 * as possible; many platforms do not allow keeping user storage open for long
 * periods of time so you _must_ be sure to only have user storage open when you
 * are absolutely 100% ready to interact with the storage handle.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* This is the list of steps that will occur as part of saving or loading a game */
typedef enum savestate_t
{
    SAVE_STATE_UNSTARTED, /* blue */
    SAVE_STATE_PROCESSING_GAME_WORLD, /* yellow */
    SAVE_STATE_PREPARING_STORAGE, /* cyan */
    SAVE_STATE_PROCESSING_STORAGE_FILE, /* magenta */
    SAVE_STATE_FINAL_CHECK /* green if success, red if failed */
} savestate_t;

SDL_AtomicInt current_save_state;

/* During the final check, this indicates success or failure */
static int save_result = -1;

/* This is the thread that handles the majority of save operations */
static SDL_Thread *save_thread = NULL;

/* Opening storage is itself an async operation, so the thread will have some waiting to do */
static SDL_Semaphore *storage_ready = NULL;

/* This is the handle for the user's filesystem */
static SDL_Storage *save_storage = NULL;

#define SAVE_FILE_NAME "save.sav"

/* This function pretends to serialize a fictional game world, then starts
 * opening the filesystem to write the serialized data
 */
static int SDLCALL WriteSaveData(void *data)
{
    Uint64 game_world; /* to keep things simple, let's just pretend that an entire game fits in 64-bits */
    bool write_result;
    
    SDL_SetAtomicInt(&current_save_state, SAVE_STATE_PROCESSING_GAME_WORLD);

    /* again, let's just pretend that an entire game fits in 64-bits */
    game_world = SDL_GetPerformanceCounter();

    /* now that save data is ready to go, we can start opening the filesystem */
    save_storage = SDL_OpenUserStorage("libsdl", "User Storage Example", 0);
    if (save_storage == NULL) {
        SDL_SetAtomicInt(&current_save_state, SAVE_STATE_FINAL_CHECK);
        return -1;
    }
    SDL_SetAtomicInt(&current_save_state, SAVE_STATE_PREPARING_STORAGE);

    /* the main thread will eventually signal to us that storage is ready */
    SDL_WaitSemaphore(storage_ready);

    /* the save data can now be written to the storage device */
    write_result = SDL_WriteStorageFile(save_storage, SAVE_FILE_NAME, &game_world, sizeof(game_world));

    /* regardless of what happened above, we've reached the end of the routine */
    SDL_CloseStorage(save_storage);
    SDL_SetAtomicInt(&current_save_state, SAVE_STATE_FINAL_CHECK);

    if (!write_result) {
        return -1;
    }
    return 0;
}

/* This function opens the filesystem to read a save file, then deserializes the
 * data into fictional game world data
 */
static int SDLCALL ReadSaveData(void *data)
{
    Uint64 game_world; /* to keep things simple, let's just pretend that an entire game fits in 64-bits */
    Uint64 save_len;
    bool read_result;

    /* start by preparing the filesystem for reading */
    save_storage = SDL_OpenUserStorage("libsdl", "User Storage Example", 0);
    if (save_storage == NULL) {
        SDL_SetAtomicInt(&current_save_state, SAVE_STATE_FINAL_CHECK);
        return -1;
    }
    SDL_SetAtomicInt(&current_save_state, SAVE_STATE_PREPARING_STORAGE);

    /* the main thread will eventually signal to us that storage is ready */
    SDL_WaitSemaphore(storage_ready);

    read_result = SDL_GetStorageFileSize(save_storage, SAVE_FILE_NAME, &save_len);
    if (!read_result) {
        SDL_CloseStorage(save_storage);
        SDL_SetAtomicInt(&current_save_state, SAVE_STATE_FINAL_CHECK);
        SDL_Log("Save data was not found");
        return -1;
    } else if (save_len != sizeof(game_world)) {
        SDL_CloseStorage(save_storage);
        SDL_SetAtomicInt(&current_save_state, SAVE_STATE_FINAL_CHECK);
        SDL_Log("Save data size is incorrect, was the file corrupted?");
        return -1;
    }

    /* once we've read the file in, the storage handle is no longer needed */
    read_result = SDL_ReadStorageFile(save_storage, SAVE_FILE_NAME, &game_world, sizeof(game_world));
    SDL_CloseStorage(save_storage);

    SDL_SetAtomicInt(&current_save_state, SAVE_STATE_PROCESSING_GAME_WORLD);

    if (read_result) {
        /* again, let's just pretend that an entire game fits in 64-bits */
        SDL_Log("Game World loaded, value was %" SDL_PRIu64, game_world);
    }

    /* regardless of what happened above, we've reached the end of the routine */
    SDL_SetAtomicInt(&current_save_state, SAVE_STATE_FINAL_CHECK);

    if (!read_result) {
        return -1;
    }
    return 0;
}

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("User Storage Example", "1.0", "com.example.storage-user");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/storage/user", 640, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* initialize the default save state */
    SDL_SetAtomicInt(&current_save_state, SAVE_STATE_UNSTARTED);
    storage_ready = SDL_CreateSemaphore(0);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (save_thread != NULL) {
            SDL_Log("Ignoring interaction, save/load is in progress");
        } else {
            /* once the thread starts, it will update this to the first "real" state */
            SDL_SetAtomicInt(&current_save_state, SAVE_STATE_UNSTARTED);
            if (event->button.button == 1) {
                save_thread = SDL_CreateThread(WriteSaveData, "Save Write Thread", NULL);
            } else {
                save_thread = SDL_CreateThread(ReadSaveData, "Save Read Thread", NULL);
            }
        }
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    float red, green, blue;
    int save_state = SDL_GetAtomicInt(&current_save_state);

    /* the main thread does not have to do much other than help the thread wait
     * for storage to be ready and read the result when the thread is finished
     */
    if (save_state == SAVE_STATE_PREPARING_STORAGE) {
        if (SDL_StorageReady(save_storage)) {
            SDL_SetAtomicInt(&current_save_state, SAVE_STATE_PROCESSING_STORAGE_FILE);
            SDL_SignalSemaphore(storage_ready);
        }
    } else if (save_state == SAVE_STATE_FINAL_CHECK) {
        if (save_thread != NULL) {
            SDL_WaitThread(save_thread, &save_result);
            save_thread = NULL;
            if (save_result == 0) {
                SDL_Log("Save/Load complete!");
            } else {
                SDL_Log("Save/Load failed: %s", SDL_GetError());
            }
        }
    }

    /* set the draw color based on the state of the save system */
    switch (save_state)
    {
    case SAVE_STATE_UNSTARTED:
        red = 0.0f;
        green = 0.0f;
        blue = 1.0f;
        break;
    case SAVE_STATE_PROCESSING_GAME_WORLD:
        red = 1.0f;
        green = 1.0f;
        blue = 0.0f;
        break;
    case SAVE_STATE_PREPARING_STORAGE:
        red = 0.0f;
        green = 1.0f;
        blue = 1.0f;
        break;
    case SAVE_STATE_PROCESSING_STORAGE_FILE:
        red = 1.0f;
        green = 0.0f;
        blue = 1.0f;
        break;
    case SAVE_STATE_FINAL_CHECK:
        if (save_result == 0) {
            red = 0.0f;
            green = 1.0f;
        } else {
            red = 1.0f;
            green = 0.0f;
        }
        blue = 0.0f;
        break;
    default:
        red = 0.0f;
        green = 0.0f;
        blue = 0.0f;
        SDL_assert(!"Unrecognized save state");
        break;
    }
    SDL_SetRenderDrawColorFloat(renderer, red, green, blue, SDL_ALPHA_OPAQUE_FLOAT);  /* new color, full alpha. */

    /* clear the window to the draw color. */
    SDL_RenderClear(renderer);

    /* put the newly-cleared rendering on the screen. */
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* If saving/loading is still in progress, force the thread not to wait */
    SDL_SignalSemaphore(storage_ready);
    SDL_WaitThread(save_thread, NULL);
    SDL_DestroySemaphore(storage_ready);

    /* SDL will clean up the window/renderer for us. */
}

