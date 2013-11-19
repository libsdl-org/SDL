/*
  Copyright (r) 1997-2011 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "SDL_test_common.h"

#if defined(__IPHONEOS__) || defined(__ANDROID__)
#define HAVE_OPENGLES2
#endif

#ifdef HAVE_OPENGLES2

#include "SDL_opengles2.h"

static SDLTest_CommonState *state;
static SDL_GLContext *context = NULL;
static int depth = 16;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    int i;

    if (context != NULL) {
        for (i = 0; i < state->num_windows; i++) {
            if (context[i]) {
                SDL_GL_DeleteContext(context[i]);
            }
        }

        SDL_free(context);
    }

    SDLTest_CommonQuit(state);
    exit(rc);
}

#define GL_CHECK(x) \
        x; \
        { \
          GLenum glError = glGetError(); \
          if(glError != GL_NO_ERROR) { \
            fprintf(stderr, "glGetError() = %i (0x%.8x) at line %i\n", glError, glError, __LINE__); \
            quit(1); \
          } \
        }

/* 
 * Simulates desktop's glRotatef. The matrix is returned in column-major 
 * order. 
 */
static void
rotate_matrix(double angle, double x, double y, double z, float *r)
{
    double radians, c, s, c1, u[3], length;
    int i, j;

    radians = (angle * M_PI) / 180.0;

    c = cos(radians);
    s = sin(radians);

    c1 = 1.0 - cos(radians);

    length = sqrt(x * x + y * y + z * z);

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
            r[i * 4 + j] += c1 * u[i] * u[j] + (i == j ? c : 0.0);
        }
    }
}

/* 
 * Simulates gluPerspectiveMatrix 
 */
static void 
perspective_matrix(double fovy, double aspect, double znear, double zfar, float *r)
{
    int i;
    double f;

    f = 1.0/tan(fovy * 0.5);

    for (i = 0; i < 16; i++) {
        r[i] = 0.0;
    }

    r[0] = f / aspect;
    r[5] = f;
    r[10] = (znear + zfar) / (znear - zfar);
    r[11] = -1.0;
    r[14] = (2.0 * znear * zfar) / (znear - zfar);
    r[15] = 0.0;
}

/* 
 * Multiplies lhs by rhs and writes out to r. All matrices are 4x4 and column
 * major. In-place multiplication is supported.
 */
static void
multiply_matrix(float *lhs, float *rhs, float *r)
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

/* 
 * Create shader, load in source, compile, dump debug as necessary.
 *
 * shader: Pointer to return created shader ID.
 * source: Passed-in shader source code.
 * shader_type: Passed to GL, e.g. GL_VERTEX_SHADER.
 */
void 
process_shader(GLuint *shader, const char * source, GLint shader_type)
{
    GLint status;
    const char *shaders[1] = { NULL };

    /* Create shader and load into GL. */
    *shader = GL_CHECK(glCreateShader(shader_type));

    shaders[0] = source;

    GL_CHECK(glShaderSource(*shader, 1, shaders, NULL));

    /* Clean up shader source. */
    shaders[0] = NULL;

    /* Try compiling the shader. */
    GL_CHECK(glCompileShader(*shader));
    GL_CHECK(glGetShaderiv(*shader, GL_COMPILE_STATUS, &status));

    // Dump debug info (source and log) if compilation failed.
    if(status != GL_TRUE) {
        quit(-1);
    }
}

/* 3D data. Vertex range -0.5..0.5 in all axes.
* Z -0.5 is near, 0.5 is far. */
const float _vertices[] =
{
    /* Front face. */
    /* Bottom left */
    -0.5,  0.5, -0.5,
    0.5, -0.5, -0.5,
    -0.5, -0.5, -0.5,
    /* Top right */
    -0.5,  0.5, -0.5,
    0.5,  0.5, -0.5,
    0.5, -0.5, -0.5,
    /* Left face */
    /* Bottom left */
    -0.5,  0.5,  0.5,
    -0.5, -0.5, -0.5,
    -0.5, -0.5,  0.5,
    /* Top right */
    -0.5,  0.5,  0.5,
    -0.5,  0.5, -0.5,
    -0.5, -0.5, -0.5,
    /* Top face */
    /* Bottom left */
    -0.5,  0.5,  0.5,
    0.5,  0.5, -0.5,
    -0.5,  0.5, -0.5,
    /* Top right */
    -0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,
    0.5,  0.5, -0.5,
    /* Right face */
    /* Bottom left */
    0.5,  0.5, -0.5,
    0.5, -0.5,  0.5,
    0.5, -0.5, -0.5,
    /* Top right */
    0.5,  0.5, -0.5,
    0.5,  0.5,  0.5,
    0.5, -0.5,  0.5,
    /* Back face */
    /* Bottom left */
    0.5,  0.5,  0.5,
    -0.5, -0.5,  0.5,
    0.5, -0.5,  0.5,
    /* Top right */
    0.5,  0.5,  0.5,
    -0.5,  0.5,  0.5,
    -0.5, -0.5,  0.5,
    /* Bottom face */
    /* Bottom left */
    -0.5, -0.5, -0.5,
    0.5, -0.5,  0.5,
    -0.5, -0.5,  0.5,
    /* Top right */
    -0.5, -0.5, -0.5,
    0.5, -0.5, -0.5,
    0.5, -0.5,  0.5,
};

const float _colors[] =
{
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

const char* _shader_vert_src = 
" attribute vec4 av4position; "
" attribute vec3 av3color; "
" uniform mat4 mvp; "
" varying vec3 vv3color; "
" void main() { "
"    vv3color = av3color; "
"    gl_Position = mvp * av4position; "
" } ";

const char* _shader_frag_src = 
" precision lowp float; "
" varying vec3 vv3color; "
" void main() { "
"    gl_FragColor = vec4(vv3color, 1.0); "
" } ";

typedef struct shader_data
{
    GLuint shader_program, shader_frag, shader_vert;

    GLint attr_position;
    GLint attr_color, attr_mvp;

    int angle_x, angle_y, angle_z;

} shader_data;

static void
Render(unsigned int width, unsigned int height, shader_data* data)
{
    float matrix_rotate[16], matrix_modelview[16], matrix_perspective[16], matrix_mvp[16];

    /* 
    * Do some rotation with Euler angles. It is not a fixed axis as
    * quaterions would be, but the effect is cool. 
    */
    rotate_matrix(data->angle_x, 1.0, 0.0, 0.0, matrix_modelview);
    rotate_matrix(data->angle_y, 0.0, 1.0, 0.0, matrix_rotate);

    multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

    rotate_matrix(data->angle_z, 0.0, 1.0, 0.0, matrix_rotate);

    multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

    /* Pull the camera back from the cube */
    matrix_modelview[14] -= 2.5;

    perspective_matrix(45.0, (double)width/(double)height, 0.01, 100.0, matrix_perspective);
    multiply_matrix(matrix_perspective, matrix_modelview, matrix_mvp);

    GL_CHECK(glUniformMatrix4fv(data->attr_mvp, 1, GL_FALSE, matrix_mvp));

    data->angle_x += 3;
    data->angle_y += 2;
    data->angle_z += 1;

    if(data->angle_x >= 360) data->angle_x -= 360;
    if(data->angle_x < 0) data->angle_x += 360;
    if(data->angle_y >= 360) data->angle_y -= 360;
    if(data->angle_y < 0) data->angle_y += 360;
    if(data->angle_z >= 360) data->angle_z -= 360;
    if(data->angle_z < 0) data->angle_z += 360;

    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 36));
}

int
main(int argc, char *argv[])
{
    int fsaa, accel;
    int value;
    int i, done;
    SDL_DisplayMode mode;
    SDL_Event event;
    Uint32 then, now, frames;
    int status;
    shader_data *datas, *data;

    /* Initialize parameters */
    fsaa = 0;
    accel = 0;

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
            } else if (SDL_strcasecmp(argv[i], "--zdepth") == 0) {
                i++;
                if (!argv[i]) {
                    consumed = -1;
                } else {
                    depth = SDL_atoi(argv[i]);
                    consumed = 1;
                }
            } else {
                consumed = -1;
            }
        }
        if (consumed < 0) {
            fprintf(stderr, "Usage: %s %s [--fsaa] [--accel] [--zdepth %%d]\n", argv[0],
                    SDLTest_CommonUsage(state));
            quit(1);
        }
        i += consumed;
    }

    /* Set OpenGL parameters */
    state->window_flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS;
    state->gl_red_size = 5;
    state->gl_green_size = 5;
    state->gl_blue_size = 5;
    state->gl_depth_size = depth;
    state->gl_major_version = 2;
    state->gl_minor_version = 0;
    state->gl_profile_mask = SDL_GL_CONTEXT_PROFILE_ES;

    if (fsaa) {
        state->gl_multisamplebuffers=1;
        state->gl_multisamplesamples=fsaa;
    }
    if (accel) {
        state->gl_accelerated=1;
    }
    if (!SDLTest_CommonInit(state)) {
        return;
        quit(2);
    }

    context = SDL_calloc(state->num_windows, sizeof(context));
    if (context == NULL) {
        fprintf(stderr, "Out of memory!\n");
        quit(2);
    }
    
    /* Create OpenGL ES contexts */
    for (i = 0; i < state->num_windows; i++) {
        context[i] = SDL_GL_CreateContext(state->windows[i]);
        if (!context[i]) {
            fprintf(stderr, "SDL_GL_CreateContext(): %s\n", SDL_GetError());
            quit(2);
        }
    }

    if (state->render_flags & SDL_RENDERER_PRESENTVSYNC) {
        SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }

    SDL_GetCurrentDisplayMode(0, &mode);
    printf("Screen bpp: %d\n", SDL_BITSPERPIXEL(mode.format));
    printf("\n");
    printf("Vendor     : %s\n", glGetString(GL_VENDOR));
    printf("Renderer   : %s\n", glGetString(GL_RENDERER));
    printf("Version    : %s\n", glGetString(GL_VERSION));
    printf("Extensions : %s\n", glGetString(GL_EXTENSIONS));
    printf("\n");

    status = SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &value);
    if (!status) {
        printf("SDL_GL_RED_SIZE: requested %d, got %d\n", 5, value);
    } else {
        fprintf(stderr, "Failed to get SDL_GL_RED_SIZE: %s\n",
                SDL_GetError());
    }
    status = SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &value);
    if (!status) {
        printf("SDL_GL_GREEN_SIZE: requested %d, got %d\n", 5, value);
    } else {
        fprintf(stderr, "Failed to get SDL_GL_GREEN_SIZE: %s\n",
                SDL_GetError());
    }
    status = SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &value);
    if (!status) {
        printf("SDL_GL_BLUE_SIZE: requested %d, got %d\n", 5, value);
    } else {
        fprintf(stderr, "Failed to get SDL_GL_BLUE_SIZE: %s\n",
                SDL_GetError());
    }
    status = SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &value);
    if (!status) {
        printf("SDL_GL_DEPTH_SIZE: requested %d, got %d\n", depth, value);
    } else {
        fprintf(stderr, "Failed to get SDL_GL_DEPTH_SIZE: %s\n",
                SDL_GetError());
    }
    if (fsaa) {
        status = SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &value);
        if (!status) {
            printf("SDL_GL_MULTISAMPLEBUFFERS: requested 1, got %d\n", value);
        } else {
            fprintf(stderr, "Failed to get SDL_GL_MULTISAMPLEBUFFERS: %s\n",
                    SDL_GetError());
        }
        status = SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &value);
        if (!status) {
            printf("SDL_GL_MULTISAMPLESAMPLES: requested %d, got %d\n", fsaa,
                   value);
        } else {
            fprintf(stderr, "Failed to get SDL_GL_MULTISAMPLESAMPLES: %s\n",
                    SDL_GetError());
        }
    }
    if (accel) {
        status = SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &value);
        if (!status) {
            printf("SDL_GL_ACCELERATED_VISUAL: requested 1, got %d\n", value);
        } else {
            fprintf(stderr, "Failed to get SDL_GL_ACCELERATED_VISUAL: %s\n",
                    SDL_GetError());
        }
    }

    datas = SDL_calloc(state->num_windows, sizeof(shader_data));

    /* Set rendering settings for each context */
    for (i = 0; i < state->num_windows; ++i) {

        status = SDL_GL_MakeCurrent(state->windows[i], context[i]);
        if (status) {
            printf("SDL_GL_MakeCurrent(): %s\n", SDL_GetError());

            /* Continue for next window */
            continue;
        }
        glViewport(0, 0, state->window_w, state->window_h);

        data = &datas[i];
        data->angle_x = 0; data->angle_y = 0; data->angle_z = 0;

        /* Shader Initialization */
        process_shader(&data->shader_vert, _shader_vert_src, GL_VERTEX_SHADER);
        process_shader(&data->shader_frag, _shader_frag_src, GL_FRAGMENT_SHADER);

        /* Create shader_program (ready to attach shaders) */
        data->shader_program = GL_CHECK(glCreateProgram());

        /* Attach shaders and link shader_program */
        GL_CHECK(glAttachShader(data->shader_program, data->shader_vert));
        GL_CHECK(glAttachShader(data->shader_program, data->shader_frag));
        GL_CHECK(glLinkProgram(data->shader_program));

        /* Get attribute locations of non-fixed attributes like color and texture coordinates. */
        data->attr_position = GL_CHECK(glGetAttribLocation(data->shader_program, "av4position"));
        data->attr_color = GL_CHECK(glGetAttribLocation(data->shader_program, "av3color"));

        /* Get uniform locations */
        data->attr_mvp = GL_CHECK(glGetUniformLocation(data->shader_program, "mvp"));

        GL_CHECK(glUseProgram(data->shader_program));

        /* Enable attributes for position, color and texture coordinates etc. */
        GL_CHECK(glEnableVertexAttribArray(data->attr_position));
        GL_CHECK(glEnableVertexAttribArray(data->attr_color));

        /* Populate attributes for position, color and texture coordinates etc. */
        GL_CHECK(glVertexAttribPointer(data->attr_position, 3, GL_FLOAT, GL_FALSE, 0, _vertices));
        GL_CHECK(glVertexAttribPointer(data->attr_color, 3, GL_FLOAT, GL_FALSE, 0, _colors));

        GL_CHECK(glEnable(GL_CULL_FACE));
        GL_CHECK(glEnable(GL_DEPTH_TEST));
    }

    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;
    while (!done) {
        /* Check for events */
        ++frames;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        for (i = 0; i < state->num_windows; ++i) {
                            if (event.window.windowID == SDL_GetWindowID(state->windows[i])) {
                                status = SDL_GL_MakeCurrent(state->windows[i], context[i]);
                                if (status) {
                                    printf("SDL_GL_MakeCurrent(): %s\n", SDL_GetError());
                                    break;
                                }
                                /* Change view port to the new window dimensions */
                                glViewport(0, 0, event.window.data1, event.window.data2);
                                /* Update window content */
                                Render(event.window.data1, event.window.data2, &datas[i]);
                                SDL_GL_SwapWindow(state->windows[i]);
                                break;
                            }
                        }
                        break;
                }
            }
            SDLTest_CommonEvent(state, &event, &done);
        }
        for (i = 0; i < state->num_windows; ++i) {
            status = SDL_GL_MakeCurrent(state->windows[i], context[i]);
            if (status) {
                printf("SDL_GL_MakeCurrent(): %s\n", SDL_GetError());

                /* Continue for next window */
                continue;
            }
            Render(state->window_w, state->window_h, &datas[i]);
            SDL_GL_SwapWindow(state->windows[i]);
        }
    }

    /* Print out some timing information */
    now = SDL_GetTicks();
    if (now > then) {
        printf("%2.2f frames per second\n",
               ((double) frames * 1000) / (now - then));
    }
#if !defined(__ANDROID__)    
    quit(0);
#endif    
    return 0;
}

#else /* HAVE_OPENGLES2 */

int
main(int argc, char *argv[])
{
    printf("No OpenGL ES support on this system\n");
    return 1;
}

#endif /* HAVE_OPENGLES2 */

/* vi: set ts=4 sw=4 expandtab: */
