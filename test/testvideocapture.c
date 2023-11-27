/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_test.h"
#include "SDL3/SDL_video_capture.h"
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static const char *usage = "\
 \n\
 =========================================================================\n\
 \n\
Use keyboards:\n\
 o: open first video capture device. (close previously opened)\n\
 l: switch to, and list video capture devices\n\
 i: information about status (Init, Playing, Stopped)\n\
 f: formats and resolutions available\n\
 s: start / stop capture\n\
 h: display help\n\
 esc: exit \n\
 \n\
 =========================================================================\n\
 \n\
";

typedef struct {
    Uint64 next_check;
    int frame_counter;
    int check_delay;
    double last_fps;
} measure_fps_t;

static void
update_fps(measure_fps_t *m)
{
    Uint64 now = SDL_GetTicks();
    Uint64 deadline;
    m->frame_counter++;
    if (m->check_delay == 0) {
        m->check_delay = 1500;
    }
    deadline = m->next_check;
    if (now >= deadline) {
        /* Print out some timing information */
        const Uint64 then = m->next_check - m->check_delay;
        m->last_fps = ((double) m->frame_counter * 1000) / (now - then);
        m->next_check = now + m->check_delay;
        m->frame_counter = 0;
    }
}

#if defined(__linux__) && !defined(__ANDROID__)
static void load_average(float *val)
{
    FILE *fp = 0;
    char line[1024];
    fp = fopen("/proc/loadavg", "rt");
    if (fp) {
        char *s = fgets(line, sizeof(line), fp);
        if (s) {
            SDL_sscanf(s, "%f", val);
        }
        fclose(fp);
    }
}
#endif


struct data_capture_t {
    SDL_VideoCaptureDevice *device;
    SDL_VideoCaptureSpec obtained;
    int stopped;
    SDL_VideoCaptureFrame frame_current;
    measure_fps_t fps_capture;
    SDL_Texture *texture;
    int texture_updated;
};

#define SAVE_CAPTURE_STATE(x)                                               \
    data_capture_tab[(x)].device = device;                                  \
    data_capture_tab[(x)].obtained = obtained;                              \
    data_capture_tab[(x)].stopped = stopped;                                \
    data_capture_tab[(x)].frame_current = frame_current;                    \
    data_capture_tab[(x)].fps_capture = fps_capture;                        \
    data_capture_tab[(x)].texture = texture;                                \
    data_capture_tab[(x)].texture_updated = texture_updated;                \


#define RESTORE_CAPTURE_STATE(x)                                            \
    device = data_capture_tab[(x)].device;                                  \
    obtained = data_capture_tab[(x)].obtained;                              \
    stopped = data_capture_tab[(x)].stopped;                                \
    frame_current = data_capture_tab[(x)].frame_current;                    \
    fps_capture = data_capture_tab[(x)].fps_capture;                        \
    texture = data_capture_tab[(x)].texture;                                \
    texture_updated = data_capture_tab[(x)].texture_updated;                \





static SDL_VideoCaptureDeviceID get_instance_id(int index) {
    int ret = 0;
    int num = 0;
    SDL_VideoCaptureDeviceID *devices;
    devices = SDL_GetVideoCaptureDevices(&num);
    if (devices) {
        if (index >= 0 && index < num) {
            ret = devices[index];
        }
        SDL_free(devices);
    }

    if (ret == 0) {
/*        SDL_Log("invalid index"); */
    }

    return ret;
}



int main(int argc, char **argv)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Event evt;
    int quit = 0;

    SDLTest_CommonState  *state;

    int current_dev = 0;
    measure_fps_t fps_main;


    SDL_FRect r_playstop = { 50, 50, 120, 50 };
    SDL_FRect r_close = { 50 + (120 + 50) * 1, 50, 120, 50 };

    SDL_FRect r_open = { 50 + (120 + 50) * 2, 50, 120, 50 };

    SDL_FRect r_format = { 50 + (120 + 50) * 3, 50, 120, 50 };
    SDL_FRect r_listdev = { 50 + (120 + 50) * 4, 50, 120, 50 };

    SDL_VideoCaptureDevice *device;
    SDL_VideoCaptureSpec obtained;
    int stopped = 0;
    SDL_VideoCaptureFrame frame_current;
    measure_fps_t fps_capture;
    SDL_Texture *texture = NULL;
    int texture_updated = 0;

    struct data_capture_t data_capture_tab[16];
    const int data_capture_tab_size = SDL_arraysize(data_capture_tab);

    SDL_zero(fps_main);
    SDL_zero(fps_capture);
    SDL_zero(frame_current);
    SDL_zeroa(data_capture_tab);

    /* Set 0 to disable TouchEvent to be duplicated as MouseEvent with SDL_TOUCH_MOUSEID */
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    /* Set 0 to disable MouseEvent to be duplicated as TouchEvent with SDL_MOUSE_TOUCHID */
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

    {
        int i;
        for (i = 0; i < data_capture_tab_size; i++) {
            data_capture_tab[i].device = NULL;
        }
    }

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    {
        int i;
        for (i = 1; i < argc;) {
            int consumed;

            consumed = SDLTest_CommonArg(state, i);
            if (consumed <= 0) {
                static const char *options[] = {NULL};
                SDLTest_CommonLogUsage(state, argv[0], options);
                SDLTest_CommonDestroyState(state);
                return 1;
            }

            i += consumed;
        }
    }

    SDL_Log("%s", usage);

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) { /* FIXME: SDL_INIT_JOYSTICK needed for add/removing devices at runtime */
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Local Video", 1000, 800, 0);
    if (window == NULL) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

    renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        /* SDL_Log("Couldn't create renderer: %s", SDL_GetError()); */
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

    device = SDL_OpenVideoCapture(0);

    if (!device) {
        SDL_Log("Error SDL_OpenVideoCapture: %s", SDL_GetError());
    }

    {
        /* List formats */
        int i, num = SDL_GetNumVideoCaptureFormats(device);
        for (i = 0; i < num; i++) {
            Uint32 format;
            SDL_GetVideoCaptureFormat(device, i, &format);
            SDL_Log("format %d/%d: %s", i, num, SDL_GetPixelFormatName(format));
            {
                int w, h;
                int j, num2 = SDL_GetNumVideoCaptureFrameSizes(device, format);
                for (j = 0; j < num2; j++) {
                    SDL_GetVideoCaptureFrameSize(device, format, j, &w, &h);
                    SDL_Log("  framesizes %d/%d :  %d x %d", j, num2, w, h);
                }
            }
        }
    }

    /* Set Spec */
    {
        int ret;
        /* forced_format */
        SDL_VideoCaptureSpec desired;
        SDL_zero(desired);
        desired.width = 640 * 2;
        desired.height = 360 * 2;
        desired.format = SDL_PIXELFORMAT_NV12;
        ret = SDL_SetVideoCaptureSpec(device, &desired, &obtained, SDL_VIDEO_CAPTURE_ALLOW_ANY_CHANGE);

        if (ret < 0) {
            SDL_SetVideoCaptureSpec(device, NULL, &obtained, 0);
        }
    }

    SDL_Log("Open capture video device. Obtained spec: size=%d x %d format=%s",
            obtained.width, obtained.height, SDL_GetPixelFormatName(obtained.format));

    {
        SDL_VideoCaptureSpec spec;
        if (SDL_GetVideoCaptureSpec(device, &spec) == 0) {
            SDL_Log("Read spec: size=%d x %d format=%s",
                    spec.width, spec.height, SDL_GetPixelFormatName(spec.format));
        } else {
            SDL_Log("Error read spec: %s", SDL_GetError());
        }
    }

    if (SDL_StartVideoCapture(device) < 0) {
        SDL_Log("error SDL_StartVideoCapture(): %s", SDL_GetError());
    }

    while (!quit) {

        SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0x33, 255);

        SDL_RenderFillRect(renderer, &r_playstop);
        SDL_RenderFillRect(renderer, &r_close);
        SDL_RenderFillRect(renderer, &r_open);
        SDL_RenderFillRect(renderer, &r_format);
        SDL_RenderFillRect(renderer, &r_listdev);

        SDL_SetRenderDrawColor(renderer, 0xcc, 0xcc, 0xcc, 255);

        SDLTest_DrawString(renderer, r_playstop.x + 5, r_playstop.y + 5, "play stop");
        SDLTest_DrawString(renderer, r_close.x + 5, r_close.y + 5, "close");
        SDLTest_DrawString(renderer, r_open.x + 5, r_open.y + 5, "open dev");
        SDLTest_DrawString(renderer, r_format.x + 5, r_format.y + 5, "formats");

        {
            char buf[256];
            SDL_snprintf(buf, 256, "device %d", current_dev);
            SDLTest_DrawString(renderer, r_listdev.x + 5, r_listdev.y + 5, buf);
        }

        while (SDL_PollEvent(&evt)) {
            SDL_FRect *r = NULL;
            SDL_FPoint pt;
            int sym = 0;

            pt.x = 0;
            pt.y = 0;

            SDL_ConvertEventToRenderCoordinates(renderer, &evt);

            switch (evt.type)
            {
                case SDL_EVENT_KEY_DOWN:
                    {
                        sym = evt.key.keysym.sym;
                        break;
                    }
                case SDL_EVENT_QUIT:
                    {
                        quit = 1;
                        SDL_Log("Ctlr+C : Quit!");
                    }
                    break;

                case SDL_EVENT_FINGER_DOWN:
                    {
                        pt.x = evt.tfinger.x;
                        pt.y = evt.tfinger.y;
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    {
                        pt.x = evt.button.x;
                        pt.y = evt.button.y;
                    }
                    break;
            }

            if (pt.x != 0 && pt.y != 0) {
                if (SDL_PointInRectFloat(&pt, &r_playstop)) {
                    r = &r_playstop;
                    sym = SDLK_s;
                }
                if (SDL_PointInRectFloat(&pt, &r_close)) {
                    r = &r_close;
                    sym = SDLK_c;
                }
                if (SDL_PointInRectFloat(&pt, &r_open)) {
                    r = &r_open;
                    sym = SDLK_o;
                }

                if (SDL_PointInRectFloat(&pt, &r_format)) {
                    r = &r_format;
                    sym = SDLK_f;
                }
                if (SDL_PointInRectFloat(&pt, &r_listdev)) {
                    r = &r_listdev;
                    sym = SDLK_l;
                }
            }


            if (r) {
                SDL_SetRenderDrawColor(renderer, 0x33, 0, 0, 255);
                SDL_RenderFillRect(renderer, r);
            }


            if (sym == SDLK_c) {
                if (frame_current.num_planes) {
                    SDL_ReleaseVideoCaptureFrame(device, &frame_current);
                }
                SDL_CloseVideoCapture(device);
                device = NULL;
                SDL_Log("Close");
            }

            if (sym == SDLK_o) {
                if (device) {
                    SDL_Log("Close previous ..");
                    if (frame_current.num_planes) {
                        SDL_ReleaseVideoCaptureFrame(device, &frame_current);
                    }
                    SDL_CloseVideoCapture(device);
                }

                texture_updated = 0;

                SDL_ClearError();

                SDL_Log("Try to open:%s", SDL_GetVideoCaptureDeviceName(get_instance_id(current_dev)));

                obtained.width = 640 * 2;
                obtained.height = 360 * 2;
                device = SDL_OpenVideoCaptureWithSpec(get_instance_id(current_dev), &obtained, &obtained, SDL_VIDEO_CAPTURE_ALLOW_ANY_CHANGE);

                /* spec may have changed because of re-open */
                if (texture) {
                    SDL_DestroyTexture(texture);
                    texture = NULL;
                }

                SDL_Log("Open device:%p %s", (void*)device, SDL_GetError());
                stopped = 0;
            }

            if (sym == SDLK_l) {
                int num = 0;
                SDL_VideoCaptureDeviceID *devices;
                int i;
                devices = SDL_GetVideoCaptureDevices(&num);

                SDL_Log("Num devices : %d", num);
                for (i = 0; i < num; i++) {
                    SDL_Log("Device %d/%d : %s", i, num, SDL_GetVideoCaptureDeviceName(devices[i]));
                }
                SDL_free(devices);

                SAVE_CAPTURE_STATE(current_dev);

                current_dev += 1;
                if (current_dev >= num || current_dev >= (int) SDL_arraysize(data_capture_tab)) {
                    current_dev = 0;
                }

                RESTORE_CAPTURE_STATE(current_dev);
                SDL_Log("--> select dev %d / %d", current_dev, num);
            }

            if (sym == SDLK_i) {
                SDL_VideoCaptureStatus status = SDL_GetVideoCaptureStatus(device);
                if (status == SDL_VIDEO_CAPTURE_STOPPED) { SDL_Log("STOPPED"); }
                if (status == SDL_VIDEO_CAPTURE_PLAYING) { SDL_Log("PLAYING"); }
                if (status == SDL_VIDEO_CAPTURE_INIT) { SDL_Log("INIT"); }
            }

            if (sym == SDLK_s) {
                if (stopped) {
                    SDL_Log("Stop");
                    SDL_StopVideoCapture(device);
                } else {
                    SDL_Log("Start");
                    SDL_StartVideoCapture(device);
                }
                stopped = !stopped;
            }

            if (sym == SDLK_f) {
                SDL_Log("List formats");

                if (!device) {
                    device = SDL_OpenVideoCapture(get_instance_id(current_dev));
                }

                /* List formats */
                {
                    int i, num = SDL_GetNumVideoCaptureFormats(device);
                    for (i = 0; i < num; i++) {
                        Uint32 format;
                        SDL_GetVideoCaptureFormat(device, i, &format);
                        SDL_Log("format %d/%d : %s", i, num, SDL_GetPixelFormatName(format));
                        {
                            int w, h;
                            int j, num2 = SDL_GetNumVideoCaptureFrameSizes(device, format);
                            for (j = 0; j < num2; j++) {
                                SDL_GetVideoCaptureFrameSize(device, format, j, &w, &h);
                                SDL_Log("  framesizes %d/%d :  %d x %d", j, num2, w, h);
                            }
                        }
                    }
                }
            }
            if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) {
                quit = 1;
                SDL_Log("Key : Escape!");
            }

            if (sym == SDLK_h || sym == SDLK_F1) {
                SDL_Log("%s", usage);
            }
        }


        SAVE_CAPTURE_STATE(current_dev);

        {
            int i, n = SDL_arraysize(data_capture_tab);
            for (i = 0; i < n; i++) {
                RESTORE_CAPTURE_STATE(i);

                if (!device) {
                    /* device has been closed */
                    frame_current.num_planes = 0;
                    texture_updated = 0;
                } else {
                    int ret;
                    SDL_VideoCaptureFrame frame_next;
                    SDL_zero(frame_next);

                    ret = SDL_AcquireVideoCaptureFrame(device, &frame_next);
                    if (ret < 0) {
                        SDL_Log("dev[%d] err SDL_AcquireVideoCaptureFrame: %s", i, SDL_GetError());
                    }
#if 1
                    if (frame_next.num_planes) {
                        SDL_Log("dev[%d] frame: %p  at %" SDL_PRIu64, i, (void*)frame_next.data[0], frame_next.timestampNS);
                    }
#endif

                    if (frame_next.num_planes) {

                        update_fps(&fps_capture);

                        if (frame_current.num_planes) {
                            ret = SDL_ReleaseVideoCaptureFrame(device, &frame_current);
                            if (ret < 0) {
                                SDL_Log("dev[%d] err SDL_ReleaseVideoCaptureFrame: %s", i, SDL_GetError());
                            }
                        }
                        frame_current = frame_next;
                        texture_updated = 0;
                    }
                }

                SAVE_CAPTURE_STATE(i);
            }
        }


        RESTORE_CAPTURE_STATE(current_dev);



        /* Moving square */
        SDL_SetRenderDrawColor(renderer, 0, 0xff, 0, 255);
        {
            SDL_FRect r;
            static float x = 0;
            x += 10;
            if (x > 1000) {
                x = 0;
            }
            r.x = x;
            r.y = 100;
            r.w = r.h = 10;
            SDL_RenderFillRect(renderer, &r);
        }

        SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0x33, 255);


        SAVE_CAPTURE_STATE(current_dev);

        {
            int i, n = SDL_arraysize(data_capture_tab);
            for (i = 0; i < n; i++) {
                RESTORE_CAPTURE_STATE(i);

                /* Update SDL_Texture with last video frame (only once per new frame) */
                if (frame_current.num_planes && texture_updated == 0) {

                    /* Create texture with appropriate format (for DMABUF or not) */
                    if (texture == NULL) {
                        Uint32 format = obtained.format;
                        texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STATIC, obtained.width, obtained.height);
                        if (texture == NULL) {
                            SDL_Log("Couldn't create texture: %s", SDL_GetError());
                            return 1;
                        }
                    }

                    {
                        /* Use software data */
                        if (frame_current.num_planes == 1) {
                            SDL_UpdateTexture(texture, NULL,
                                    frame_current.data[0], frame_current.pitch[0]);
                        } else if (frame_current.num_planes == 2) {
                            SDL_UpdateNVTexture(texture, NULL,
                                    frame_current.data[0], frame_current.pitch[0],
                                    frame_current.data[1], frame_current.pitch[1]);
                        } else if (frame_current.num_planes == 3) {
                            SDL_UpdateYUVTexture(texture, NULL, frame_current.data[0], frame_current.pitch[0],
                                    frame_current.data[1], frame_current.pitch[1],
                                    frame_current.data[2], frame_current.pitch[2]);
                        }
                        texture_updated = 1;
                    }
                }

                SAVE_CAPTURE_STATE(i);
            }
        }


        RESTORE_CAPTURE_STATE(current_dev);

        {
            int i, n = SDL_arraysize(data_capture_tab);
            int win_w, win_h;
            int total_texture_updated = 0;
            int curr_texture_updated = 0;
            for (i = 0; i < n; i++) {
                if (data_capture_tab[i].texture_updated) {
                    total_texture_updated += 1;
                }
            }

            SDL_GetRenderOutputSize(renderer, &win_w, &win_h);


            for (i = 0; i < n; i++) {
                RESTORE_CAPTURE_STATE(i);
                /* RenderCopy the SDL_Texture */
                if (texture_updated == 1) {
                    /* Scale texture to fit the screen */

                    int tw, th;
                    int w;
                    SDL_FRect d;
                    SDL_QueryTexture(texture, NULL, NULL, &tw, &th);

                    w = win_w / total_texture_updated;

                    if (tw > w - 20) {
                        float scale = (float) (w - 20) / (float) tw;
                        tw = w - 20;
                        th = (int)((float) th * scale);
                    }
                    d.x = (float)(10 + curr_texture_updated * w);
                    d.y = (float)(win_h - th);
                    d.w = (float)tw;
                    d.h = (float)(th - 10);
                    SDL_RenderTexture(renderer, texture, NULL, &d);

                    curr_texture_updated += 1;
                }
            }

        }

        RESTORE_CAPTURE_STATE(current_dev);


        /* display status and FPS */
        if (!device) {
#ifdef __IOS__
            const float x_offset = 500;
#else
            const float x_offset = 0;
#endif
            char buf[256];
            SDL_snprintf(buf, 256, "Device %d (%s) is not opened", current_dev, SDL_GetVideoCaptureDeviceName(get_instance_id(current_dev)));
            SDLTest_DrawString(renderer, x_offset + 10, 10, buf);
        } else {
#ifdef __IOS__
            const float x_offset = 500;
#else
            const float x_offset = 0;
#endif
            const char *status = "no status";
            char buf[256];

            if (device) {
                SDL_VideoCaptureStatus s = SDL_GetVideoCaptureStatus(device);
                if (s == SDL_VIDEO_CAPTURE_INIT) {
                    status = "init";
                } else if (s == SDL_VIDEO_CAPTURE_PLAYING) {
                    status = "playing";
                } else if (s == SDL_VIDEO_CAPTURE_STOPPED) {
                    status = "stopped";
                } else if (s == SDL_VIDEO_CAPTURE_FAIL) {
                    status = "failed";
                }

            }

            /* capture device, capture fps, capture status */
            SDL_snprintf(buf, 256, "Device %d - %2.2f fps - %s", current_dev, fps_capture.last_fps, status);
            SDLTest_DrawString(renderer, x_offset + 10, 10, buf);

            /* capture spec */
            SDL_snprintf(buf, sizeof(buf), "%d x %d %s", obtained.width, obtained.height, SDL_GetPixelFormatName(obtained.format));
            SDLTest_DrawString(renderer, x_offset + 10, 20, buf);

            /* video fps */
            SDL_snprintf(buf, sizeof(buf), "%2.2f fps", fps_main.last_fps);
            SDLTest_DrawString(renderer, x_offset + 10, 30, buf);

        }

        /* display last error */
        {
            SDLTest_DrawString(renderer, 400, 10, SDL_GetError());
        }

        /* display load average */
#if defined(__linux__) && !defined(__ANDROID__)
        {
            float val = 0.0f;
            char buf[128];
            load_average(&val);
            if (val != 0.0f) {
                SDL_snprintf(buf, sizeof(buf), "load avg %2.2f percent", val);
                SDLTest_DrawString(renderer, 800, 10, buf);
            }
        }
#endif


        SDL_Delay(20);
        SDL_RenderPresent(renderer);

        update_fps(&fps_main);

    }



    SAVE_CAPTURE_STATE(current_dev);

    {
        int i, n = SDL_arraysize(data_capture_tab);
        for (i = 0; i < n; i++) {
            RESTORE_CAPTURE_STATE(i);

            if (device) {
                if (SDL_StopVideoCapture(device) < 0) {
                    SDL_Log("error SDL_StopVideoCapture(): %s", SDL_GetError());
                }
                if (frame_current.num_planes) {
                    SDL_ReleaseVideoCaptureFrame(device, &frame_current);
                }
                SDL_CloseVideoCapture(device);
            }

            if (texture) {
                SDL_DestroyTexture(texture);
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    SDLTest_CommonDestroyState(state);

    return 0;
}
