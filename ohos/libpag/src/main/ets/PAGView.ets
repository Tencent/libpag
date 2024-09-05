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

import { JPAGView } from 'libpag.so'
import { PAGComposition } from './PAGComposition';
import { PAGScaleMode } from './PAGScaleMode';
import { PAGLayer } from './PAGLayer';
import { image } from '@kit.ImageKit';
import { PAGUtils } from './private/PAGUtils';
import { Matrix4 } from '@ohos.arkui.node';
import { PAGFile } from './PAGFile';

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
   * Set true to make PAGView playing in the main thread. The default value is false.
   */
  @Prop @Watch("OnSyncChange") isSync: boolean = false;
  /**
   * If set to false, PAGView skips rendering for video composition.
   */
  @Prop @Watch("OnVideoEnabledChange") videoEnabled: boolean = true;
  /**
   * If set to true, PAG renderer caches an internal bitmap representation of the static content
   * for each layer. This caching can increase performance for layers that contain complex vector
   * content. The execution speed can be significantly faster depending on the complexity of the
   * content, but it requires extra graphics memory. The default value is true.
   */
  @Prop @Watch("OnCacheEnabledChange") cacheEnabled: boolean = true;
  /**
   * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
   * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of
   * graphics memory which leads to better performance. The default value is 1.0.
   */
  @Prop @Watch("OnCacheScaleChange") cacheScale: number = 1.0;
  /**
   * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
   * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
   * value is 60.
   */
  @Prop @Watch("OnMaxFrameRateChange") maxFrameRate: number = 60.0;
  /**
   * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
   * changes when this method is called.
   */
  @Prop @Watch("OnScaleModeChange") scaleMode: PAGScaleMode = PAGScaleMode.LetterBox;
  /**
   * Sets the transformation which will be applied to the composition. The scaleMode property
   * will be set to PAGScaleMode::None when this method is called.
   */
  @Prop @Watch("OnMatrixChange") matrix: Matrix4 = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  /**
   * listener will be sent events through the life of an animation,
   * such as start, repeat, and end. PAGView only holds a weak reference to the listener.
   */
  @State listeners: Array<WeakRef<PAGViewListener>> = [];

  private filePath: string = "";

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

  /**
   * The path string of a pag file set by setPath() or setPathAsync().
   * @returns
   */
  getPath():string {
    return this.filePath;
  }

  /**
   * Loads a pag file from the local path, returns false if the file does not exist or the data is not a pag file.
   * @param path
   * @returns
   */
  setPath(path: string): boolean {
    this.filePath = path;
    let file = PAGFile.LoadFromPath(path);
    this.composition = file;
    return file != null;
 }

 /**
  * Asynchronously load a pag file from the specific path which can be a network path or a local path.
  * @param path
  * @returns
  */
  setPathAsync(path: string): Promise<PAGFile | null> {
     this.filePath = path;
     return new Promise<PAGFile | null>((resolve) => {
       PAGFile.LoadAsyncFromPath(path).then((pagFile) => {
         this.composition = pagFile;
         resolve(pagFile)
       })
     })
  }

  flush(): void {
    this.controller?.flush();
  }

  /**
   * Adds a listener to the set of listeners that are sent events through the life of an
   * animation, such as start, repeat, and end.
   */
  addListener(listener: WeakRef<PAGViewListener>) {
    this.listeners.push(listener);
  }

  /**
   * Removes a listener from the set listening to this animation.
   */
  removeListener(listener: WeakRef<PAGViewListener>) {
    const index = this.listeners.indexOf(listener);
    if (index > -1) {
      this.listeners.splice(index, 1);
    }
  }

  /**
   * Returns the current frame.
   */
  currentFrame(): number {
    return this.composition ? this.composition.currentTime() : 0;
  }

  /**
   * Returns an array of layers that lie under the specified point.
   * The point is in pixels not dp.
   */
  getLayersUnderPoint(x: number, y: number): Array<PAGLayer> {
    return this.controller ? PAGUtils.WarpJPAGLayers(this.controller.getLayersUnderPoint(x, y)) : [];
  }

  /**
   * Free the cache created by the pag view immediately. Can be called to reduce memory pressure.
   */
  freeCache(): void {
    this.controller?.freeCache();
  }

  /**
   * Returns a bitmap capturing the contents of the PAGView. Subsequent rendering of the
   * PAGView will not be captured. Returns null if the PAGView hasn't been presented yet.
   */
  makeSnapshot(): image.PixelMap | null {
    return this.controller ? this.controller.makeSnapshot() : null;
  }

  onPageShow(): void {
    if (super.onPageShow) {
      super.onPageShow();
    }
    this.flush();
  }

  aboutToAppear(): void {
    this.initView();
  }

  aboutToDisappear(): void {
    this.controller?.release();
    this.controller = null;
  }

  private initView() {
    this.controller?.setStateChangeCallback(this.onAnimatorStateChange);
    this.controller?.setProgressUpdateCallback(this.onAnimatorProgressUpdate);
    this.controller?.setComposition(this.composition?.getNativeComposition());
    this.controller?.setProgress(this.progress);
    this.controller?.setRepeatCount(this.repeatCount)
    this.controller?.setSync(this.isSync);
    this.controller?.setVideoEnabled(this.videoEnabled);
    this.controller?.setCacheEnabled(this.cacheEnabled);
    this.controller?.setCacheScale(this.cacheScale);
    this.controller?.setMaxFrameRate(this.maxFrameRate);
    if (PAGUtils.IsEmptyMatrix(this.matrix)) {
      this.controller?.setScaleMode(this.scaleMode);
    } else {
      this.controller?.setMatrix(PAGUtils.ToNativeMatrix(this.matrix));
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

  private onProgressChange() {
    if (this.progress == this.nativeProgress) {
      return;
    }
    this.controller?.setProgress(this.progress);
    if (!this.isPlaying) {
      this.flush()
    }
  }

  private OnSyncChange() {
    this.controller?.setSync(this.isSync);
  }

  private OnVideoEnabledChange() {
    this.controller?.setVideoEnabled(this.videoEnabled);
  }

  private OnCacheEnabledChange() {
    this.controller?.setCacheEnabled(this.cacheEnabled);
  }

  private OnCacheScaleChange() {
    this.controller?.setCacheScale(this.cacheScale);
  }

  private OnMaxFrameRateChange() {
    this.controller?.setMaxFrameRate(this.maxFrameRate);
  }

  private OnScaleModeChange() {
    this.controller?.setScaleMode(this.scaleMode);
  }

  private OnMatrixChange() {
    this.controller?.setMatrix(PAGUtils.ToNativeMatrix(this.matrix))
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
  private controller: JPAGView | null = new JPAGView();
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

enum PAGAnimatorState {
  Start = 0,
  Cancel = 1,
  End = 2,
  Repeat = 3
}