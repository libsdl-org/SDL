/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
import {
  hideTextInput,
  requestPermission,
  setOrientation,
  setTitle,
  setWindowStyle,
  shouldMinimizeOnFocusLoss,
  showTextInput,
  showTextInputKeyboard,
  setPointer
} from '../service/ResponseNative'

import {
  setResourceManager,
  nativePause,
  nativeResume,
  onNativeFocusChanged,
  nativeSendQuit,
  onNativeKeyDown,
  onNativeTextInput,
  setScreenResolution
} from '../service/InvokeNative'

import pointer from '@ohos.multimodalInput.pointer'
import sdl from 'libSDL2d.so'
import common from '@ohos.app.ability.common'
import window from '@ohos.window'
import { BusinessError } from '@ohos.base'
import { Context } from '@ohos.abilityAccessCtrl'
import image from '@ohos.multimedia.image'

export let sdlPageContext: common.UIAbilityContext

export interface NapiCallback {
  setTitle(title: string): void;

  setWindowStyle(fullscree: boolean): void;

  setOrientation(w: number, h: number, resizable: number, hint: string): void;

  shouldMinimizeOnFocusLoss(): void;

  showTextInput(x: number, y: number, w: number, h: number): void;

  hideTextInput(): void;

  requestPermission(permission: string): void;

  setPointer(cursorID: number): void;

  setCustomCursorandCreate(pixelmapCreated: image.PixelMap, focusX: number, focusY: number): void;
}

export class ArkNapiCallback implements NapiCallback {
  setCustomCursorandCreate(pixelmapCreated: image.PixelMap, focusX: number, focusY: number) {
    window.getLastWindow(sdlPageContext, (error: BusinessError, windowClass: window.Window) => {
      if (error.code) {
        console.error('Failed to obtain the top window. Cause: ' + JSON.stringify(error))
        return
      }
      let windowId = windowClass.getWindowProperties().id
      if (windowId < 0) {
        console.log(`Invalid windowId`)
        return
      }
      try {
        pointer.setCustomCursor(windowId, pixelmapCreated, focusX, focusY).then(() => {
          console.log(`Successfully set custom pointer`)
        }).catch(error => {
          console.log('Failed to set the custom pointer, error=' + error)
        })
      } catch (error) {
        console.log(`Failed to set the custom pointer, error=${JSON.stringify(error)}, msg=${JSON.stringify(`message`)}`)
      }
    })
  }

  setPointer(cursorID: number) {
    const SDL_SYSTEM_CURSOR_NONE = -1
    const SDL_SYSTEM_CURSOR_ARROW = 0
    const SDL_SYSTEM_CURSOR_IBEAM = 1
    const SDL_SYSTEM_CURSOR_WAIT = 2
    const SDL_SYSTEM_CURSOR_CROSSHAIR = 3
    const SDL_SYSTEM_CURSOR_WAITARROW = 4
    const SDL_SYSTEM_CURSOR_SIZENWSE = 5
    const SDL_SYSTEM_CURSOR_SIZENESW = 6
    const SDL_SYSTEM_CURSOR_SIZEWE = 7
    const SDL_SYSTEM_CURSOR_SIZENS = 8
    const SDL_SYSTEM_CURSOR_SIZEALL = 9
    const SDL_SYSTEM_CURSOR_NO = 10
    const SDL_SYSTEM_CURSOR_HAND = 11

    let cursor_type = 0

    switch (cursorID) {
      case SDL_SYSTEM_CURSOR_ARROW:
        cursor_type = pointer.PointerStyle.DEFAULT //PointerIcon.TYPE_ARROW
        break
      case SDL_SYSTEM_CURSOR_IBEAM:
        cursor_type = pointer.PointerStyle.TEXT_CURSOR //PointerIcon.TYPE_TEXT
        break
      case SDL_SYSTEM_CURSOR_WAIT:
        cursor_type = pointer.PointerStyle.DEFAULT //PointerIcon.TYPE_WAIT
        break
      case SDL_SYSTEM_CURSOR_CROSSHAIR:
        cursor_type = pointer.PointerStyle.DEFAULT //PointerIcon.TYPE_CROSSHAIR
        break
      case SDL_SYSTEM_CURSOR_WAITARROW:
        cursor_type = pointer.PointerStyle.DEFAULT //PointerIcon.TYPE_WAIT
        break
      case SDL_SYSTEM_CURSOR_SIZENWSE:
        cursor_type = pointer.PointerStyle.DEFAULT //PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW
        break
      case SDL_SYSTEM_CURSOR_SIZENESW:
        cursor_type = pointer.PointerStyle.DEFAULT //PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW
        break
      case SDL_SYSTEM_CURSOR_SIZEWE:
        cursor_type = pointer.PointerStyle.HORIZONTAL_TEXT_CURSOR //PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW
        break
      case SDL_SYSTEM_CURSOR_SIZENS:
        cursor_type = pointer.PointerStyle.DEFAULT //PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW
        break
      case SDL_SYSTEM_CURSOR_SIZEALL:
        cursor_type = pointer.PointerStyle.HAND_GRABBING //PointerIcon.TYPE_GRAB
        break
      case SDL_SYSTEM_CURSOR_NO:
        cursor_type = pointer.PointerStyle.DEFAULT //PointerIcon.TYPE_NO_DROP
        break
      case SDL_SYSTEM_CURSOR_HAND:
        cursor_type = pointer.PointerStyle.HAND_OPEN //PointerIcon.TYPE_HAND
        break
      default:
        cursor_type = pointer.PointerStyle.DEFAULT
        break
    }

    window.getLastWindow(sdlPageContext, (error: BusinessError, windowClass: window.Window) => {
      if (error.code) {
        console.error('Failed to obtain the top window. Cause: ' + JSON.stringify(error))
        return
      }
      let windowId = windowClass.getWindowProperties().id
      if (windowId < 0) {
        console.log(`Invalid windowId`)
        return
      }

      try {
        pointer.setPointerStyle(windowId, cursor_type).then(() => {
          console.log(`Successfully set mouse pointer style`)
        })
      } catch (error) {
        console.log(`Failed to set the pointer style, error=${JSON.stringify(error)}, msg=${JSON.stringify(`message`)}`)
      }
    })
  }

  showTextInputKeyboard(isshow: boolean): void {

  }

  setTitle(title: string): void {
    setTitle(title)
  }

  setWindowStyle(fullscree: boolean): void {

  }

  setOrientation(w: number, h: number, resizable: number, hint: string): void {
    setOrientation(sdlPageContext, w, h, resizable, hint)
  }

  shouldMinimizeOnFocusLoss(): void {
    shouldMinimizeOnFocusLoss()
  }

  showTextInput(x: number, y: number, w: number, h: number): void {
    showTextInput(x, y, w, h)
  }

  hideTextInput(): void {
    hideTextInput()
  }

  requestPermission(permission: string): void {
    requestPermission(sdlPageContext, permission)
  }

  nAPISetWindowResize(x: number, y: number, w: number, h: number): void {

  }

}

let callbackRef: NapiCallback = new ArkNapiCallback()

function setWindow(windowStage: window.WindowStage,fullscreen:boolean) {
  windowStage.getMainWindow().then((win: window.Window) => {
    win.setWindowLayoutFullScreen(fullscreen)
    try {
      const avoidArea = win.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM)
      if (avoidArea) {
      }
      win.on('avoidAreaChange', (data) => {
      })
    } catch (err) {

    }
  })
}

enum NativeState {
  INIT,
  RESUMED,
  PAUSED
}

let currentNativeState: NativeState
let nextNativeState: NativeState
let isResumedCalled: Boolean

function handleNativeState(): void {

  if (nextNativeState == currentNativeState) {
  return
}

if (nextNativeState == NativeState.INIT) {

  currentNativeState = nextNativeState
  return
}

if (nextNativeState == NativeState.PAUSED) {
  nativePause()
  currentNativeState = nextNativeState
  return
}

// Try a transition to resumed state
if (nextNativeState == NativeState.RESUMED) {
  sdl.init(callbackRef)
  sdl.sdlAppEntry("libentry.so", "main")
  currentNativeState = nextNativeState
}
}

function resumeNativeThread(): void {
  nextNativeState = NativeState.RESUMED
  isResumedCalled = true
  handleNativeState()
}

function pauseNativeThread(): void {
  nextNativeState = NativeState.PAUSED
  isResumedCalled = false
  handleNativeState()
}

export function notifySdlPageShow(): void {
  onNativeFocusChanged(true)
  resumeNativeThread()
}

export function notifySdlPageHide(): void {
  onNativeFocusChanged(false)
  pauseNativeThread()
}

export function notifySdlAboutToAppear(context: common.UIAbilityContext, resourceMannagerContext: Context): void {
  sdlPageContext = context;
  setResourceManager(resourceMannagerContext)
  setScreenResolution()
}

export function notifySdlAboutToDisappear() {
  nativeSendQuit()
}

export function notifySdlKeyDown(keyCode: number) {
  onNativeKeyDown(keyCode)
}

export function notifySdlTextInput(count :number, textcontent: string) {
  onNativeTextInput(count, textcontent)
}
