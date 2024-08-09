import { JPAGAnimator } from 'libpag.so';
import { PAGInit } from './PAGInit';

export interface PAGAnimatorListener {
  /**
   * Notifies the beginning of the animation. It can be called from either the UI thread or the thread
   * that calls the play method.
   */
  onAnimationStart?: (animator: PAGAnimator) => void;

  /**
   * Notifies the end of the animation. It can only be called from the UI thread.
   */
  onAnimationEnd?: (animator: PAGAnimator) => void;

  /**
   * Notifies the repetition of the animation. It can only be called from the UI thread.
   */
  onAnimationRepeat?: (animator: PAGAnimator) => void;

  /**
   * Notifies the cancellation of the animation. It can be called from either the UI thread or the
   * thread that calls the stop method.
   */
  onAnimationCancel?: (animator: PAGAnimator) => void;

  /**
   * Notifies another frame of the animation has occurred. It may be called from an arbitrary
   * thread if the animation is running asynchronously.
   */
  onAnimationUpdate?: (animator: PAGAnimator) => void;
}

export class PAGAnimator {
  constructor() {
    PAGInit.Init();
    this.animator = new JPAGAnimator();
    this.animator.setStateChangeCallback(this.onAnimatorStateChange);
    this.animator.setProgressUpdateCallback(this.onAnimatorProgressUpdate);
  }
  private animator: JPAGAnimator;
  /**
   * listener will be sent events through the life of an animation,
   * such as start, repeat, and end. PAGView only holds a weak reference to the listener.
   */
  private listeners: Array<WeakRef<PAGAnimatorListener>> = [];

  /**
   * Adds a listener to the set of listeners that are sent events through the life of an
   * animation, such as start, repeat, and end.
   */
  addListener(listener: WeakRef<PAGAnimatorListener>) {
    this.listeners.push(listener);
  }

  /**
   * Removes a listener from the set listening to this animation.
   */
  removeListener(listener: WeakRef<PAGAnimatorListener>) {
    const index = this.listeners.indexOf(listener);
    if (index > -1) {
      this.listeners.splice(index, 1);
    }
  }

  private onAnimationStart() {
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationStart) {
        listener.onAnimationStart(this);
      }
    }
  }

  private onAnimationEnd() {
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationEnd) {
        listener.onAnimationEnd(this);
      }
    }
  }

  private onAnimationCancel() {
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

  private onAnimatorStateChange = (state: number): void => {
    switch (state) {
      case PAGAnimatorState.Start:
        return this.onAnimationStart();
      case PAGAnimatorState.Cancel:
        return this.onAnimationCancel();
      case PAGAnimatorState.End:
        return this.onAnimationEnd();
      case PAGAnimatorState.Repeat:
        return this.onAnimationRepeat();
    }
  }

  private onAnimatorProgressUpdate = (progress: number): void => {
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationUpdate) {
        listener.onAnimationUpdate(this);
      }
    }
  }

  duration(): number {
    return this.animator.duration();
  }
  setDuration(duration: number): void {
    this.animator.setDuration(duration);
  }
  setProgress(progress: number): void {
    this.animator.setProgress(progress);
  }
  progress(): number {
    return this.animator.progress();
  }
  setRepeatCount(repeatCount: number): void {
    this.animator.setRepeatCount(repeatCount);
  }
  repeatCount(): number {
    return this.animator.repeatCount();
  }
  start(): void {
    this.animator.start();
  }
  cancel(): void {
    this.animator.cancel();
  }
  update(): void {
    this.animator.update();
  }
  setSync(isSync: boolean): void {
    this.animator.setSync(isSync);
  }
  isRunning(): boolean {
    return this.animator.isRunning();
  }

}

enum PAGAnimatorState {
  Start = 0,
  Cancel = 1,
  End = 2,
  Repeat = 3
}