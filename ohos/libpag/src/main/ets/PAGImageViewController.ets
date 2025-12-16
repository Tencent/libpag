/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

import { Matrix4 } from '@ohos.arkui.node';
import { JPAGImageView } from 'libpag.so'
import { PAGComposition } from './PAGComposition';
import { PAGScaleMode } from './PAGScaleMode';
import { PAGFile } from './PAGFile';
import { PAGAnimatorState } from './private/PAGAnimatorState';
import { PAGUtils } from './private/PAGUtils';
import { PAGViewCallbackHandler } from './private/PAGViewCallbackHandler'

export interface PAGImageViewListener {
  /**
   * Notifies the beginning of the animation. It can be called from either the UI thread or the thread
   * that calls the play method.
   */
  onAnimationStart?: (view: PAGImageViewController) => void;

  /**
   * Notifies the end of the animation. It can only be called from the UI thread.
   */
  onAnimationEnd?: (view: PAGImageViewController) => void;

  /**
   * Notifies the repetition of the animation. It can only be called from the UI thread.
   */
  onAnimationRepeat?: (view: PAGImageViewController) => void;

  /**
   * Notifies the cancellation of the animation. It can be called from either the UI thread or the
   * thread that calls the stop method.
   */
  onAnimationCancel?: (view: PAGImageViewController) => void;

  /**
   * Notifies another frame of the animation has occurred. It may be called from an arbitrary
   * thread if the animation is running asynchronously.
   */
  onAnimationUpdate?: (view: PAGImageViewController) => void;
}

export class PAGImageViewController {
  /**
   * Returns the current PAGComposition in the PAGImageView. Returns null if the internal
   * composition was loaded from a pag file path.
   */
  getComposition(): PAGComposition | null {
    return this.filePath != null ? null : this.composition;
  }

  /**
   * Sets a new PAGComposition and the maxFrameRate limit to the PAGImageView. Note: If the
   * composition is already added to another PAGImageView, it will be removed from the previous
   * PAGImageView.
   */
  setComposition(composition: PAGComposition | null, maxFrameRate: number = 30.0) {
    this.filePath = null;
    this.composition = composition;
    this.jImageView.setComposition(composition?.getNativeComposition(), maxFrameRate);
  }

  /**
   * The path string of a pag file set by setPath() or setPathAsync().
   * @returns
   */
  getPath(): string | null {
    return this.filePath;
  }

  /**
   * Loads a pag file from the local path, returns false if the file does not exist or the data is not a pag file.
   * @param path
   * @returns
   */
  setPath(path: string, maxFrameRate: number = 30.0): boolean {
    this.filePath = path;
    let file = PAGFile.LoadFromPath(path);
    this.composition = file;
    this.jImageView.setComposition(file?.getNativeComposition(), maxFrameRate);
    return file != null;
  }

  /**
   * Asynchronously load a pag file from the specific path which can be a network path or a local path.
   * @param path
   * @returns
   */
  setPathAsync(path: string, maxFrameRate: number = 30.0): Promise<PAGFile | null> {
    this.filePath = path;
    return new Promise<PAGFile | null>((resolve) => {
      PAGFile.LoadFromPathAsync(path).then((pagFile) => {
        this.composition = pagFile;
        this.jImageView.setComposition(pagFile?.getNativeComposition(), maxFrameRate);
        resolve(pagFile)
      })
    })
  }

  /**
   * Returns the current scale mode.
   */
  public scaleMode(): PAGScaleMode {
    return this.jImageView.scaleMode();
  }

  /**
   * Specifies the rule of how to scale the pag content to fit the PAGImageView's size. The
   * current matrix of the PAGImageView changes when this method is called.
   */
  setScaleMode(scaleMode: PAGScaleMode) {
    this.jImageView.setScaleMode(scaleMode);
  }

  /**
   * Returns a copy of the current matrix.
   */
  matrix(): Matrix4 {
    return PAGUtils.ToTsMatrix(this.jImageView.matrix());
  }

  /**
   * Sets the transformation which will be applied to the composition. The scaleMode property
   * will be set to PAGScaleMode::None when this method is called.
   */
  setMatrix(matrix: Matrix4) {
    this.jImageView.setMatrix(PAGUtils.ToNativeMatrix(matrix));
  }

  /**
   * This value defines the scale factor for the size of the cached image frames, which ranges
   * from 0.0 to 1.0. A scale factor less than 1.0 may result in blurred output, but it can reduce
   * graphics memory usage, increasing the rendering performance. The default value is 1.0.
   */
  renderScale(): number {
    return this.jImageView.renderScale();
  }

  /**
   * Sets the value of the renderScale property.
   */
  setRenderScale(renderScale: number) {
    this.jImageView.setRenderScale(renderScale);
  }

  /**
   * If set to true, the PAGImageView loads all image frames into the memory, which will
   * significantly increase the rendering performance but may cost lots of additional memory. Set
   * it to true if you prefer rendering speed over memory usage. If set to false, the PAGImageView
   * loads only one image frame at a time into the memory. The default value is false.
   */
  cacheAllFramesInMemory(): boolean {
    return this.jImageView.cacheAllFramesInMemory();
  }

  /**
   * Sets the value of the cacheAllFramesInMemory property.
   */
  setCacheAllFramesInMemory(enable: boolean) {
    this.jImageView.setCacheAllFramesInMemory(enable);
  }

  /**
   * Returns the current frame index the PAGImageView is rendering.
   */
  currentFrame(): number {
    return this.jImageView.currentFrame();
  }

  /**
   * Sets the frame index for the PAGImageView to render.
   */
  setCurrentFrame(currentFrame: number) {
    this.jImageView.setCurrentFrame(currentFrame);
  }

  /**
   * Returns a PixelMap capturing the contents of the current PAGImageView.
   */
  currentImage(): PixelMap | null {
    return this.jImageView.currentImage();
  }

  /**
   * Starts to play the animation from the current position. Calling the play() method when the
   * animation is already playing has no effect. The play() method does not alter the animation's
   * current position. However, if the animation previously reached its end, it will restart from
   * the beginning.
   */
  play() {
    this.jImageView.play();
  }

  /**
   * Indicates whether the animation is playing.
   */
  isPlaying(): boolean {
    return this.jImageView.isPlaying();
  }

  /**
   * Cancels the animation at the current position. Calling the play() method can resume the
   * animation from the last paused position.
   */
  pause() {
    this.jImageView.pause();
  }

  /**
   * The total number of times the animation is set to play. The default is 1, which means the
   * animation will play only once. If the repeat count is set to 0 or a negative value, the
   * animation will play infinity times.
   */
  repeatCount(): number {
    return this.jImageView.repeatCount();
  }

  /**
   * Set the number of times the animation to play.
   */
  setRepeatCount(repeatCount: number) {
    this.jImageView.setRepeatCount(repeatCount);
  }

  /**
   * Adds a listener to the set of listeners that are sent events through the life of an
   * animation, such as start, repeat, and end.
   */
  addListener(listener: WeakRef<PAGImageViewListener>) {
    this.listeners.push(listener);
  }

  /**
   * Removes a listener from the set listening to this animation.
   */
  removeListener(listener: WeakRef<PAGImageViewListener>) {
    let index = this.listeners.indexOf(listener);
    if (index != -1) {
      this.listeners.splice(index);
    }
  }

  /**
   * Renders the current image frame immediately. Note that all the changes previously made to the
   * PAGImageView will only take effect after this method is called. If the play() method is
   * already called, there is no need to call it manually since it will be automatically called
   * every frame. Returns true if the content has changed.
   */
  flush(): boolean {
    return this.jImageView.flush();
  }

  uniqueID(): string {
    return this.jImageView.uniqueID();
  }

  update(): void {
    return this.jImageView.update();
  }

  constructor() {
    let handler = new PAGViewCallbackHandler(this);
    this.jImageView.setStateChangeCallback(handler.onStateChange);
    this.jImageView.setProgressUpdateCallback(handler.onProgressUpdate);
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

  attachToView(view: WeakRef<ESObject>) {
    if (this.view != null && this.view != view) {
      console.error("controller has attached to view");
    }
    this.view = view;
  }

  detachFromView() {
    this.view = null;
  }

  /**
   * Free up resources used by the PAGImageViewController instance immediately instead of relying on the
   * garbage collector to do this for you at some point in the future.
   */
  release() {
    this.jImageView.release();
  }

  private composition: PAGComposition | null = null;
  private filePath: string | null = null;
  private listeners: Array<WeakRef<PAGImageViewListener>> = [];
  private view: WeakRef<ESObject> | null = null;
  private jImageView: JPAGImageView = new JPAGImageView();
  readonly onAnimatorStateChange = (state: number): void => {
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
  readonly onAnimatorProgressUpdate = (): void => {
    for (const weakListener of this.listeners) {
      const listener = weakListener.deref();
      if (listener && listener.onAnimationUpdate) {
        listener.onAnimationUpdate(this);
      }
    }
  }
}