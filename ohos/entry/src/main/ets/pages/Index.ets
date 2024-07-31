import * as pag from 'libpag';


@Entry
@Component
struct Index {
  @State message: string = "";
  @State @Watch("updateMessage") stateString: string = "";
  @State composition: pag.PAGComposition | null = null;
  @State @Watch("updateMessage") progress: number = 0;
  @State isPlaying: boolean = false;
  @State repeatCount: number = 1;

  aboutToAppear(): void {
    let manager = getContext(this).resourceManager;
    let file = pag.PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.composition = file as pag.PAGComposition;
    this.isPlaying = true;
  }

  onAnimationStart = (view: pag.PAGView) => {
    this.stateString = 'PAG start';
  }
  onAnimationEnd = (view: pag.PAGView) => {
    this.stateString = `PAG end`;
  }
  onAnimationRepeat = (view: pag.PAGView) => {
    this.stateString = `PAG repeat`;
  }
  onAnimationCancel = (view: pag.PAGView) => {
    this.stateString = `PAG cancel`;
  }
  onAnimationUpdate = (view: pag.PAGView) => {
  }

  updateMessage() {
    this.message = this.stateString + ` progress ${this.progress.toFixed(2)}`;
  }

  build() {
    Row() {
      Column() {
        pag.PAGView({
          composition: this.composition,
          progress: this.progress,
          isPlaying: this.isPlaying,
          repeatCount: this.repeatCount,
          listener: this as pag.PAGViewListener
        })
          .height('50%')
          .onClick(() => {
            this.isPlaying = !this.isPlaying
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
