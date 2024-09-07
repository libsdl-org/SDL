/*
 * This example code reads frames from a camera and draws it to the screen.
 *
 * This is a very simple approach that is often Good Enough. You can get
 * fancier with this: multiple cameras, front/back facing cameras on phones,
 * color spaces, choosing formats and framerates...this just requests
 * _anything_ and goes with what it is handed.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Camera *camera = NULL;
static SDL_Texture *texture = NULL;


/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_CameraID *devices = NULL;
    int devcount = 0;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/camera/read-and-draw", 640, 480, 0, &window, &renderer)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    devices = SDL_GetCameras(&devcount);
    if (devices == NULL) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't enumerate camera devices!", SDL_GetError(), window);
        return SDL_APP_FAILURE;
    } else if (devcount == 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't find any camera devices!", "Please connect a camera and try again.", window);
        return SDL_APP_FAILURE;
    }

    camera = SDL_OpenCamera(devices[0], NULL);  // just take the first thing we see in any format it wants.
    SDL_free(devices);
    if (camera == NULL) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't open camera!", SDL_GetError(), window);
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_CAMERA_DEVICE_APPROVED) {
        SDL_Log("Camera use approved by user!");
    } else if (event->type == SDL_EVENT_CAMERA_DEVICE_DENIED) {
        SDL_Log("Camera use denied by user!");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Camera permission denied!", "User denied access to the camera!", window);
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    Uint64 timestampNS = 0;
    SDL_Surface *frame = SDL_AcquireCameraFrame(camera, &timestampNS);

    if (frame != NULL) {
        /* Some platforms (like Emscripten) don't know _what_ the camera offers
           until the user gives permission, so we build the texture and resize
           the window when we get a first frame from the camera. */
        if (!texture) {
            SDL_SetWindowSize(window, frame->w, frame->h);  /* Resize the window to match */
            texture = SDL_CreateTexture(renderer, frame->format, SDL_TEXTUREACCESS_STREAMING, frame->w, frame->h);
        }

        if (texture) {
            SDL_UpdateTexture(texture, NULL, frame->pixels, frame->pitch);
        }

        SDL_ReleaseCameraFrame(camera, frame);
    }

    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, 255);
    SDL_RenderClear(renderer);
    if (texture) {  /* draw the latest camera frame, if available. */
        SDL_RenderTexture(renderer, texture, NULL, NULL);
    }
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate)
{
    SDL_CloseCamera(camera);
    SDL_DestroyTexture(texture);
    /* SDL will clean up the window/renderer for us. */
}

