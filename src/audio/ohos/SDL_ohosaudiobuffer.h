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
#ifndef SDL_OHOSAUDIOBUFFER_H
#define SDL_OHOSAUDIOBUFFER_H

int OHOS_AUDIOBUFFER_InitCapture(unsigned int bufferSize);
int OHOS_AUDIOBUFFER_DeInitCapture(void);
int OHOS_AUDIOBUFFER_ReadCaptureBuffer(unsigned char *buffer, unsigned int size);
int OHOS_AUDIOBUFFER_WriteCaptureBuffer(unsigned char *buffer, unsigned int size);
void OHOS_AUDIOBUFFER_FlushBuffer(void);

#endif /* SDL_OHOSAUDIOBUFFER_H */

/* vi: set ts=4 sw=4 expandtab: */
