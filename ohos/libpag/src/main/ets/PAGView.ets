import { JsPAGView } from 'liblibpag.so'
import { PAGComposition } from './PAGComposition';

export interface PAGViewListener {
  onAnimationStart?: (view: PAGView) => void;
  onAnimationEnd?: (view: PAGView) => void;
  onAnimationRepeat?: (view: PAGView) => void;
  onAnimationCancel?: (view: PAGView) => void;
  onAnimationUpdate?: (view: PAGView) => void;
}


@Component
export struct PAGView {
  @Link @Watch("onCompositionChange") composition: PAGComposition | null;
  @Link @Watch("onPlayStateChange") isPlaying: boolean;
  @Link @Watch("onProgressChange") progress: number;
  @Prop @Watch("onRepeatCountChange") repeatCount: number = 0;
  @Prop listener: PAGViewListener | null;

  build() {
    XComponent({
      id: this.viewController.uniqueID(),
      type: "surface",
      libraryname: "libpag",
    })
      .onLoad(() => {
        this.viewController.setComposition(this.composition?.nativeComposition);
        this.viewController.setProgress(this.progress);
        this.viewController.setRepeatCount(this.repeatCount)
        if (this.isPlaying) {
          this.viewController.play();
        } else {
          this.viewController.flush();
        }
      })
      .onDestroy(() => {
      })
      .backgroundColor(Color.Transparent)
  }

  aboutToAppear(): void {
    this.viewController.setStateChangeCallback(this.onAnimatorStateChange);
    this.viewController.setProgressUpdateCallback(this.onAnimatorProgressUpdate);
  }

  onPageShow(): void {
    this.flush();
  }

  onCompositionChange() {
    this.viewController.setComposition(this.composition?.nativeComposition);
    if (!this.isPlaying) {
      this.flush()
    }
  }

  onProgressChange() {
    if (this.progress == this.nativeProgress) {
      return;
    }
    this.viewController.setProgress(this.progress);
    if (!this.isPlaying) {
      this.flush()
    }
  }

  onPlayStateChange() {
    if (this.isPlaying) {
      this.viewController.play();
    } else {
      this.viewController.pause();
    }
  }

  onRepeatCountChange() {
    this.viewController.setRepeatCount(this.repeatCount);
  }

  flush(): void {
    this.viewController.flush();
  }

  private nativeProgress: number = 0.0;
  private viewController: JsPAGView = new JsPAGView();
  private onAnimatorStateChange = (state: number): void => {
    switch (state) {
      case 0:
        if (this.listener?.onAnimationStart) {
          this.listener.onAnimationStart(this);
        }
        this.isPlaying = true;
        break;
      case 1:
        if (this.listener?.onAnimationCancel) {
          this.listener.onAnimationCancel(this);
        }
        this.isPlaying = false;
        break;
      case 2:
        if (this.listener?.onAnimationEnd) {
          this.listener.onAnimationEnd(this);
        }
        this.isPlaying = false;
        break;
      case 3:
        if (this.listener?.onAnimationRepeat) {
          this.listener.onAnimationRepeat(this);
        }
        break;
    }
  }
  private onAnimatorProgressUpdate = (progress: number): void => {
    this.nativeProgress = progress;
    this.progress = progress;
    if (this.listener?.onAnimationUpdate) {
      this.listener.onAnimationUpdate(this);
    }
  }
}