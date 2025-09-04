/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_EMSCRIPTEN) || defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_LINUX)
#define HAVE_OPENGLES2
#endif

#ifdef HAVE_OPENGLES2

#include <SDL3/SDL_opengles2.h>

typedef struct GLES2_Context
{
#define SDL_PROC(ret, func, params) ret (APIENTRY *func) params;
#include "../src/render/opengles2/SDL_gles2funcs.h"
#undef SDL_PROC
} GLES2_Context;

typedef struct shader_data
{
    GLuint shader_program, shader_frag, shader_vert;

    GLint attr_position;
    GLint attr_color, attr_mvp;

    int angle_x, angle_y, angle_z;

    GLuint position_buffer;
    GLuint color_buffer;
} shader_data;

typedef enum wait_state
{
    WAIT_STATE_GO = 0,
    WAIT_STATE_ENTER_SEM,
    WAIT_STATE_WAITING_ON_SEM,
} wait_state;

typedef struct thread_data
{
    SDL_Thread *thread;
    SDL_Semaphore *suspend_sem;
    SDL_AtomicInt suspended;
    int done;
    int index;
} thread_data;

static SDLTest_CommonState *state;
static SDL_GLContext *context = NULL;
static int depth = 16;
static bool suspend_when_occluded;
static GLES2_Context ctx;
static shader_data *datas;

static bool LoadContext(GLES2_Context *data)
{
#ifdef SDL_VIDEO_DRIVER_UIKIT
#define __SDL_NOGETPROCADDR__
#elif defined(SDL_VIDEO_DRIVER_ANDROID)
#define __SDL_NOGETPROCADDR__
#endif

#if defined __SDL_NOGETPROCADDR__
#define SDL_PROC(ret, func, params) data->func = func;
#else
#define SDL_PROC(ret, func, params)                                                            \
    do {                                                                                       \
        data->func = (ret (APIENTRY *) params)SDL_GL_GetProcAddress(#func);                    \
        if (!data->func) {                                                                     \
            return SDL_SetError("Couldn't load GLES2 function %s: %s", #func, SDL_GetError()); \
        }                                                                                      \
    } while (0);
#endif /* __SDL_NOGETPROCADDR__ */

#include "../src/render/opengles2/SDL_gles2funcs.h"
#undef SDL_PROC
    return true;
}

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    int i;

    SDL_free(datas);
    if (context) {
        for (i = 0; i < state->num_windows; i++) {
            if (context[i]) {
                SDL_GL_DestroyContext(context[i]);
            }
        }

        SDL_free(context);
    }

    SDLTest_CommonQuit(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

#define GL_CHECK(x)                                                                         \
    x;                                                                                      \
    {                                                                                       \
        GLenum glError = ctx.glGetError();                                                  \
        if (glError != GL_NO_ERROR) {                                                       \
            SDL_Log("glGetError() = %i (0x%.8x) at line %i", glError, glError, __LINE__); \
            quit(1);                                                                        \
        }                                                                                   \
    }

/**
 * Simulates desktop's glRotatef. The matrix is returned in column-major
 * order.
 */
static void
rotate_matrix(float angle, float x, float y, float z, float *r)
{
    float radians, c, s, c1, u[3], length;
    int i, j;

    radians = (angle * SDL_PI_F) / 180.0f;

    c = SDL_cosf(radians);
    s = SDL_sinf(radians);

    c1 = 1.0f - SDL_cosf(radians);

    length = (float)SDL_sqrt(x * x + y * y + z * z);

    u[0] = x / length;
    u[1] = y / length;
    u[2] = z / length;

    for (i = 0; i < 16; i++) {
        r[i] = 0.0;
    }

    r[15] = 1.0;

    for (i = 0; i < 3; i++) {
        r[i * 4 + (i + 1) % 3] = u[(i + 2) % 3] * s;
        r[i * 4 + (i + 2) % 3] = -u[(i + 1) % 3] * s;
    }

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r[i * 4 + j] += c1 * u[i] * u[j] + (i == j ? c : 0.0f);
        }
    }
}

/**
 * Simulates gluPerspectiveMatrix
 */
static void
perspective_matrix(float fovy, float aspect, float znear, float zfar, float *r)
{
    int i;
    float f;

    f = 1.0f / SDL_tanf((fovy / 180.0f) * SDL_PI_F * 0.5f);

    for (i = 0; i < 16; i++) {
        r[i] = 0.0;
    }

    r[0] = f / aspect;
    r[5] = f;
    r[10] = (znear + zfar) / (znear - zfar);
    r[11] = -1.0f;
    r[14] = (2.0f * znear * zfar) / (znear - zfar);
    r[15] = 0.0f;
}

/**
 * Multiplies lhs by rhs and writes out to r. All matrices are 4x4 and column
 * major. In-place multiplication is supported.
 */
static void
multiply_matrix(const float *lhs, const float *rhs, float *r)
{
    int i, j, k;
    float tmp[16];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            tmp[j * 4 + i] = 0.0;

            for (k = 0; k < 4; k++) {
                tmp[j * 4 + i] += lhs[k * 4 + i] * rhs[j * 4 + k];
            }
        }
    }

    for (i = 0; i < 16; i++) {
        r[i] = tmp[i];
    }
}

/**
 * Create shader, load in source, compile, dump debug as necessary.
 *
 * shader: Pointer to return created shader ID.
 * source: Passed-in shader source code.
 * shader_type: Passed to GL, e.g. GL_VERTEX_SHADER.
 */
static void
process_shader(GLuint *shader, const char *source, GLint shader_type)
{
    GLint status = GL_FALSE;
    const char *shaders[1] = { NULL };
    char buffer[1024];
    GLsizei length = 0;

    /* Create shader and load into GL. */
    *shader = GL_CHECK(ctx.glCreateShader(shader_type));

    shaders[0] = source;

    GL_CHECK(ctx.glShaderSource(*shader, 1, shaders, NULL));

    /* Clean up shader source. */
    shaders[0] = NULL;

    /* Try compiling the shader. */
    GL_CHECK(ctx.glCompileShader(*shader));
    GL_CHECK(ctx.glGetShaderiv(*shader, GL_COMPILE_STATUS, &status));

    /* Dump debug info (source and log) if compilation failed. */
    if (status != GL_TRUE) {
        ctx.glGetShaderInfoLog(*shader, sizeof(buffer), &length, &buffer[0]);
        buffer[length] = '\0';
        SDL_Log("Shader compilation failed: %s", buffer);
        quit(-1);
    }
}

static void
link_program(struct shader_data *data)
{
    GLint status = GL_FALSE;
    char buffer[1024];
    GLsizei length = 0;

    GL_CHECK(ctx.glAttachShader(data->shader_program, data->shader_vert));
    GL_CHECK(ctx.glAttachShader(data->shader_program, data->shader_frag));
    GL_CHECK(ctx.glLinkProgram(data->shader_program));
    GL_CHECK(ctx.glGetProgramiv(data->shader_program, GL_LINK_STATUS, &status));

    if (status != GL_TRUE) {
        ctx.glGetProgramInfoLog(data->shader_program, sizeof(buffer), &length, &buffer[0]);
        buffer[length] = '\0';
        SDL_Log("Program linking failed: %s", buffer);
        quit(-1);
    }
}

/* 3D data. Vertex range -0.5..0.5 in all axes.
 * Z -0.5 is near, 0.5 is far. */
static const float g_vertices[] = {
    /* Front face. */
    /* Bottom left */
    -0.5,
    0.5,
    -0.5,
    0.5,
    -0.5,
    -0.5,
    -0.5,
    -0.5,
    -0.5,
    /* Top right */
    -0.5,
    0.5,
    -0.5,
    0.5,
    0.5,
    -0.5,
    0.5,
    -0.5,
    -0.5,
    /* Left face */
    /* Bottom left */
    -0.5,
    0.5,
    0.5,
    -0.5,
    -0.5,
    -0.5,
    -0.5,
    -0.5,
    0.5,
    /* Top right */
    -0.5,
    0.5,
    0.5,
    -0.5,
    0.5,
    -0.5,
    -0.5,
    -0.5,
    -0.5,
    /* Top face */
    /* Bottom left */
    -0.5,
    0.5,
    0.5,
    0.5,
    0.5,
    -0.5,
    -0.5,
    0.5,
    -0.5,
    /* Top right */
    -0.5,
    0.5,
    0.5,
    0.5,
    0.5,
    0.5,
    0.5,
    0.5,
    -0.5,
    /* Right face */
    /* Bottom left */
    0.5,
    0.5,
    -0.5,
    0.5,
    -0.5,
    0.5,
    0.5,
    -0.5,
    -0.5,
    /* Top right */
    0.5,
    0.5,
    -0.5,
    0.5,
    0.5,
    0.5,
    0.5,
    -0.5,
    0.5,
    /* Back face */
    /* Bottom left */
    0.5,
    0.5,
    0.5,
    -0.5,
    -0.5,
    0.5,
    0.5,
    -0.5,
    0.5,
    /* Top right */
    0.5,
    0.5,
    0.5,
    -0.5,
    0.5,
    0.5,
    -0.5,
    -0.5,
    0.5,
    /* Bottom face */
    /* Bottom left */
    -0.5,
    -0.5,
    -0.5,
    0.5,
    -0.5,
    0.5,
    -0.5,
    -0.5,
    0.5,
    /* Top right */
    -0.5,
    -0.5,
    -0.5,
    0.5,
    -0.5,
    -0.5,
    0.5,
    -0.5,
    0.5,
};

static const float g_colors[] = {
    /* Front face */
    /* Bottom left */
    1.0, 0.0, 0.0, /* red */
    0.0, 0.0, 1.0, /* blue */
    0.0, 1.0, 0.0, /* green */
    /* Top right */
    1.0, 0.0, 0.0, /* red */
    1.0, 1.0, 0.0, /* yellow */
    0.0, 0.0, 1.0, /* blue */
    /* Left face */
    /* Bottom left */
    1.0, 1.0, 1.0, /* white */
    0.0, 1.0, 0.0, /* green */
    0.0, 1.0, 1.0, /* cyan */
    /* Top right */
    1.0, 1.0, 1.0, /* white */
    1.0, 0.0, 0.0, /* red */
    0.0, 1.0, 0.0, /* green */
    /* Top face */
    /* Bottom left */
    1.0, 1.0, 1.0, /* white */
    1.0, 1.0, 0.0, /* yellow */
    1.0, 0.0, 0.0, /* red */
    /* Top right */
    1.0, 1.0, 1.0, /* white */
    0.0, 0.0, 0.0, /* black */
    1.0, 1.0, 0.0, /* yellow */
    /* Right face */
    /* Bottom left */
    1.0, 1.0, 0.0, /* yellow */
    1.0, 0.0, 1.0, /* magenta */
    0.0, 0.0, 1.0, /* blue */
    /* Top right */
    1.0, 1.0, 0.0, /* yellow */
    0.0, 0.0, 0.0, /* black */
    1.0, 0.0, 1.0, /* magenta */
    /* Back face */
    /* Bottom left */
    0.0, 0.0, 0.0, /* black */
    0.0, 1.0, 1.0, /* cyan */
    1.0, 0.0, 1.0, /* magenta */
    /* Top right */
    0.0, 0.0, 0.0, /* black */
    1.0, 1.0, 1.0, /* white */
    0.0, 1.0, 1.0, /* cyan */
    /* Bottom face */
    /* Bottom left */
    0.0, 1.0, 0.0, /* green */
    1.0, 0.0, 1.0, /* magenta */
    0.0, 1.0, 1.0, /* cyan */
    /* Top right */
    0.0, 1.0, 0.0, /* green */
    0.0, 0.0, 1.0, /* blue */
    1.0, 0.0, 1.0, /* magenta */
};

static const char *g_shader_vert_src =
    " attribute vec4 av4position; "
    " attribute vec3 av3color; "
    " uniform mat4 mvp; "
    " varying vec3 vv3color; "
    " void main() { "
    "    vv3color = av3color; "
    "    gl_Position = mvp * av4position; "
    " } ";

static const char *g_shader_frag_src =
    " precision lowp float; "
    " varying vec3 vv3color; "
    " void main() { "
    "    gl_FragColor = vec4(vv3color, 1.0); "
    " } ";

static void
Render(unsigned int width, unsigned int height, shader_data *data)
{
    float matrix_rotate[16], matrix_modelview[16], matrix_perspective[16], matrix_mvp[16];

    /*
     * Do some rotation with Euler angles. It is not a fixed axis as
     * quaterions would be, but the effect is cool.
     */
    rotate_matrix((float)data->angle_x, 1.0f, 0.0f, 0.0f, matrix_modelview);
    rotate_matrix((float)data->angle_y, 0.0f, 1.0f, 0.0f, matrix_rotate);

    multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

    rotate_matrix((float)data->angle_z, 0.0f, 1.0f, 0.0f, matrix_rotate);

    multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

    /* Pull the camera back from the cube */
    matrix_modelview[14] -= 2.5f;

    perspective_matrix(45.0f, (float)width / height, 0.01f, 100.0f, matrix_perspective);
    multiply_matrix(matrix_perspective, matrix_modelview, matrix_mvp);

    GL_CHECK(ctx.glUniformMatrix4fv(data->attr_mvp, 1, GL_FALSE, matrix_mvp));

    data->angle_x += 3;
    data->angle_y += 2;
    data->angle_z += 1;

    if (data->angle_x >= 360) {
        data->angle_x -= 360;
    }
    if (data->angle_x < 0) {
        data->angle_x += 360;
    }
    if (data->angle_y >= 360) {
        data->angle_y -= 360;
    }
    if (data->angle_y < 0) {
        data->angle_y += 360;
    }
    if (data->angle_z >= 360) {
        data->angle_z -= 360;
    }
    if (data->angle_z < 0) {
        data->angle_z += 360;
    }

    GL_CHECK(ctx.glViewport(0, 0, width, height));
    GL_CHECK(ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    GL_CHECK(ctx.glDrawArrays(GL_TRIANGLES, 0, 36));
}

static int done;
static Uint32 frames;
#ifndef SDL_PLATFORM_EMSCRIPTEN
static thread_data *threads;
#endif

static void
render_window(int index)
{
    int w, h;

    if (!state->windows[index]) {
        return;
    }

    if (!SDL_GL_MakeCurrent(state->windows[index], context[index])) {
        SDL_Log("SDL_GL_MakeCurrent(): %s", SDL_GetError());
        return;
    }

    SDL_GetWindowSizeInPixels(state->windows[index], &w, &h);
    Render(w, h, &datas[index]);
    SDL_GL_SwapWindow(state->windows[index]);
    ++frames;
}

#ifndef SDL_PLATFORM_EMSCRIPTEN
static int SDLCALL
render_thread_fn(void *render_ctx)
{
    thread_data *thread = render_ctx;

    while (!done && !thread->done && state->windows[thread->index]) {
        if (SDL_CompareAndSwapAtomicInt(&thread->suspended, WAIT_STATE_ENTER_SEM, WAIT_STATE_WAITING_ON_SEM)) {
            SDL_WaitSemaphore(thread->suspend_sem);
        }
        render_window(thread->index);
    }

    SDL_GL_MakeCurrent(state->windows[thread->index], NULL);
    return 0;
}

static thread_data *GetThreadDataForWindow(SDL_WindowID id)
{
    int i;
    SDL_Window *window = SDL_GetWindowFromID(id);
    if (window) {
        for (i = 0; i < state->num_windows; ++i) {
            if (window == state->windows[i]) {
                return &threads[i];
            }
        }
    }
    return NULL;
}

static void
loop_threaded(void)
{
    SDL_Event event;
    thread_data *tdata;

    /* Wait for events */
    while (SDL_WaitEvent(&event) && !done) {
        if (suspend_when_occluded && event.type == SDL_EVENT_WINDOW_OCCLUDED) {
            tdata = GetThreadDataForWindow(event.window.windowID);
            if (tdata) {
                SDL_CompareAndSwapAtomicInt(&tdata->suspended, WAIT_STATE_GO, WAIT_STATE_ENTER_SEM);
            }
        } else if (suspend_when_occluded && event.type == SDL_EVENT_WINDOW_EXPOSED) {
            tdata = GetThreadDataForWindow(event.window.windowID);
            if (tdata) {
                if (SDL_SetAtomicInt(&tdata->suspended, WAIT_STATE_GO) == WAIT_STATE_WAITING_ON_SEM) {
                    SDL_SignalSemaphore(tdata->suspend_sem);
                }
            }
        } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            tdata = GetThreadDataForWindow(event.window.windowID);
            if (tdata) {
                /* Stop the render thread when the window is closed */
                tdata->done = 1;
                if (tdata->thread) {
                    SDL_SetAtomicInt(&tdata->suspended, WAIT_STATE_GO);
                    SDL_SignalSemaphore(tdata->suspend_sem);
                    SDL_WaitThread(tdata->thread, NULL);
                    tdata->thread = NULL;
                    SDL_DestroySemaphore(tdata->suspend_sem);
                }
                break;
            }
        }
        SDLTest_CommonEvent(state, &event, &done);
    }
}
#endif

static void
loop(void)
{
    SDL_Event event;
    int i;
    int active_windows = 0;

    /* Check for events */
    while (SDL_PollEvent(&event) && !done) {
        SDLTest_CommonEvent(state, &event, &done);
    }
    if (!done) {
        for (i = 0; i < state->num_windows; ++i) {
            if (state->windows[i] == NULL ||
                (suspend_when_occluded && (SDL_GetWindowFlags(state->windows[i]) & SDL_WINDOW_OCCLUDED))) {
                continue;
            }
            ++active_windows;
            render_window(i);
        }
    }
#ifdef SDL_PLATFORM_EMSCRIPTEN
    else {
        emscripten_cancel_main_loop();
    }
#endif

    /* If all windows are occluded, throttle event polling to 15hz. */
    if (!done && !active_windows) {
        SDL_DelayNS(SDL_NS_PER_SECOND / 15);
    }
}

int main(int argc, char *argv[])
{
    int fsaa, accel, threaded;
    int value;
    int i;
    const SDL_DisplayMode *mode;
    Uint64 then, now;
    shader_data *data;

    /* Initialize parameters */
    fsaa = 0;
    accel = 0;
    threaded = 0;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            if (SDL_strcasecmp(argv[i], "--fsaa") == 0) {
                ++fsaa;
                consumed = 1;
            } else if (SDL_strcasecmp(argv[i], "--accel") == 0) {
                ++accel;
                consumed = 1;
            } else if (SDL_strcasecmp(argv[i], "--threaded") == 0) {
                ++threaded;
                consumed = 1;
            } else if(SDL_strcasecmp(argv[i], "--suspend-when-occluded") == 0) {
                suspend_when_occluded = true;
                consumed = 1;
            } else if (SDL_strcasecmp(argv[i], "--zdepth") == 0) {
                i++;
                if (!argv[i]) {
                    consumed = -1;
                } else {
                    char *endptr = NULL;
                    depth = (int)SDL_strtol(argv[i], &endptr, 0);
                    if (endptr != argv[i] && *endptr == '\0') {
                        consumed = 1;
                    } else {
                        consumed = -1;
                    }
                }
            } else {
                consumed = -1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--fsaa]", "[--accel]", "[--zdepth %d]", "[--threaded]", "[--suspend-when-occluded]",NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            quit(1);
        }
        i += consumed;
    }

    /* Set OpenGL parameters */
    state->window_flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    state->gl_red_size = 5;
    state->gl_green_size = 5;
    state->gl_blue_size = 5;
    state->gl_depth_size = depth;
    state->gl_major_version = 2;
    state->gl_minor_version = 0;
    state->gl_profile_mask = SDL_GL_CONTEXT_PROFILE_ES;

    if (fsaa) {
        state->gl_multisamplebuffers = 1;
        state->gl_multisamplesamples = fsaa;
    }
    if (accel) {
        state->gl_accelerated = 1;
    }
    if (!SDLTest_CommonInit(state)) {
        quit(2);
        return 0;
    }

    context = (SDL_GLContext *)SDL_calloc(state->num_windows, sizeof(*context));
    if (!context) {
        SDL_Log("Out of memory!");
        quit(2);
    }

    /* Create OpenGL ES contexts */
    for (i = 0; i < state->num_windows; i++) {
        context[i] = SDL_GL_CreateContext(state->windows[i]);
        if (!context[i]) {
            SDL_Log("SDL_GL_CreateContext(): %s", SDL_GetError());
            quit(2);
        }
    }

    /* Important: call this *after* creating the context */
    if (!LoadContext(&ctx)) {
        SDL_Log("Could not load GLES2 functions");
        quit(2);
        return 0;
    }

    SDL_GL_SetSwapInterval(state->render_vsync);

    mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    SDL_Log("Threaded  : %s", threaded ? "yes" : "no");
    if (mode) {
        SDL_Log("Screen bpp: %d", SDL_BITSPERPIXEL(mode->format));
        SDL_Log("%s", "");
    }
    SDL_Log("Vendor     : %s", ctx.glGetString(GL_VENDOR));
    SDL_Log("Renderer   : %s", ctx.glGetString(GL_RENDERER));
    SDL_Log("Version    : %s", ctx.glGetString(GL_VERSION));
    SDL_Log("Extensions : %s", ctx.glGetString(GL_EXTENSIONS));
    SDL_Log("%s", "");

    if (SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &value)) {
        SDL_Log("SDL_GL_RED_SIZE: requested %d, got %d", 5, value);
    } else {
        SDL_Log("Failed to get SDL_GL_RED_SIZE: %s",
                SDL_GetError());
    }
    if (SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &value)) {
        SDL_Log("SDL_GL_GREEN_SIZE: requested %d, got %d", 5, value);
    } else {
        SDL_Log("Failed to get SDL_GL_GREEN_SIZE: %s",
                SDL_GetError());
    }
    if (SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &value)) {
        SDL_Log("SDL_GL_BLUE_SIZE: requested %d, got %d", 5, value);
    } else {
        SDL_Log("Failed to get SDL_GL_BLUE_SIZE: %s",
                SDL_GetError());
    }
    if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &value)) {
        SDL_Log("SDL_GL_DEPTH_SIZE: requested %d, got %d", depth, value);
    } else {
        SDL_Log("Failed to get SDL_GL_DEPTH_SIZE: %s",
                SDL_GetError());
    }
    if (fsaa) {
        if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &value)) {
            SDL_Log("SDL_GL_MULTISAMPLEBUFFERS: requested 1, got %d", value);
        } else {
            SDL_Log("Failed to get SDL_GL_MULTISAMPLEBUFFERS: %s",
                    SDL_GetError());
        }
        if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &value)) {
            SDL_Log("SDL_GL_MULTISAMPLESAMPLES: requested %d, got %d", fsaa,
                    value);
        } else {
            SDL_Log("Failed to get SDL_GL_MULTISAMPLESAMPLES: %s",
                    SDL_GetError());
        }
    }
    if (accel) {
        if (SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &value)) {
            SDL_Log("SDL_GL_ACCELERATED_VISUAL: requested 1, got %d", value);
        } else {
            SDL_Log("Failed to get SDL_GL_ACCELERATED_VISUAL: %s",
                    SDL_GetError());
        }
    }

    datas = (shader_data *)SDL_calloc(state->num_windows, sizeof(shader_data));

    /* Set rendering settings for each context */
    for (i = 0; i < state->num_windows; ++i) {

        int w, h;
        if (!SDL_GL_MakeCurrent(state->windows[i], context[i])) {
            SDL_Log("SDL_GL_MakeCurrent(): %s", SDL_GetError());

            /* Continue for next window */
            continue;
        }
        SDL_GetWindowSizeInPixels(state->windows[i], &w, &h);
        ctx.glViewport(0, 0, w, h);

        data = &datas[i];
        data->angle_x = 0;
        data->angle_y = 0;
        data->angle_z = 0;

        /* Shader Initialization */
        process_shader(&data->shader_vert, g_shader_vert_src, GL_VERTEX_SHADER);
        process_shader(&data->shader_frag, g_shader_frag_src, GL_FRAGMENT_SHADER);

        /* Create shader_program (ready to attach shaders) */
        data->shader_program = GL_CHECK(ctx.glCreateProgram());

        /* Attach shaders and link shader_program */
        link_program(data);

        /* Get attribute locations of non-fixed attributes like color and texture coordinates. */
        data->attr_position = GL_CHECK(ctx.glGetAttribLocation(data->shader_program, "av4position"));
        data->attr_color = GL_CHECK(ctx.glGetAttribLocation(data->shader_program, "av3color"));

        /* Get uniform locations */
        data->attr_mvp = GL_CHECK(ctx.glGetUniformLocation(data->shader_program, "mvp"));

        GL_CHECK(ctx.glUseProgram(data->shader_program));

        /* Enable attributes for position, color and texture coordinates etc. */
        GL_CHECK(ctx.glEnableVertexAttribArray(data->attr_position));
        GL_CHECK(ctx.glEnableVertexAttribArray(data->attr_color));

        /* Populate attributes for position, color and texture coordinates etc. */

        GL_CHECK(ctx.glGenBuffers(1, &data->position_buffer));
        GL_CHECK(ctx.glBindBuffer(GL_ARRAY_BUFFER, data->position_buffer));
        GL_CHECK(ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertices), g_vertices, GL_STATIC_DRAW));
        GL_CHECK(ctx.glVertexAttribPointer(data->attr_position, 3, GL_FLOAT, GL_FALSE, 0, 0));
        GL_CHECK(ctx.glBindBuffer(GL_ARRAY_BUFFER, 0));

        GL_CHECK(ctx.glGenBuffers(1, &data->color_buffer));
        GL_CHECK(ctx.glBindBuffer(GL_ARRAY_BUFFER, data->color_buffer));
        GL_CHECK(ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(g_colors), g_colors, GL_STATIC_DRAW));
        GL_CHECK(ctx.glVertexAttribPointer(data->attr_color, 3, GL_FLOAT, GL_FALSE, 0, 0));
        GL_CHECK(ctx.glBindBuffer(GL_ARRAY_BUFFER, 0));

        GL_CHECK(ctx.glEnable(GL_CULL_FACE));
        GL_CHECK(ctx.glEnable(GL_DEPTH_TEST));

        SDL_GL_MakeCurrent(state->windows[i], NULL);
    }

    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    if (threaded) {
        threads = (thread_data *)SDL_calloc(state->num_windows, sizeof(thread_data));

        /* Start a render thread for each window */
        for (i = 0; i < state->num_windows; ++i) {
            threads[i].index = i;
            SDL_SetAtomicInt(&threads[i].suspended, 0);
            threads[i].suspend_sem = SDL_CreateSemaphore(0);
            threads[i].thread = SDL_CreateThread(render_thread_fn, "RenderThread", &threads[i]);
        }

        while (!done) {
            loop_threaded();
        }

        /* Join the remaining render threads (if any) */
        for (i = 0; i < state->num_windows; ++i) {
            threads[i].done = 1;
            if (threads[i].thread) {
                SDL_WaitThread(threads[i].thread, NULL);
            }
        }
        SDL_free(threads);
    } else {
        while (!done) {
            loop();
        }
    }
#endif

    /* Print out some timing information */
    now = SDL_GetTicks();
    if (now > then) {
        SDL_Log("%2.2f frames per second",
                ((double)frames * 1000) / (now - then));
    }
#ifndef SDL_PLATFORM_ANDROID
    quit(0);
#endif
    return 0;
}

#else /* HAVE_OPENGLES2 */

int main(int argc, char *argv[])
{
    SDL_Log("No OpenGL ES support on this system");
    return 1;
}

#endif /* HAVE_OPENGLES2 */
