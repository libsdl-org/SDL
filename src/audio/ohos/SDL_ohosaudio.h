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
#ifndef SDL_OHOSAUDIO_H
#define SDL_OHOSAUDIO_H

#include "../../SDL_internal.h"
#include "../SDL_sysaudio.h"

struct SDL_PrivateAudioData {
    /* Resume device if it was paused automatically */
    int resume;
};

void OHOSAUDIO_ResumeDevices(void);
void OHOSAUDIO_PauseDevices(void);
void OHOSAUDIO_PageResume(void);
void OHOSAUDIO_PagePause(void);

#endif /* SDL_OHOSAUDIO_H */

/* vi: set ts=4 sw=4 expandtab: */
