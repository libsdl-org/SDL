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

#ifndef SDL_OHOS_H
#define SDL_OHOS_H
#include "SDL_ohosfile.h"
#include <EGL/eglplatform.h>
#include "SDL_system.h"
#include "SDL_audio.h"
#include "SDL_rect.h"
#include "SDL_video.h"
#include "../../SDL_internal.h"
#include "napi/native_api.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct {
    int x;
    int y;
    int width;
    int height;
} WindowPosition;

extern SDL_DisplayOrientation displayOrientation;
extern SDL_atomic_t bPermissionRequestPending;
extern SDL_bool bPermissionRequestResult;
extern int g_windowId;

/* Cursor support */
extern int OHOS_CreateCustomCursor(SDL_Surface *xcomponent, int hotX, int hotY);
extern SDL_bool OHOS_SetCustomCursor(int cursorID);
extern SDL_bool OHOS_SetSystemCursor(int cursorID);

/* Relative mouse support */
extern SDL_bool OHOS_SupportsRelativeMouse(void);
extern SDL_bool OHOS_SetRelativeMouseEnabled(SDL_bool enabled);

extern void OHOS_PAGEMUTEX_Lock()(void);
extern void OHOS_PAGEMUTEX_Unlock(void);
extern void OHOS_PAGEMUTEX_LockRunning(void);

extern void OHOS_SetDisplayOrientation(int orientation);
extern SDL_DisplayOrientation OHOS_GetDisplayOrientation();
extern void OHOS_NAPI_ShowTextInput(int x, int y, int w, int h);
extern SDL_bool OHOS_NAPI_RequestPermission(const char *Permission);
extern void OHOS_NAPI_HideTextInput(int a);
extern void OHOS_NAPI_ShouldMinimizeOnFocusLoss(int a);
extern void OHOS_NAPI_SetTitle(const char *title);
extern void OHOS_NAPI_SetWindowStyle(SDL_bool fullscreen);
extern void OHOS_NAPI_SetOrientation(int w, int h, int resizable, const char *hint);
extern void OHOS_NAPI_ShowTextInputKeyboard(SDL_bool fullscreen);
extern void OHOS_NAPI_SetWindowResize(int x, int y, int w, int h);
extern int OHOS_NAPI_GetWindowId();

extern void OHOS_GetRootNode(int windowId, napi_ref *rootRef);
extern char* OHOS_GetXComponentId(napi_ref nodeRef);
extern void OHOS_AddChildNode(napi_ref nodeRef, napi_ref *childRef, WindowPosition *windowPosition);
extern bool OHOS_RemoveChildNode(napi_ref nodeChildRef);
extern bool OHOS_ResizeNode(napi_ref nodeRef, int w, int h);
extern bool OHOS_ReParentNode(napi_ref nodeParentNewRef, napi_ref nodeChildRef);
extern bool OHOS_MoveNode(napi_ref nodeRef, int x, int y);
/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* SDL_OHOS_H */

/* vi: set ts=4 sw=4 expandtab: */
