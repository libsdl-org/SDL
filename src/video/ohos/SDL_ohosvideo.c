#include "../SDL_sysvideo.h"
#include "SDL_internal.h"
#include "dynapi/SDL_dynapi_overrides.h"

#ifdef SDL_VIDEO_DRIVER_OHOS
#include "../../core/ohos/SDL_ohos.h"
#include "SDL_ohosgl.h"
#include "SDL_ohosvulkan.h"
#include "SDL_ohoswindow.h"

bool OHOS_VideoInit(SDL_VideoDevice *_this)
{
    SDL_DisplayMode mode;
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGBA32;
    mode.w = OHOS_FetchWidth();
    mode.h = OHOS_FetchHeight();
    mode.refresh_rate = 60;

    SDL_AddBasicVideoDisplay(&mode);
    return true;
}
void OHOS_VideoQuit(SDL_VideoDevice *_this)
{
}
void OHOS_DeviceFree(SDL_VideoDevice *device)
{
    SDL_free(device);
}
bool OHOS_SetClipboardTextImpl(SDL_VideoDevice *, const char *data)
{
    OHOS_SetClipboardText(data);
    return true;
}
bool OHOS_HasClipboardText(SDL_VideoDevice *)
{
    return false;
}
bool OHOS_HasScreenKeyboardSupport(SDL_VideoDevice *)
{
    return true;
}
bool OHOS_IsScreenKeyboardShownImpl(SDL_VideoDevice *, SDL_Window *)
{
    return OHOS_IsScreenKeyboardShown();
}
bool OHOS_StartTextInputImpl(SDL_VideoDevice *, SDL_Window *, SDL_PropertiesID)
{
    OHOS_StartTextInput();
    return true;
}
bool OHOS_StopTextInputImpl(SDL_VideoDevice *, SDL_Window *)
{
    OHOS_StopTextInput();
    return true;
}
void OHOS_PumpEvents(SDL_VideoDevice *)
{
    
}
static SDL_VideoDevice *OHOS_CreateDevice(void)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }

    data = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        return NULL;
    }

    device->internal = data;
    device->free = OHOS_DeviceFree;

    device->VideoInit = OHOS_VideoInit;
    device->VideoQuit = OHOS_VideoQuit;
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = OHOS_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = OHOS_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = OHOS_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = OHOS_Vulkan_CreateSurface;
    device->Vulkan_DestroySurface = OHOS_Vulkan_DestroySurface;
#endif
    device->CreateSDLWindow = OHOS_CreateWindow;
    device->DestroyWindow = OHOS_DestroyWindow;

#ifdef SDL_VIDEO_OPENGL_EGL
    device->GL_LoadLibrary = OHOS_GLES_LoadLibrary;
    device->GL_MakeCurrent = OHOS_GLES_MakeCurrent;
    device->GL_CreateContext = OHOS_GLES_CreateContext;
    device->GL_SwapWindow = OHOS_GLES_SwapWindow;
    device->GL_GetProcAddress = SDL_EGL_GetProcAddressInternal;
    device->GL_UnloadLibrary = SDL_EGL_UnloadLibrary;
    device->GL_SetSwapInterval = SDL_EGL_SetSwapInterval;
    device->GL_GetSwapInterval = SDL_EGL_GetSwapInterval;
    device->GL_DestroyContext = SDL_EGL_DestroyContext;
#endif
    
    device->SetClipboardText = OHOS_SetClipboardTextImpl;
    device->HasClipboardText = OHOS_HasClipboardText;
    
    device->HasScreenKeyboardSupport = OHOS_HasScreenKeyboardSupport;
    device->IsScreenKeyboardShown = OHOS_IsScreenKeyboardShownImpl;
    device->StartTextInput = OHOS_StartTextInputImpl;
    device->StopTextInput = OHOS_StopTextInputImpl;
    
    device->PumpEvents = OHOS_PumpEvents;

    return device;
}
bool OHOS_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
    int length1 = SDL_strlen(messageboxdata->title) + 1;
    char* titlecopy = (char*)SDL_malloc(length1);
    SDL_memcpy(titlecopy, messageboxdata->title, length1);
    length1 = SDL_strlen(messageboxdata->message) + 1;
    char* messagecopy = (char*)SDL_malloc(length1);
    SDL_memcpy(messagecopy, messageboxdata->message, length1);
    OHOS_MessageBox(titlecopy, messagecopy);
    return true;
}
VideoBootStrap OHOS_bootstrap = {
    "ohos", "OpenHarmony video driver",
    OHOS_CreateDevice,
    OHOS_ShowMessageBox,
    false
};
#endif
