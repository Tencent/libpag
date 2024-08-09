import { PAGAnimator, PAGFile, PAGPlayer, PAGSurface } from 'libpag';

@Entry
@Component
struct Surface {
  @State message: string = "";
  @State @Watch("updateMessage") stateString: string = "";
  @State @Watch("updateMessage") progress: number = 0;
  private player: PAGPlayer = new PAGPlayer();
  @State isPlaying: boolean = false;
  @State repeatCount: number = 1;
  private surfaceId: string = '';
  private xComponentController: XComponentController = new XComponentController();
  private animator: PAGAnimator|undefined;

  private initPlayer() {
    let file = PAGFile.LoadFromAssets(getContext(this).resourceManager, "PAG_LOGO.pag");
    let surface = PAGSurface.MakeSurface(Number.parseInt(this.surfaceId), file!.width(), file!.height());
    this.player.setSurface(surface);
    this.player.setComposition(file);
    this.player.prepare();
    this.isPlaying = true;
    this.animator = new PAGAnimator();
    this.animator.addListener(new WeakRef(this));
    this.animator.setRepeatCount(1);
    this.animator.setDuration(this.player.duration());
    this.animator.start();
  }

  onAnimationStart = (animator: PAGAnimator) => {
    this.stateString = 'PAG start';
  }
  onAnimationEnd = (animator: PAGAnimator) => {
    this.stateString = `PAG end`;
  }
  onAnimationRepeat = (animator: PAGAnimator) => {
    this.stateString = `PAG repeat`;
  }
  onAnimationCancel = (animator: PAGAnimator) => {
    this.stateString = `PAG cancel`;
  }
  onAnimationUpdate = (animator: PAGAnimator) => {
    this.progress = animator.progress();
    this.player.setProgress(this.progress);
    this.player.flush();
  }

  updateMessage() {
    this.message = this.stateString + ` progress ${this.progress.toFixed(2)}`;
  }

  build() {
    Row() {
      Column() {
        XComponent({
          id: '',
          type: XComponentType.SURFACE,
          controller: this.xComponentController
        })
          .onLoad(() => {
            this.surfaceId = this.xComponentController.getXComponentSurfaceId();
            this.initPlayer();
          })
          .height('50%')
          .backgroundColor(Color.Transparent)
          .onClick(() => {
            this.isPlaying = !this.isPlaying
            if (this.isPlaying) {
              this.animator?.start();
            } else {
              this.animator?.cancel();
            }
          })

        Text(this.message)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .onClick(() => {
            this.animator?.setProgress(0.5);
          })
          .height('50%')
      }
      .width('100%')

    }
    .height('100%')
  }
}
