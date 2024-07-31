import pag from 'liblibpag.so'
import { PAGComposition } from './PAGComposition';

export enum PAGViewState {
  Play = 0,
  Cancel,
  End,
  Pause = Cancel
}

@Component
export struct PAGView {
  @Link @Watch("onCompositionChange") composition: PAGComposition | null;
  @Link @Watch("onProgressChange") progress: number;
  @Link @Watch("onPlayStateChange") state: PAGViewState;
  @Prop @Watch("onRepeatCountChange") repeatCount: number = 0;

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
        if (this.state == PAGViewState.Play) {
          this.viewController.play();
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
    if (this.state != PAGViewState.Play) {
      this.flush()
    }
  }

  onProgressChange() {
    if (this.nativeProgress == this.progress) {
      return;
    }
    this.viewController.setProgress(this.progress);
    if (this.state != PAGViewState.Play) {
      this.flush()
    }
  }

  onPlayStateChange() {
    if (this.state == PAGViewState.Play) {
      this.viewController.play();
    } else {
      this.viewController.pause();
      if (this.state == PAGViewState.End) {
        this.progress = 1.0;
      }
    }
  }

  onRepeatCountChange() {
    this.viewController.setRepeatCount(this.repeatCount);
  }

  flush(): void {
    this.viewController.flush();
  }

  private viewController: pag.JsPAGView = new pag.JsPAGView();
  private nativeProgress: number = 0.0;
  private onAnimatorStateChange = (state: number): void => {
    this.state = state;
  };
  private onAnimatorProgressUpdate = (progress: number): void => {
    this.nativeProgress = progress;
    this.progress = progress;
  }
}