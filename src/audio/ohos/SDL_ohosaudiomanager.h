/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License,Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SDL_OHOSAUDIOMANAGER_H
#define SDL_OHOSAUDIOMANAGER_H

#include "../../SDL_internal.h"
#include "SDL_audio.h"
#include "../SDL_sysaudio.h"

void OHOSAUDIO_PageResume(void);
void OHOSAUDIO_PagePause(void);

/* Audio support */
extern int OHOSAUDIO_NATIVE_OpenAudioDevice(int iscapture, SDL_AudioSpec *spec);
extern void* OHOSAUDIO_NATIVE_GetAudioBuf(SDL_AudioDevice *device);
extern void OHOSAUDIO_NATIVE_WriteAudioBuf(void);
extern int OHOSAUDIO_NATIVE_CaptureAudioBuffer(void *buffer, int buflen);
extern void OHOSAUDIO_NATIVE_FlushCapturedAudio(void);
extern void OHOSAUDIO_NATIVE_CloseAudioDevice(const int iscapture);

#endif /* SDL_OHOSAUDIOMANAGER_H */

/* vi: set ts=4 sw=4 expandtab: */
