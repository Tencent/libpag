/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import { PAGComposition } from './PAGComposition';
import { PAGInit } from './PAGInit';
import { Matrix4 } from '@kit.ArkUI';
import { PAGScaleMode } from './PAGScaleMode';
import { PAGUtils } from './private/PAGUtils';
import { JPAGImageView } from 'libpag.so'
import { PAGFile } from './PAGFile';

export interface PAGImageViewListener {
  /**
   * Notifies the beginning of the animation. It can be called from either the UI thread or the thread
   * that calls the play method.
   */
  onAnimationStart?: (view: PAGImageView) => void;

  /**
   * Notifies the end of the animation. It can only be called from the UI thread.
   */
  onAnimationEnd?: (view: PAGImageView) => void;

  /**
   * Notifies the repetition of the animation. It can only be called from the UI thread.
   */
  onAnimationRepeat?: (view: PAGImageView) => void;

  /**
   * Notifies the cancellation of the animation. It can be called from either the UI thread or the
   * thread that calls the stop method.
   */
  onAnimationCancel?: (view: PAGImageView) => void;

  /**
   * Notifies another frame of the animation has occurred. It may be called from an arbitrary
   * thread if the animation is running asynchronously.
   */
  onAnimationUpdate?: (view: PAGImageView) => void;
}

@Component
export struct PAGImageView {
  /**
   * the content content of PAGView.
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
  @Link @Watch("onCurrentFrameChange") currentFrame: number;
  /**
   * The total number of times the animation is set to play. The default is 1, which means the
   * animation will play only once. If the repeat count is set to 0 or a negative value, the
   * animation will play infinity times.
   */
  @Prop @Watch("onRepeatCountChange") repeatCount: number = 0;
  /**
   * This value defines the scale factor for the size of the cached image frames, which ranges
   * from 0.0 to 1.0. A scale factor less than 1.0 may result in blurred output, but it can reduce
   * graphics memory usage, increasing the rendering performance. The default value is 1.0.
   */
  @Prop @Watch("OnRenderScaleChange") renderScale: number = 1.0;
  /**
   * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
   * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
   * value is 60.
   */
  @Prop @Watch("OnMaxFrameRateChange") maxFrameRate: number = 60.0;
  /**
   * listener will be sent events through the life of an animation,
   * such as start, repeat, and end. PAGView only holds a weak reference to the listener.
   */
  @State listeners: Array<WeakRef<PAGImageViewListener>> = [];

  /**
   * Returns the number of frames in the PAGImageView in one loop. Note that the value may change
   * if the associated PAGComposition was modified.
   */
  numFrames(): number {
    return this.controller ? this.controller.numFrames() : 0;
  }

  /**
   * Returns the number of frames in the PAGImageView in one loop. Note that the value may change
   * if the associated PAGComposition was modified.
   */
  currentImage(): PixelMap | null {
    if (this.controller) {
      return this.controller.currentImage();
    } else {
      return null;
    }
  }

  flush() {
    this.controller?.flush();
  }

  getComposition(): PAGComposition | null {
    return this.composition;
  }

  onPageShow(): void {
    if (super.onPageShow) {
      super.onPageShow();
    }
    this.flush();
  }

  private initView() {
    PAGInit.Init();
    this.controller?.setStateChangeCallback(this.onAnimatorStateChange);
    this.controller?.setProgressUpdateCallback(this.onAnimatorProgressUpdate);
    this.controller?.setComposition(this.composition?.getNativeComposition());
    this.controller?.setCurrentFrame(this.currentFrame);
    this.controller?.setRepeatCount(this.repeatCount)
    this.controller?.setRenderScale(this.renderScale);
    this.controller?.setMaxFrameRate(this.maxFrameRate);
    if (this.isPlaying) {
      this.controller?.play();
    } else {
      this.controller?.flush();
    }
  }

  private onCompositionChange() {
    this.controller?.setComposition(this.composition?.getNativeComposition());
    if (!this.isPlaying) {
      this.flush()
    }
  }

  private onPlayStateChange() {
    if (this.isPlaying) {
      this.controller?.play();
    } else {
      this.controller?.pause();
    }
  }

  private onRepeatCountChange() {
    this.controller?.setRepeatCount(this.repeatCount);
  }

  private onCurrentFrameChange() {
    if (this.currentFrame == this.nativeFrame) {
      return;
    }
    this.controller?.setCurrentFrame(this.currentFrame);
    if (!this.isPlaying) {
      this.flush()
    }
  }

  private OnRenderScaleChange() {
    this.controller?.setRenderScale(this.renderScale);
  }

  private OnMaxFrameRateChange() {
    this.controller?.setMaxFrameRate(this.maxFrameRate);
  }

  build() {
    XComponent({
      id: this.controller?.uniqueID(),
      type: XComponentType.SURFACE,
      libraryname: "pag",
    })
      .onLoad(() => {
        if (this.isPlaying) {
          this.controller?.play();
        } else {
          this.controller?.flush();
        }
      })
      .backgroundColor(Color.Transparent)
  }

  aboutToAppear(): void {
    this.controller = new JPAGImageView();
    this.initView();
  }

  aboutToDisappear(): void {
    this.controller?.release();
    this.controller = null;
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
  private controller: JPAGImageView | null = null;
  private nativeFrame: number = 0;
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
  private onAnimatorProgressUpdate = (frame: number): void => {
    this.nativeFrame = frame;
    this.currentFrame = frame;
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationUpdate) {
        listener.onAnimationUpdate(this);
      }
    }
  }
}

enum PAGAnimatorState {
  Start = 0,
  Cancel = 1,
  End = 2,
  Repeat = 3
}