import * as pag from 'libpag';

@Entry
@Component
struct Index {
  @State message: string = "PAG Test"
  @State composition: pag.PAGComposition | null = null;
  @State @Watch("updateMessage") progress: number = 0;
  @State @Watch("updateMessage") isPlaying: boolean = true;

  aboutToAppear(): void {
    let manager = getContext(this).resourceManager;
    let file = pag.PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.composition = file as pag.PAGComposition;
    this.isPlaying = true;
  }

  updateMessage(): void {
    this.message = `PAG isPlaying: ${this.isPlaying} progress:${this.progress.toFixed(2)}`;
  }

  build() {
    Row() {
      Column() {
        pag.PAGView({ composition: this.composition, progress: this.progress, isPlaying: this.isPlaying })
          .height('50%')
          .onClick(() => {
            this.isPlaying = !this.isPlaying;
          })

        Text(this.message)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .onClick(() => {
            this.progress = 0.5;
          })
          .height('50%')
      }
      .width('100%')

    }
    .height('100%')
  }
}
