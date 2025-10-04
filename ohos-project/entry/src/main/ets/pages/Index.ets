import { hilog } from '@kit.PerformanceAnalysisKit';
import sdltest from 'libSDL3.so';
import { intl } from '@kit.LocalizationKit';
import { promptAction } from '@kit.ArkUI';
import { inputMethod } from '@kit.IMEKit'
import { Action, Key, KeyEvent } from '@kit.InputKit';
import { pasteboard, BusinessError } from '@kit.BasicServicesKit';

const DOMAIN = 0x0000;

let controller = inputMethod.getController()
let input = false
let targetText = ""

export class ArkNapiCallback {
  onMainLaunch() {
    sdltest.sdlLaunchMain("libentry.so", "main")
  }
  showDialog(title: string, message: string) {
    promptAction.showDialog({title: title, message: message, buttons: [{text: 'Ok', color: '#999999'}]})
  }
  fetchLocale(): string {
    let locale = new intl.Locale()
    return locale.language + "_" + locale.region
  }
  test(): number {
    hilog.info(DOMAIN, 'testTag', 'Call from native !!!')
    return 1
  }

  setPasteboardString(text: string) {
    let pasteData: pasteboard.PasteData = pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, text);
    let systemPasteboard: pasteboard.SystemPasteboard = pasteboard.getSystemPasteboard();
    systemPasteboard.setData(pasteData, (err, data) => {
      if (err) {
        hilog.error(DOMAIN, 'testTag', 'Failed to set PasteData. Cause: ' + err.message);
        return;
      }

      hilog.info(DOMAIN, 'testTag', 'Succeed in setting PasteData.');
    });
  }

  textEditing(): number {
    return input ? 1 : 0;
  }

  startTextInput() {
    input = true
    controller.showTextInput()
    hilog.info(DOMAIN, 'testTag', 'text input start')
  }
  stopTextInput() {
    input = false
    controller.hideTextInput()
    hilog.info(DOMAIN, 'testTag', 'text input stop')
    targetText = ""
  }
}

let callbackRef: ArkNapiCallback = new ArkNapiCallback()

@Entry
@Component
struct Index {
  async aboutToAppear(): Promise<void> {
    sdltest.sdlCallbackInit(callbackRef)
    focusControl.requestFocus('mainView')
    await controller.attach(true, {
      inputAttribute: {
        textInputType: inputMethod.TextInputType.TEXT,
        enterKeyType: inputMethod.EnterKeyType.DONE
      }
    });
    controller.on('insertText', (text) => {
      targetText += text;
      sdltest.sdlTextAppend(text)
    })
    controller.on('deleteLeft', (i) => {
      sdltest.sdlTextEditing(targetText, targetText.length - 1 - i, i);

      if (targetText.length > 0) {
        targetText = targetText.substring(0, targetText.length - i)
      }
      hilog.info(DOMAIN, 'testTag', targetText)
    })
    controller.on('deleteRight', (i) => {
      sdltest.sdlTextEditing(targetText, 0, i);

      if (targetText.length > 0) {
        targetText = targetText.substring(i, targetText.length)
      }
      hilog.info(DOMAIN, 'testTag', targetText)
    })
  }

  build() {
    Column() {
      XComponent({ id: 'mainView', type: 'surface', libraryname: 'SDL3' })
        .id('mainView')
    }
    .onKeyEvent((keyevent) => {
      sdltest.sdlKeyEvent(keyevent.keyCode, keyevent.type);
    })
    .alignItems(HorizontalAlign.End)
    .justifyContent(FlexAlign.End)
  }
}
