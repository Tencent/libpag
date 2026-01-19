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

import { JPAGView } from 'libpag.so'
import { PAGComposition } from './PAGComposition';
import { PAGScaleMode } from './PAGScaleMode';
import { PAGLayer } from './PAGLayer';
import { image } from '@kit.ImageKit';
import { PAGUtils } from './private/PAGUtils';
import { Matrix4 } from '@ohos.arkui.node';
import { PAGAnimatorState } from './private/PAGAnimatorState';
import { PAGFile } from './PAGFile';
import { common2D } from '@kit.ArkGraphics2D';
import { PAGViewCallbackHandler } from './private/PAGViewCallbackHandler';

export interface PAGViewListener {
  /**
   * Notifies the beginning of the animation. It can be called from either the UI thread or the thread
   * that calls the play method.
   */
  onAnimationStart?: (viewController: PAGViewController) => void;

  /**
   * Notifies the end of the animation. It can only be called from the UI thread.
   */
  onAnimationEnd?: (viewController: PAGViewController) => void;

  /**
   * Notifies the repetition of the animation. It can only be called from the UI thread.
   */
  onAnimationRepeat?: (viewController: PAGViewController) => void;

  /**
   * Notifies the cancellation of the animation. It can be called from either the UI thread or the
   * thread that calls the stop method.
   */
  onAnimationCancel?: (viewController: PAGViewController) => void;

  /**
   * Notifies another frame of the animation has occurred. It may be called from an arbitrary
   * thread if the animation is running asynchronously.
   */
  onAnimationUpdate?: (viewController: PAGViewController) => void;
}

export class PAGViewController {
  /**
   * Sets a new PAGComposition for PAGView to render as content. Note that if the composition is
   * already added to another PAGView, it will be removed from the previous PAGView.
   */
  setComposition(composition: PAGComposition | null) {
    this.filePath = null;
    this.composition = composition;
    this.jView.setComposition(composition?.getNativeComposition());
  }

  /**
   * Returns the current PAGComposition for PAGView to render as content.
   */
  getComposition(): PAGComposition | null {
    return this.composition;
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
  setPath(path: string): boolean {
    this.filePath = path;
    let file = PAGFile.LoadFromPath(path);
    this.composition = file;
    this.jView.setComposition(file?.getNativeComposition());
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
      PAGFile.LoadFromPathAsync(path).then((pagFile) => {
        this.composition = pagFile;
        this.jView.setComposition(pagFile?.getNativeComposition());
        resolve(pagFile)
      })
    })
  }

  /**
   * Starts to play the animation from the current position. Calling the play() method when the
   * animation is already playing has no effect. The play() method does not alter the animation's
   * current position. However, if the animation previously reached its end, it will restart from
   * the beginning.
   */
  play() {
    this.jView?.play();
  }

  /**
   * Cancels the animation at the current position. Calling the play() method can resume the
   * animation from the last paused position.
   */
  pause() {
    this.jView.pause();
  }

  /**
   * Indicates whether the animation is playing.
   */
  isPlaying(): boolean {
    return this.jView.isPlaying();
  }

  /**
   * Returns true if PAGView is playing in the main thread. The default value is false.
   */
  isSync(): boolean {
    return this.jView.isSync();
  }

  /**
   * Sets the sync flag for the PAGView.
   */
  setSync(isSync: boolean) {
    this.jView.setSync(isSync);
  }

  /**
   * Set the number of times the animation to play.
   */
  setRepeatCount(repeatCount: number) {
    this.jView.setRepeatCount(repeatCount);
  }

  /**
   * The total number of times the animation is set to play. The default is 1, which means the
   * animation will play only once. If the repeat count is set to 0 or a negative value, the
   * animation will play infinity times.
   */
  repeatCount(): number {
    return this.jView.repeatCount();
  }

  /**
   * If set to false, PAGView skips rendering for video composition.
   */
  videoEnabled(): boolean {
    return this.jView.videoEnabled();
  }

  /**
   * Sets the value of videoEnabled property.
   */
  setVideoEnabled(enable: boolean) {
    this.jView.setVideoEnabled(enable);
  }

  /**
   * If set to true, PAG renderer caches an internal bitmap representation of the static content
   * for each layer. This caching can increase performance for layers that contain complex vector
   * content. The execution speed can be significantly faster depending on the complexity of the
   * content, but it requires extra graphics memory. The default value is true.
   */
  cacheEnabled(): boolean {
    return this.jView.cacheEnabled();
  }

  /**
   * Sets the value of cacheEnabled property.
   */
  setCacheEnabled(value: boolean) {
    this.jView.setCacheEnabled(value);
  }

  /**
   * If set to true, PAG will cache the associated rendering data into a disk file, such as the
   * decoded image frames of video compositions. This can help reduce memory usage and improve
   * rendering performance.
   */
  useDiskCache(): boolean {
    return this.jView.useDiskCache();
  }

  /**
   * Set the value of useDiskCache property.
   */
  setUseDiskCache(value: boolean) {
    this.jView.setUseDiskCache(value);
  }

  /**
   * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
   * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of
   * graphics memory which leads to better performance. The default value is 1.0.
   */
  cacheScale(): number {
    return this.jView.cacheScale();
  }

  /**
   * Sets the value of cacheScale property.
   */
  setCacheScale(value: number) {
    this.jView.setCacheScale(value);
  }

  /**
   * Sets the maximum frame rate for rendering.
   */
  setMaxFrameRate(maxFrameRate: number) {
    this.jView.setMaxFrameRate(maxFrameRate);
  }

  /**
   * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
   * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
   * value is 60.
   */
  maxFrameRate(): number {
    return this.jView.maxFrameRate();
  }

  /**
   * Returns the current scale mode.
   */
  scaleMode(): PAGScaleMode {
    return this.jView.scaleMode();
  }

  /**
   * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
   * changes when this method is called.
   */
  setScaleMode(mode: PAGScaleMode) {
    this.jView.setScaleMode(mode);
  }

  /**
   * Returns a copy of current matrix.
   */
  matrix(): Matrix4 {
    return PAGUtils.ToTsMatrix(this.jView.matrix());
  }

  /**
   * Sets the transformation which will be applied to the composition. The scaleMode property
   * will be set to PAGScaleMode::None when this method is called.
   */
  setMatrix(matrix: Matrix4) {
    this.jView.setMatrix(PAGUtils.ToNativeMatrix(matrix));
  }

  /**
   * The duration of current composition in microseconds.
   */
  duration(): number {
    return this.composition ? this.composition.duration() : 0;
  }

  /**
   * Returns the current progress of play position, the value is from 0.0 to 1.0.
   */
  getProgress(): number {
    return this.jView.getProgress();
  }

  /**
   * Sets the progress of play position, the valid value is from 0.0 to 1.0.
   */
  setProgress(progress: number) {
    this.jView?.setProgress(progress);
  }

  /**
   * Returns the current frame.
   */
  currentFrame(): number {
    return this.jView.currentFrame();
  }

  /**
   * Call this method to render current position immediately. Note that all the changes previously
   * made to the PAGView will only take effect after this method is called. If the play() method
   * is already called, there is no need to call it manually since it will be automatically called
   * every frame. Returns true if the content has changed.
   */
  flush(): boolean {
    return this.jView.flush();
  }

  /**
   * Returns an array of layers that lie under the specified point.
   * The point is in pixels not dp.
   */
  getLayersUnderPoint(x: number, y: number): Array<PAGLayer> {
    return PAGUtils.WarpJPAGLayers(this.jView.getLayersUnderPoint(x, y));
  }

  /**
   * Free the cache created by the pag view immediately. Can be called to reduce memory pressure.
   */
  freeCache() {
    this.jView.freeCache();
  }

  /**
   * Returns a bitmap capturing the contents of the PAGView. Subsequent rendering of the
   * PAGView will not be captured. Returns null if the PAGView hasn't been presented yet.
   */
  makeSnapshot(): image.PixelMap | null {
    return this.jView.makeSnapshot();
  }

  /**
   * Returns a rectangle in pixels that defines the displaying area of the specified layer, which
   * is in the coordinate of the PAGView.
   */
  getBounds(pagLayer: PAGLayer): common2D.Rect {
    return PAGUtils.ToTsRect(this.jView.getBounds(pagLayer.nativeLayer));
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
    let index = this.listeners.indexOf(listener);
    if (index != -1) {
      this.listeners.splice(index);
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
   * Free up resources used by the PAGViewController instance immediately instead of relying on the
   * garbage collector to do this for you at some point in the future.
   */
  release() {
    this.jView.release();
  }

  uniqueID() {
    return this.jView.uniqueID();
  }

  update() {
    return this.jView.update();
  }

  constructor() {
    let handler = new PAGViewCallbackHandler(this);
    this.jView.setStateChangeCallback(handler.onStateChange);
    this.jView.setProgressUpdateCallback(handler.onProgressUpdate);
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
  private composition: PAGComposition | null = null;
  private filePath: string | null = null;
  private listeners: Array<WeakRef<PAGViewListener>> = [];
  private jView: JPAGView = new JPAGView();
  private view: WeakRef<ESObject> | null = null;
}
