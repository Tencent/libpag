import { common } from '@kit.AbilityKit';
import { PAGView } from 'libpag';
import { PAGFile } from 'libpag'
import { PAGComposition } from 'libpag/src/main/ets/PAGComposition';

@Entry
@Component
struct Index {
  @State message: string = 'PAG Test'
  @State composition: PAGComposition | null = null;
  @State progress: number = 0;
  @State isPlaying: boolean = true;

  onDidBuild(): void {
    let manager = getContext(this).resourceManager;
    let file = PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.composition = file as PAGComposition;
    this.isPlaying = true;
  }

  build() {
    Row() {
      Column() {
        PAGView({ composition: this.composition, progress: this.progress, isPlaying: this.isPlaying })
          .height('50%')

        Text(this.message)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .onClick(() => {
          })
          .height('50%')
      }
      .width('100%')

    }
    .height('100%')
  }
}
