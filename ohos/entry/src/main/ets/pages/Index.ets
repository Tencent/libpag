import * as pag from 'libpag';


@Entry
@Component
struct Index {
  @State message: string = "PAG Test"
  @State composition: pag.PAGComposition | null = null;
  @State @Watch("updateMessage") progress: number = 0;
  @State @Watch("updateMessage") viewState: pag.PAGViewState = pag.PAGViewState.Pause;
  @State repeatCount: number = 1;

  aboutToAppear(): void {
    let manager = getContext(this).resourceManager;
    let file = pag.PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.composition = file as pag.PAGComposition;
    this.viewState = pag.PAGViewState.Play;
  }

  updateMessage(): void {
    this.message = `PAG state: ${this.viewState} progress:${this.progress.toFixed(2)}`;
  }

  build() {
    Row() {
      Column() {
        pag.PAGView({
          composition: this.composition,
          progress: this.progress,
          state: this.viewState,
          repeatCount: this.repeatCount
        })
          .height('50%')
          .onClick(() => {
            if (this.viewState == pag.PAGViewState.Play) {
              this.viewState = pag.PAGViewState.Pause;
            } else {
              this.viewState = pag.PAGViewState.Play;
            }
          })

        Text(this.message)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .onClick(() => {
            this.progress = 0.5;
            this.repeatCount = 0;
          })
          .height('50%')
      }
      .width('100%')

    }
    .height('100%')
  }
}
