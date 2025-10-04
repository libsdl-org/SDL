#include "SDL3/SDL_assert.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_messagebox.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"
#include "SDL3/SDL_locale.h"
#include "SDL3/SDL_clipboard.h"
#include "napi/native_api.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_hints.h"
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <vulkan/vulkan_core.h>
#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostreambuilder.h>

static napi_value Add(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = {nullptr};

    napi_get_cb_info(env, info, &argc, args , nullptr, nullptr);

    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);

    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);

    double value0;
    napi_get_value_double(env, args[0], &value0);

    double value1;
    napi_get_value_double(env, args[1], &value1);

    napi_value sum;
    napi_create_double(env, value0 + value1, &sum);

    SDL_Log("Add invoke!");

    return sum;

}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "add", nullptr, Add, nullptr, nullptr, nullptr, napi_default, nullptr }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

float vtxdata[] = {
    -0.5f, -0.5f, 0.f,
    0.5f, -0.5f, 0.0f,
    0.0f, 0.5f, 0.0f
};

float vtxdata2[] = {
    0.f, 0.f, 1.f, 
    0.f, 1.f, 0.f, 
    1.f, 0.f, 0.f
};

int main()
{
    /*OH_AudioStreamBuilder* builder;
    OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);

    OH_AudioStreamBuilder_SetSamplingRate(builder, 48000);
    OH_AudioStreamBuilder_SetChannelCount(builder, 2);
    OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_U8);
    OH_AudioStreamBuilder_SetEncodingType(builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
    OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_MUSIC);
    // OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_FAST);
    
    OH_AudioRenderer_Callbacks callbacks;

    srand(time(nullptr));
    callbacks.OH_AudioRenderer_OnStreamEvent = nullptr;
    callbacks.OH_AudioRenderer_OnInterruptEvent = nullptr;
    callbacks.OH_AudioRenderer_OnError = nullptr;
    callbacks.OH_AudioRenderer_OnWriteData = [](OH_AudioRenderer* renderer, void* userData, void* buffer, int32_t length) {
        auto p = (uint8_t *)buffer;
        int t = length;
        while (t > 0) {
            *p = rand() % 255;
            p++;
            t--;
        }
        return 0;
    };
    
    OH_AudioStreamBuilder_SetRendererCallback(builder, callbacks, nullptr);
    
    OH_AudioRenderer* audioRenderer;
    OH_AudioStreamBuilder_GenerateRenderer(builder, &audioRenderer);
    OH_AudioRenderer_Start(audioRenderer);
    OH_AudioStreamBuilder_Destroy(builder);*/
    
    SDL_SetHint(SDL_HINT_EGL_LIBRARY, "libEGL.so");
    SDL_SetHint(SDL_HINT_OPENGL_LIBRARY, "libGLESv2.so");
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "libGLESv2.so");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_Init(SDL_INIT_VIDEO);
    int i = 0;
    auto t = SDL_GetPreferredLocales(&i);
    SDL_Log("Main func invoke !!! %s %s", t[0]->country, t[0]->language);
    // SDL_GL_LoadLibrary("libGLESv2.so");
    SDL_Log("sdl error: %s", SDL_GetError());
    SDL_Window* win = SDL_CreateWindow("test", 1024, 1024, SDL_WINDOW_OPENGL);
    
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "SDL Application", "test!", win);
    SDL_StartTextInput(win);
    // SDL_StopTextInput();
    
    auto context = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, context);
    
    int frgshader = ((PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader"))(GL_FRAGMENT_SHADER);
    int vexshader = ((PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader"))(GL_VERTEX_SHADER);
    
    auto frag = "varying vec4 v_color;void main() {gl_FragColor = v_color;}";
    ((PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource"))(frgshader, 1, &frag, nullptr);
    ((PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader"))(frgshader);
    int status = 1;
    ((PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv"))(frgshader, GL_COMPILE_STATUS, &status);
    int status2 = 1;
    ((PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv"))(frgshader, GL_INFO_LOG_LENGTH, &status2);
    std::string log(status2, '\0');
    ((PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog"))(frgshader, status2, nullptr, (char*)log.c_str());
    
    SDL_Log("test: %d %d %s", status, status2, log.c_str());
    
    auto vert = "attribute vec3 pos;attribute vec3 color;varying vec4 v_color;void main() {gl_Position=vec4(pos,1.0);v_color=vec4(color, 1.0);}";
    ((PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource"))(vexshader, 1, &vert, nullptr);
    ((PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader"))(vexshader);
    ((PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv"))(vexshader, GL_COMPILE_STATUS, &status);
    ((PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv"))(vexshader, GL_INFO_LOG_LENGTH, &status2);
    std::string log2(status2, '\0');
    ((PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog"))(vexshader, status2, nullptr, (char*)log2.c_str());
    
    SDL_Log("test: %d %d %s", status, status2, log2.c_str());
    
    auto prog = ((PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram"))();
    ((PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader"))(prog, vexshader);
    ((PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader"))(prog, frgshader);
    ((PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram"))(prog);
    ((PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv"))(prog, GL_LINK_STATUS, &status);
    SDL_Log("link: %d", status);
    
    ((PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader"))(vexshader);
    ((PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader"))(frgshader);
    
    while (true) {
        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        
        ((PFNGLVIEWPORTPROC)SDL_GL_GetProcAddress("glViewport"))(0, 0, w, h);
        ((PFNGLCLEARPROC)SDL_GL_GetProcAddress("glClear"))(GL_COLOR_BUFFER_BIT);
        ((PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram"))(prog);
        for (int i = 0; i < 9; i++) {
            vtxdata2[i] = SDL_randf();
            /*vtxdata2[i] += 0.01f;
            if (vtxdata2[i] >= 1.f) {
                vtxdata2[i] = 0.f;
            }*/
        }
        ((PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer"))(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)vtxdata);
        ((PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray"))(0);
        ((PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer"))(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)vtxdata2);
        ((PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray"))(1);
        
        ((PFNGLDRAWARRAYSPROC)SDL_GL_GetProcAddress("glDrawArrays"))(GL_TRIANGLES, 0, 3);
        
        SDL_GL_SwapWindow(win);
        
        SDL_Event event;
        if (SDL_PollEvent(&event)) {
            SDL_Log("event type: %d", event.type);
            
            if (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_MOTION) {
                SDL_Log("%f %f", event.tfinger.x, event.tfinger.y); 
            }
        }
    }
    
    SDL_Log("glversion: %s", ((PFNGLGETSTRINGPROC)SDL_GL_GetProcAddress("glGetString"))(GL_VERSION));
    
    SDL_GL_DestroyContext(context);
    
    SDL_DestroyWindow(win);
    
    return 0;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
