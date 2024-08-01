import { JPAGView } from 'libpag.so'
import { PAGComposition } from './PAGComposition';
import { PAGInit } from "./PAGInit"

export interface PAGViewListener {
  /**
   * Notifies the beginning of the animation. It can be called from either the UI thread or the thread
   * that calls the play method.
   */
  onAnimationStart?: (view: PAGView) => void;

  /**
   * Notifies the end of the animation. It can only be called from the UI thread.
   */
  onAnimationEnd?: (view: PAGView) => void;

  /**
   * Notifies the repetition of the animation. It can only be called from the UI thread.
   */
  onAnimationRepeat?: (view: PAGView) => void;

  /**
   * Notifies the cancellation of the animation. It can be called from either the UI thread or the
   * thread that calls the stop method.
   */
  onAnimationCancel?: (view: PAGView) => void;

  /**
   * Notifies another frame of the animation has occurred. It may be called from an arbitrary
   * thread if the animation is running asynchronously.
   */
  onAnimationUpdate?: (view: PAGView) => void;
}

@Component
export struct PAGView {
  /**
   * PAGComposition is the content of PAGView.
   * Note: If the composition is already added to another PAGView, it will be removed from the
   * previous PAGView.
   */
  @Link @Watch("onCompositionChange") composition: PAGComposition | null;
  /**
   * Indicates whether this pag view is playing.
   */
  @Link @Watch("onPlayStateChange") isPlaying: boolean;
  /**
   * The current progress of play position, the value is from 0.0 to 1.0. It is applied only
   * when the composition is not null.
   */
  @Link @Watch("onProgressChange") progress: number;
  /**
   * The total number of times the animation is set to play. The default is 1, which means the
   * animation will play only once. If the repeat count is set to 0 or a negative value, the
   * animation will play infinity times.
   */
  @Prop @Watch("onRepeatCountChange") repeatCount: number = 0;
  /**
   * listener will be sent events through the life of an animation,
   * such as start, repeat, and end. PAGView only holds a weak reference to the listener.
   */
  @State listeners: Array<WeakRef<PAGViewListener>> = [];

  build() {
    XComponent({
      id: this.viewController.uniqueID(),
      type: "surface",
      libraryname: "pag",
    })
      .onLoad(() => {
        this.initView();
      })
      .backgroundColor(Color.Transparent)
  }

  flush(): void {
    this.viewController.flush();
  }

  onPageShow(): void {
    this.flush();
  }

  private initView() {
    PAGInit.Init();
    this.viewController.setStateChangeCallback(this.onAnimatorStateChange);
    this.viewController.setProgressUpdateCallback(this.onAnimatorProgressUpdate);

    this.viewController.setComposition(this.composition?.getNativeComposition());
    this.viewController.setProgress(this.progress);
    this.viewController.setRepeatCount(this.repeatCount)
    if (this.isPlaying) {
      this.viewController.play();
    } else {
      this.viewController.flush();
    }
  }

  private onCompositionChange() {
    this.viewController.setComposition(this.composition?.getNativeComposition());
    if (!this.isPlaying) {
      this.flush()
    }
  }

  private onPlayStateChange() {
    if (this.isPlaying) {
      this.viewController.play();
    } else {
      this.viewController.pause();
    }
  }

  private onRepeatCountChange() {
    this.viewController.setRepeatCount(this.repeatCount);
  }

  private onProgressChange() {
    if (this.progress == this.nativeProgress) {
      return;
    }
    this.viewController.setProgress(this.progress);
    if (!this.isPlaying) {
      this.flush()
    }
  }

  private onAnimationStart() {
    this.isPlaying = true;
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationStart) {
        listener.onAnimationStart(this);
      }
    }
  }

  private onAnimationEnd() {
    this.isPlaying = false;
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationEnd) {
        listener.onAnimationEnd(this);
      }
    }
  }

  private onAnimationCancel() {
    this.isPlaying = false;
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationCancel) {
        listener.onAnimationCancel(this);
      }
    }
  }

  private onAnimationRepeat() {
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationRepeat) {
        listener.onAnimationRepeat(this);
      }
    }
  }

  private nativeProgress: number = 0.0;
  private viewController: JPAGView = new JPAGView();
  private onAnimatorStateChange = (state: number): void => {
    switch (state) {
      case 0:
        return this.onAnimationStart();
      case 1:
        return this.onAnimationCancel();
      case 2:
        return this.onAnimationEnd();
      case 3:
        return this.onAnimationRepeat();
    }
  }
  private onAnimatorProgressUpdate = (progress: number): void => {
    this.nativeProgress = progress;
    this.progress = progress;
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationUpdate) {
        listener.onAnimationUpdate(this);
      }
    }
  }
}