/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

import { Matrix4, Rect } from '@ohos.arkui.node';
import { JPAGPlayer } from 'libpag.so'
import { PAGComposition } from './PAGComposition';
import { PAGSurface } from './PAGSurface';
import { PAGLayer } from './PAGLayer';
import { PAGUtils } from './private/PAGUtils';
import { PAGScaleMode } from './PAGScaleMode';

export class PAGPlayer {
  constructor(nativePlayer: JPAGPlayer | null = null) {
    if (nativePlayer == null) {
      this.nativePlayer = new JPAGPlayer();
    } else {
      this.nativePlayer = nativePlayer;
    }
  }

  /**
   * Returns the current PAGComposition for PAGPlayer to render as content.
   */
  getComposition(): PAGComposition | null {
    return this.composition;
  }

  /**
   * Sets a new PAGComposition for PAGPlayer to render as content.
   * Note: If the composition is already added to another PAGPlayer, it will be removed from the
   * previous PAGPlayer.
   */
  setComposition(composition: PAGComposition | null) {
    this.composition = composition;
    if (composition == null) {
      this.nativePlayer.setComposition(null);
    } else {
      this.nativePlayer.setComposition(composition.getNativeComposition());
    }
  }

  /**
   * Returns the PAGSurface object for PAGPlayer to render onto.
   */
  getSurface(): PAGSurface | null {
    return this.surface;
  }

  /**
   * Set the PAGSurface object for PAGPlayer to render onto.
   */
  setSurface(surface: PAGSurface | null): void {
    this.surface = surface;
    if (surface == null) {
      this.nativePlayer.setSurface(null);
    } else {
      this.nativePlayer.setSurface(surface.nativeSurface);
    }
  }

  /**
   * If set to false, the player skips rendering for video composition.
   */
  videoEnabled(): boolean {
    return this.nativePlayer.videoEnabled();
  }

  /**
   * Set the value of videoEnabled property.
   */
  setVideoEnabled(value: boolean) {
    return this.nativePlayer.setVideoEnabled(value);
  }

  /**
   * If set to true, PAGPlayer caches an internal bitmap representation of the static content for
   * each layer. This caching can increase performance for layers that contain complex vector
   * content. The execution speed can be significantly faster depending on the complexity of the
   * content, but it requires extra graphics memory. The default value is true.
   */
  cacheEnabled(): boolean {
    return this.nativePlayer.cacheEnabled();
  }

  /**
   * Set the value of cacheEnabled property.
   */
  setCacheEnabled(value: boolean) {
    this.nativePlayer.setCacheEnabled(value);
  }

  /**
   * If set to true, PAG will cache the associated rendering data into a disk file, such as the
   * decoded image frames of video compositions. This can help reduce memory usage and improve
   * rendering performance.
   */
  public useDiskCache(): boolean {
    return this.nativePlayer.useDiskCache();
  }

  /**
   * Set the value of useDiskCache property.
   */
  public setUseDiskCache(value: boolean) {
    this.nativePlayer.setUseDiskCache(value);
  }

  /**
   * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
   * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of
   * graphics memory which leads to better performance. The default value is 1.0.
   */
  public cacheScale(): number {
    return this.nativePlayer.cacheScale();
  }

  /**
   * Set the value of cacheScale property.
   */
  public setCacheScale(value: number) {
    this.nativePlayer.setCacheScale(value);
  }

  /**
   * The maximum frame rate for rendering, ranges from 1 to 60. If set to a value less than the
   * actual frame rate from composition, it drops frames but increases performance. Otherwise, it
   * has no effect. The default value is 60.
   */
  public maxFrameRate(): number {
    return this.nativePlayer.maxFrameRate();
  }

  /**
   * Set the maximum frame rate for rendering.
   */
  public setMaxFrameRate(value: number) {
    this.nativePlayer.setMaxFrameRate(value);
  }

  /**
   * Returns the current scale mode.
   */
  scaleMode(): PAGScaleMode {
    return this.nativePlayer.scaleMode();
  }

  /**
   * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
   * changes when this method is called.
   */
  setScaleMode(mode: PAGScaleMode) {
    this.nativePlayer.setScaleMode(mode);
  }

  /**
   * Returns a copy of current matrix.
   */
  matrix(): Matrix4 {
    return PAGUtils.ToTsMatrix(this.nativePlayer.matrix());
  }

  /**
   * Sets the transformation which will be applied to the composition. The scaleMode property
   * will be set to PAGScaleMode::None when this method is called.
   */
  public setMatrix(matrix: Matrix4) {
    this.nativePlayer.setMatrix(PAGUtils.ToNativeMatrix(matrix));
  }

  /**
   * The duration of current composition in microseconds.
   */
  public duration(): number {
    return this.nativePlayer.duration();
  }

  /**
   * Returns the current progress of play position, the value is from 0.0 to 1.0.
   */
  public getProgress(): number {
    return this.nativePlayer.getProgress();
  }

  /**
   * Sets the progress of play position, the value ranges from 0.0 to 1.0. It is applied only when
   * the composition is not null.
   */
  setProgress(progress: number): void {
    this.nativePlayer.setProgress(progress);
  }

  /**
   * Returns the current frame.
   */
  public currentFrame(): number {
    return this.nativePlayer.currentFrame();
  }

  /**
   * Prepares the player for the next flush() call. It collects all CPU tasks from the current
   * progress of the composition and runs them asynchronously in parallel. It is usually used for
   * speeding up the first frame rendering.
   */
  prepare() {
    this.nativePlayer.prepare();
  }

  flush() {
    this.nativePlayer.flush();
  }

  /**
   * Returns a rectangle in pixels that defines the displaying area of the specified layer, which
   * is in the coordinate of the PAGSurface.
   */
  getBounds(pagLayer: PAGLayer): Rect {
    return PAGUtils.ToTsRect(this.nativePlayer.getBounds(pagLayer.nativeLayer))
  }

  /**
   * Returns an array of layers that lie under the specified point. The point is in pixels and
   * from the surface's coordinates.
   */
  public getLayersUnderPoint(surfaceX: number, surfaceY: number): Array<PAGLayer> {
    return PAGUtils.WarpJPAGLayers(this.nativePlayer.getLayersUnderPoint(surfaceX, surfaceY));
  }

  /**
   * Evaluates the PAGLayer to see if it overlaps or intersects with the specified point. The point
   * is in the coordinate space of the PAGSurface, not the PAGComposition that contains the
   * PAGLayer. It always returns false if the PAGLayer or its parent (or parent's parent...) has not
   * been added to this PAGPlayer. The pixelHitTest parameter indicates whether or not to check
   * against the actual pixels of the object (true) or the bounding box (false). Returns true if the
   * PAGLayer overlaps or intersects with the specified point.
   */
  hitTestPoint(pagLayer: PAGLayer, surfaceX: number,
    surfaceY: number, pixelHitTest: boolean): boolean {
    return this.nativePlayer.hitTestPoint(pagLayer.nativeLayer, surfaceX, surfaceY, pixelHitTest);
  }

  /**
   * Free up resources used by the PAGPlayer instance immediately instead of relying on the
   * garbage collector to do this for you at some point in the future.
   */
  release(): void {
    this.nativePlayer.release();
  }

  readonly nativePlayer: JPAGPlayer;
  private composition: PAGComposition | null = null;
  private surface: PAGSurface | null = null;
}
