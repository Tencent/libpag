import { PAGFile, PAGPlayer, PAGSurface } from 'libpag';
import { displaySync } from '@kit.ArkGraphics2D';

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
  private animator: ValueAnimator|undefined;

  private initPlayer() {
    let file = PAGFile.LoadFromAssets(getContext(this).resourceManager, "PAG_LOGO.pag");
    let surface = PAGSurface.FromSurfaceID(Number.parseInt(this.surfaceId));
    this.player.setSurface(surface);
    this.player.setComposition(file);
    this.player.prepare();
    this.isPlaying = true;
    this.animator = new ValueAnimator(0, 1, this.player.duration());
    this.animator.addAnimatorListener({
      onAnimationStart: () => {
        this.stateString = 'PAG start';
      },
      onAnimationEnd: () => {
        this.stateString = `PAG end`;
      },
      onAnimationCancel: () => {
        this.stateString = `PAG cancel`;
      },
      onAnimationRepeat: () => {
        this.stateString = `PAG repeat`;
      },
      onAnimationPause: () => {
      },
      onAnimationResume: () => {
      },
    });
    this.animator.addUpdateListener((progress:number) => {
      this.progress = progress;
      this.player.setProgress(progress);
      this.player.flush();
    });
    this.animator.setRepeatCount(1);
    this.animator.setDuration(this.player.duration());
    this.animator.start();
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
            // this.animator?.setProgress(0.5);
          })
          .height('50%')
      }
      .width('100%')

    }
    .height('100%')
  }
}

interface AnimatorListener {
  onAnimationStart?: () => void;
  onAnimationEnd?: () => void;
  onAnimationCancel?: () => void;
  onAnimationRepeat?: () => void;
  onAnimationPause?: () => void;
  onAnimationResume?: () => void;
};

class ValueAnimator {
  private duration: number;
  private startValue: number;
  private endValue: number;
  private startTime: number = 0;
  private interpolator: (input: number) => number;
  private updateListener?: (animatedValue: number) => void;
  private animatorListener?: AnimatorListener;

  private repeatCount: number = 0;
  private currentRepeat: number = 0;
  private running: boolean = false;

  backDisplaySync: displaySync.DisplaySync = displaySync.create();

  constructor(
    startValue: number,
    endValue: number,
    duration: number,
    interpolator: (input: number) => number = t => t // Linear interpolator by default
  ) {
    this.startValue = startValue;
    this.endValue = endValue;
    this.duration = duration;
    this.interpolator = interpolator;
  }

  setDuration(duration: number): void {
    this.duration = duration * 1000;
  }

  addUpdateListener(listener: (progress:number) => void): void {
    this.updateListener = listener;
  }

  addAnimatorListener(listener: AnimatorListener): void {
    this.animatorListener = listener;
  }

  setRepeatCount(count: number): void {
    this.repeatCount = count;
  }

  get isRunning(): boolean {
    return this.running;
  }

  start(): void {
    if (this.animatorListener?.onAnimationStart) {
      this.animatorListener.onAnimationStart();
    }
    this.currentRepeat = 0;
    this.startTime = 0;

    let range : ExpectedFrameRateRange = {
      expected: 60,
      min:0,
      max:120
    };
    this.backDisplaySync?.setExpectedFrameRateRange(range)

    this.backDisplaySync?.on("frame", this.animate.bind(this));
    // 在当前UI上下文中执行DisplaySync的start接口
    this.backDisplaySync?.start();
    this.running = true;
  }

  cancel(): void {
    if (this.animatorListener?.onAnimationCancel) {
      this.animatorListener.onAnimationCancel();
    }
    this.backDisplaySync.stop();
    this.backDisplaySync.off("frame", this.animate.bind(this));
    this.startTime = 0;
  }

  private animate(frameInfo: displaySync.IntervalInfo): void {
    if (this.startTime === 0) {
      this.startTime = frameInfo.timestamp;
      return;
    }
    const elapsed = frameInfo.timestamp - this.startTime;
    const t = Math.min(elapsed / this.duration, 1);
    const interpolatedTime = this.interpolator(t);
    const animatedValue = this.startValue + (this.endValue - this.startValue) * interpolatedTime;

    if (this.updateListener) {
      this.updateListener(animatedValue);
    }
    if (t > 1) {
      if (this.currentRepeat < this.repeatCount) {
        this.currentRepeat++;
        if (this.animatorListener?.onAnimationRepeat) {
          this.animatorListener.onAnimationRepeat();
        }
        this.startTime = 0;
      } else {
        if (this.animatorListener?.onAnimationEnd) {
          this.animatorListener.onAnimationEnd();
        }
        this.backDisplaySync.off("frame", this.animate.bind(this));
      }
    }
  }
}
