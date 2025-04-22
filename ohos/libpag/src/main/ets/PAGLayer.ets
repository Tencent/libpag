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

import { JPAGLayer } from 'libpag.so'
import { PAGComposition } from './PAGComposition';
import { PAGMarker } from './PAGMarker';
import { Matrix4, Rect } from '@ohos.arkui.node';
import { PAGUtils } from './private/PAGUtils';

/**
 * Layers are always one of the following types.
 */
export enum PAGLayerType {
  Unknown = 0,
  Null,
  Solid,
  Text,
  Shape,
  Image,
  PreCompose
}

export class PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    this.nativeLayer = nativeLayer;
  }

  /**
   * Returns the type of layer.
   */
  layerType(): PAGLayerType {
    return this.nativeLayer.layerType();
  }

  /**
   * Returns the name of the layer.
   */
  layerName(): string {
    return this.nativeLayer.layerName();
  }

  /**
   * A matrix object containing values that alter the scaling, rotation, and translation of the layer.
   * Altering it does not change the animation matrix, and it will be concatenated to current animation matrix for
   * displaying.
   */
  matrix(): Matrix4 {
    return PAGUtils.ToTsMatrix(this.nativeLayer.matrix());
  }

  /**
   * A matrix object containing values that alter the scaling, rotation, and translation of the layer.
   * Altering it does not change the animation matrix, and it will be concatenated to current animation matrix for
   * displaying.
   */
  setMatrix(matrix: Matrix4) {
    this.nativeLayer.setMatrix(PAGUtils.ToNativeMatrix(matrix));
  }

  /**
   * Resets the matrix to its default value.
   */
  resetMatrix() {
    this.nativeLayer.resetMatrix();
  }

  /**
   * Returns the layer's display matrix by combining its matrix) property with the current animation
   * matrix from the AE timeline. This does not include the parent layer's matrix.
   * To calculate the final bounds relative to the PAGSurface,
   * use the PAGPlayer::getBounds(PAGLayer layer) method directly.
   */
  getTotalMatrix(): Matrix4 {
    return PAGUtils.ToTsMatrix(this.nativeLayer.getTotalMatrix());
  }

  /**
   * Whether or not the layer is visible.
   */
  visible(): boolean {
    return this.nativeLayer.visible();
  }

  setVisible(value: boolean) {
    this.nativeLayer.setVisible(value);
  }

  /**
   * Ranges from 0 to PAGFile.numTexts - 1 if the layer type is text, or from 0 to PAGFile.numImages -1 if the
   * layer type is image, otherwise returns -1.
   */
  editableIndex(): number {
    return this.nativeLayer.editableIndex();
  }

  /**
   * Indicates the Container instance that contains this Node instance.
   */
  parent(): PAGComposition | null {
    let native = this.nativeLayer.parent();
    if (native) {
      return new PAGComposition(native);
    } else {
      return null;
    }
  }

  /**
   * Returns the markers of this layer.
   */
  markers(): Array<PAGMarker> {
    return this.nativeLayer.markers() as Array<PAGMarker>;
  }

  /**
   * Converts the time from the PAGLayer's (local) timeline to the PAGSurface (global) timeline. The time is in
   * microseconds.
   */
  localTimeToGlobal(localTime: number): number {
    return this.nativeLayer.localTimeToGlobal(localTime);
  }

  /**
   * Converts the time from the PAGSurface (global) to the PAGLayer's (local) timeline timeline. The time is in
   * microseconds.
   */
  globalToLocalTime(globalTime: number): number {
    return this.nativeLayer.globalToLocalTime(globalTime);
  }

  /**
   * The duration of the layer in microseconds, indicates the length of the visible range.
   */
  duration(): number {
    return this.nativeLayer.duration();
  }

  /**
   * Returns the frame rate of this layer.
   */
  frameRate(): number {
    return this.nativeLayer.frameRate();
  }

  /**
   * The start time of the layer in microseconds, indicates the start position of the visible range. It could be a negative value.
   */
  startTime(): number {
    return this.nativeLayer.startTime();
  }

  /**
   * Set the start time of the layer, in microseconds.
   */
  setStartTime(time: number) {
    this.nativeLayer.setStartTime(time);
  }

  /**
   * The current time of the layer in microseconds, the layer is invisible if currentTime is not in the visible range
   * {@code (startTime <= currentTime < startTime + duration)}.
   */
  currentTime(): number {
    return this.nativeLayer.currentTime();
  }

  /**
   * Set the current time of the layer in microseconds.
   */
  setCurrentTime(time: number) {
    this.nativeLayer.setCurrentTime(time);
  }

  /**
   * Returns the current progress of play position, the value is from 0.0 to 1.0.
   */
  getProgress(): number {
    return this.nativeLayer.getProgress();
  }

  /**
   * Set the progress of play position, the value ranges from 0.0 to 1.0. A value of 0.0 represents the
   * frame at startTime. A value of 1.0 represents the frame at the end of duration.
   */
  setProgress(value: number) {
    this.nativeLayer.setProgress(value);
  }

  /**
   * Returns trackMatte layer of this layer.
   */
  trackMatteLayer(): PAGLayer | null {
    let native = this.nativeLayer.trackMatteLayer();
    if (native) {
      return PAGUtils.WarpJPAGLayer(native);
    } else  {
      return null;
    }
  }

  /**
   * Returns a rectangle in pixels that defines the original area of the layer, which is not
   * transformed by the matrix.
   */
  getBounds(): Rect {
    return PAGUtils.ToTsRect(this.nativeLayer.getBounds());
  }

  /**
   * Indicate whether this layer is excluded from parent's timeline. If set to true, this layer's current time
   * will not change when parent's current time changes.
   */
  excludedFromTimeline(): boolean {
    return this.nativeLayer.excludedFromTimeline();
  }

  /**
   * Set the excludedFromTimeline flag of this layer.
   */
  setExcludedFromTimeline(value: boolean) {
    this.nativeLayer.setExcludedFromTimeline(value);
  }

  readonly nativeLayer: JPAGLayer;
}
