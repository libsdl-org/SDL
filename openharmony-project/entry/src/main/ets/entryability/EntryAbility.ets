import { abilityAccessCtrl, AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { i18n } from '@kit.LocalizationKit';
import { inputMethod } from '@kit.IMEKit';
import pointer from "@ohos.multimodalInput.pointer"
import sdl from 'libSDL3.so';

const DOMAIN = 0x0000;

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, 'SDL/STARTUP', '%{public}s', 'Handing control from ArkTS to native SDL.');
    try {
        // This hands stuff to SDL's C code that it needs to deal with ArkTS interfaces. We are trying not to write scripting code, if we can
        //  handle it awkwardly in C through NAPI calls, so we have a _tiny_ surface area of risk where the script and the native code can go
        //  out of sync. This function call will add methods to `this`, such as onWindowStageCreate(), so we can drive everything from C and
        //  have almost no ArkTS code at all.
        // Which is to say: if this function call fails, your app won't actually start.
        //
        // ONLY EVER ADD PARAMETERS TO THIS FUNCTION CALL. THE C CODE WILL ABORT IF IT DOESN'T GET THE EXACT NUMBER IT EXPECTS.
        sdl.provideArkTSObjects(this, abilityAccessCtrl.createAtManager(), i18n.System, pointer.setPointerVisibleSync, inputMethod.getController());
    } catch (e) {
        hilog.info(DOMAIN, 'SDL/STARTUP', '%{public}s', 'Exception thrown! App will not start.');
    }
  }
}

