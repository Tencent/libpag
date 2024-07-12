import { hilog } from '@kit.PerformanceAnalysisKit';
import testNapi from 'libentry.so'
import { common } from '@kit.AbilityKit';
import fs, { ReadOptions } from '@ohos.file.fs';

@Entry
@Component
struct Index {
  @State message: string = 'PAG Test'
  build() {
    Row() {
      Column() {
        Text(this.message)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .onClick(() => {
            let context = getContext(this) as common.UIAbilityContext;
            let filesDir = context.resourceDir;
            let filePath = filesDir + '/particle_video.pag';
            if (fs.accessSync(filePath)) {
              console.info("---------- filePath exist, path:" + filePath);
            } else
            {
              console.error("---------- filePath not exist, path:" + filePath);
            }


            hilog.info(0x0000, 'testTag', 'Test NAPI 2 + 3 = %{public}d', testNapi.add(2, 3));
          })
      }
      .width('100%')
    }
    .height('100%')
  }
}
