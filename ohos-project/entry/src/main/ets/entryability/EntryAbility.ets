import { abilityAccessCtrl, AbilityConstant,
  common,
  Configuration,
  ConfigurationConstant,
  Permissions,
  UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { KeyboardAvoidMode } from '@ohos.arkui.UIContext';
import { BusinessError } from '@kit.BasicServicesKit';
import sdl from 'libSDL3.so';

const DOMAIN = 0x0000;

const permissions: Array<Permissions> = ['ohos.permission.INPUT_KEYBOARD_CONTROLLER', 'ohos.permission.READ_PASTEBOARD'];
function reqPermissionsFromUser(permissions: Array<Permissions>, context: common.UIAbilityContext): void {
  let atManager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
  atManager.requestPermissionsFromUser(context, permissions).then((data) => {
    let grantStatus: Array<number> = data.authResults;
    let length: number = grantStatus.length;
    for (let i = 0; i < length; i++) {
      if (grantStatus[i] === 0) {
        // promptAction.showDialog({title: "permissions", message: "granted", buttons: [{text: 'Ok', color: '#999999'}]});
      } else {
        // promptAction.showDialog({title: "permissions", message: "not granted", buttons: [{text: 'Ok', color: '#999999'}]});
        return;
      }
    }
    // 授权成功
  }).catch((err: BusinessError) => {
    console.error(`Failed to request permissions from user. Code is ${err.code}, message is ${err.message}`);
  })
}

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    this.context.getApplicationContext().setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET);
    hilog.info(DOMAIN, 'surfaceLayer', '%{public}s', 'Ability onCreate');
  }

  onDestroy(): void {
    hilog.info(DOMAIN, 'surfaceLayer', '%{public}s', 'Ability onDestroy');
  }

  async onWindowStageCreate(windowStage: window.WindowStage): Promise<void> {
    // Main window is created, set main page for this ability
    hilog.info(DOMAIN, 'surfaceLayer', '%{public}s', 'Ability onWindowStageCreate');

    windowStage.loadContent('pages/Index', (err) => {
      reqPermissionsFromUser(permissions, this.context);

      windowStage.getMainWindow().then(async (win) => {
        try {
          win.getUIContext().setKeyboardAvoidMode(KeyboardAvoidMode.OFFSET)
        } catch (error) {
          let ll: BusinessError = error;
          hilog.error(DOMAIN, 'surfaceLayer', 'Failed to load the content. Cause: %{public}s', JSON.stringify(ll));
        }
      })

      if (err.code) {
        hilog.error(DOMAIN, 'surfaceLayer', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, 'surfaceLayer', 'Succeeded in loading the content.');
    });
  }

  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, 'surfaceLayer', '%{public}s', 'Ability onWindowStageDestroy');
    sdl.sdlOnTerminate();
  }

  onForeground(): void {
    hilog.info(DOMAIN, 'surfaceLayer', '%{public}s', 'Ability onForeground');
    sdl.sdlOnForeground();
  }

  onBackground(): void {
    hilog.info(DOMAIN, 'surfaceLayer', '%{public}s', 'Ability onBackground');
    sdl.sdlOnBackground();
  }

  onMemoryLevel(level: AbilityConstant.MemoryLevel): void {
    if (level == AbilityConstant.MemoryLevel.MEMORY_LEVEL_LOW || level == AbilityConstant.MemoryLevel.MEMORY_LEVEL_CRITICAL) {
      sdl.sdlOnLowMemory();
    }
  }

  onConfigurationUpdate(newConfig: Configuration): void {
    sdl.sdlOnConfigUpdate();
  }
}
