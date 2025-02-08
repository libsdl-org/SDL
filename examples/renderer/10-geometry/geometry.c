/*
 * This example creates an SDL window and renderer, and then draws some
 * geometry (arbitrary polygons) to it every frame.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static int texture_width = 0;
static int texture_height = 0;

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_Surface *surface = NULL;
    char *bmp_path = NULL;

    SDL_SetAppMetadata("Example Renderer Geometry", "1.0", "com.example.renderer-geometry");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/renderer/geometry", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Textures are pixel data that we upload to the video hardware for fast drawing. Lots of 2D
       engines refer to these as "sprites." We'll do a static texture (upload once, draw many
       times) with data from a bitmap file. */

    /* SDL_Surface is pixel data the CPU can access. SDL_Texture is pixel data the GPU can access.
       Load a .bmp into a surface, move it to a texture from there. */
    SDL_asprintf(&bmp_path, "%ssample.bmp", SDL_GetBasePath());  /* allocate a string of the full file path */
    surface = SDL_LoadBMP(bmp_path);
    if (!surface) {
        SDL_Log("Couldn't load bitmap: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_free(bmp_path);  /* done with this, the file is loaded. */

    texture_width = surface->w;
    texture_height = surface->h;

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_Log("Couldn't create static texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_DestroySurface(surface);  /* done with this, the texture has a copy of the pixels now. */

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const Uint64 now = SDL_GetTicks();

    /* we'll have the triangle grow and shrink over a few seconds. */
    const float direction = ((now % 2000) >= 1000) ? 1.0f : -1.0f;
    const float scale = ((float) (((int) (now % 1000)) - 500) / 500.0f) * direction;
    const float size = 200.0f + (200.0f * scale);

    SDL_Vertex vertices[4];
    int i;

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  /* black, full alpha */
    SDL_RenderClear(renderer);  /* start with a blank canvas. */

    /* Draw a single triangle with a different color at each vertex. Center this one and make it grow and shrink. */
    /* You always draw triangles with this, but you can string triangles together to form polygons. */
    SDL_zeroa(vertices);
    vertices[0].position.x = ((float) WINDOW_WIDTH) / 2.0f;
    vertices[0].position.y = (((float) WINDOW_HEIGHT) - size) / 2.0f;
    vertices[0].color.r = 1.0f;
    vertices[0].color.a = 1.0f;
    vertices[1].position.x = (((float) WINDOW_WIDTH) + size) / 2.0f;
    vertices[1].position.y = (((float) WINDOW_HEIGHT) + size) / 2.0f;
    vertices[1].color.g = 1.0f;
    vertices[1].color.a = 1.0f;
    vertices[2].position.x = (((float) WINDOW_WIDTH) - size) / 2.0f;
    vertices[2].position.y = (((float) WINDOW_HEIGHT) + size) / 2.0f;
    vertices[2].color.b = 1.0f;
    vertices[2].color.a = 1.0f;

    SDL_RenderGeometry(renderer, NULL, vertices, 3, NULL, 0);

    /* you can also map a texture to the geometry! Texture coordinates go from 0.0f to 1.0f. That will be the location
       in the texture bound to this vertex. */
    SDL_zeroa(vertices);
    vertices[0].position.x = 10.0f;
    vertices[0].position.y = 10.0f;
    vertices[0].color.r = vertices[0].color.g = vertices[0].color.b = vertices[0].color.a = 1.0f;
    vertices[0].tex_coord.x = 0.0f;
    vertices[0].tex_coord.y = 0.0f;
    vertices[1].position.x = 150.0f;
    vertices[1].position.y = 10.0f;
    vertices[1].color.r = vertices[1].color.g = vertices[1].color.b = vertices[1].color.a = 1.0f;
    vertices[1].tex_coord.x = 1.0f;
    vertices[1].tex_coord.y = 0.0f;
    vertices[2].position.x = 10.0f;
    vertices[2].position.y = 150.0f;
    vertices[2].color.r = vertices[2].color.g = vertices[2].color.b = vertices[2].color.a = 1.0f;
    vertices[2].tex_coord.x = 0.0f;
    vertices[2].tex_coord.y = 1.0f;
    SDL_RenderGeometry(renderer, texture, vertices, 3, NULL, 0);

    /* Did that only draw half of the texture? You can do multiple triangles sharing some vertices,
       using indices, to get the whole thing on the screen: */

    /* Let's just move this over so it doesn't overlap... */
    for (i = 0; i < 3; i++) {
        vertices[i].position.x += 450;
    }

    /* we need one more vertex, since the two triangles can share two of them. */
    vertices[3].position.x = 600.0f;
    vertices[3].position.y = 150.0f;
    vertices[3].color.r = vertices[0].color.g = vertices[0].color.b = vertices[0].color.a = 1.0f;
    vertices[3].tex_coord.x = 1.0f;
    vertices[3].tex_coord.y = 1.0f;

    /* And an index to tell it to reuse some of the vertices between triangles... */
    {
    /* 4 vertices, but 6 actual places they used. Indices need less bandwidth to transfer and can reorder vertices easily! */
    const int indices[] = { 0, 1, 2, 1, 2, 3 };
    SDL_RenderGeometry(renderer, texture, vertices, 4, indices, SDL_arraysize(indices));
    }

    SDL_RenderPresent(renderer);  /* put it all on the screen! */

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_DestroyTexture(texture);
    /* SDL will clean up the window/renderer for us. */
}

