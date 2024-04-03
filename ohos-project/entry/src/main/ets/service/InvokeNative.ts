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

import { Context } from '@ohos.abilityAccessCtrl'
import sdl from 'libSDL2d.so'
import display from '@ohos.display'


export function setScreenResolution(){
  let mDisplay: display.Display
  try {
    mDisplay = display.getDefaultDisplaySync()
    let nDeviceWidth:number = mDisplay.width
    let nDeviceHeight:number = mDisplay.height

    let nsurfaceWidth:number = mDisplay.width
    let nsurfaceHeight:number = mDisplay.height
    let format:number = mDisplay.densityPixels
    let rate:number = mDisplay.refreshRate

    sdl.nativeSetScreenResolution(nDeviceWidth,nDeviceHeight,nsurfaceWidth,nsurfaceHeight,format,rate)
  }
  catch (exception) {
    console.error('Failed to obtain the default display object. Code: ' + JSON.stringify(exception))
  }
}

export function onNativeSoftReturnKey() {
}

export function nativeSendQuit() {
  sdl.nativeSendQuit()
}

export function onNativeResize() {
  sdl.onNativeResize()
}

export function onNativeKeyboardFocusLost() {
  sdl.onNativeKeyboardFocusLost()
}

export function nativePause() {
  sdl.nativePause()
}

export function nativeResume() {
  sdl.nativeResume()
}

export function onNativeOrientationChanged(orientation: number) {
  sdl.onNativeOrientationChanged(orientation)
}

export function onNativePermissionResult(result: Boolean) {
  sdl.nativePermissionResult(result)
}

export function setResourceManager(manager: Context) {
  sdl.setResourceManager(manager.cacheDir, manager.resourceManager)
}

export function onNativeFocusChanged(focus: Boolean) {
  sdl.onNativeFocusChanged(focus)
}

export function onNativeKeyDown(keyCode: number) {
  sdl.keyDown(keyCode)
}

export function onNativeTextInput(count :number, textcontent: string) {
  sdl.textInput(count, textcontent)
}