import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import hilog from '@ohos.hilog';
import UIAbility from '@ohos.app.ability.UIAbility';
import Want from '@ohos.app.ability.Want';
import window from '@ohos.window';
import resourceManager from '@ohos.resourceManager';

export default class EntryAbility extends UIAbility {
  public static resmgr : resourceManager.ResourceManager;
  public static filesDir : string;
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onCreate');
  }

  onDestroy(): void {
    console.log('ccnto on destory');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onWindowStageCreate');

    windowStage.loadContent('pages/MainWindow', (err) => {
      if (err.code) {
        hilog.error(0x0000, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err) ?? '');
        return;
      }
      hilog.info(0x0000, 'testTag', 'Succeeded in loading the content.');
    });
    hilog.info(0x0000, 'ccnto' ,this.context.filesDir);
    hilog.info(0x0000, 'ccnto' ,this.context.resourceDir);
    EntryAbility.resmgr = this.context.resourceManager;
    EntryAbility.filesDir = this.context.filesDir;
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    console.log("ccnto onForeground");
  }

  onBackground(): void {
    console.log("ccnto onBackground");
  }
};
