/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#include <EGL/eglplatform.h>
#include <android/native_window_jni.h>

#include "SDL_rect.h"

/* Interface from the SDL library into the Android Java activity */
/* extern SDL_bool Android_JNI_CreateContext(int majorVersion, int minorVersion, int red, int green, int blue, int alpha, int buffer, int depth, int stencil, int buffers, int samples);
extern SDL_bool Android_JNI_DeleteContext(void); */
extern void Android_JNI_SwapWindow();
extern void Android_JNI_SetActivityTitle(const char *title);
extern SDL_bool Android_JNI_GetAccelerometerValues(float values[3]);
extern void Android_JNI_ShowTextInput(SDL_Rect *inputRect);
extern void Android_JNI_HideTextInput();
extern ANativeWindow* Android_JNI_GetNativeWindow(void);

/* Audio support */
extern int Android_JNI_OpenAudioDevice(int sampleRate, int is16Bit, int channelCount, int desiredBufferFrames);
extern void* Android_JNI_GetAudioBuffer();
extern void Android_JNI_WriteAudioBuffer();
extern void Android_JNI_CloseAudioDevice();

#include "SDL_rwops.h"

int Android_JNI_FileOpen(SDL_RWops* ctx, const char* fileName, const char* mode);
Sint64 Android_JNI_FileSize(SDL_RWops* ctx);
Sint64 Android_JNI_FileSeek(SDL_RWops* ctx, Sint64 offset, int whence);
size_t Android_JNI_FileRead(SDL_RWops* ctx, void* buffer, size_t size, size_t maxnum);
size_t Android_JNI_FileWrite(SDL_RWops* ctx, const void* buffer, size_t size, size_t num);
int Android_JNI_FileClose(SDL_RWops* ctx);

/* Clipboard support */
int Android_JNI_SetClipboardText(const char* text);
char* Android_JNI_GetClipboardText();
SDL_bool Android_JNI_HasClipboardText();

/* Power support */
int Android_JNI_GetPowerInfo(int* plugged, int* charged, int* battery, int* seconds, int* percent);
    
/* Joystick support */
void Android_JNI_PollInputDevices();

/* Video */
void Android_JNI_SuspendScreenSaver(SDL_bool suspend);

/* Touch support */
int Android_JNI_GetTouchDeviceIds(int **ids);

/* Threads */
#include <jni.h>
JNIEnv *Android_JNI_GetEnv(void);
int Android_JNI_SetupThread(void);

/* Generic messages */
int Android_JNI_SendMessage(int command, int param);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

/* vi: set ts=4 sw=4 expandtab: */
