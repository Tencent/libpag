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

import { PAGLayer } from "./PAGLayer"
import { JPAGLayer, JPAGComposition } from "libpag.so"
import { PAGMarker } from './PAGMarker';
import { PAGUtils } from './private/PAGUtils';

export class PAGComposition extends PAGLayer {
  static Make(width: number, height: number): PAGComposition | null {
    let native = JPAGComposition.Make(width, height);
    if (native == null) {
      return null;
    }
    return new PAGComposition(native);
  }

  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }

  /**
   * The width of Composition.
   */
  width(): number {
    return this.getNativeComposition().width();
  }

  /**
   * Returns the height of Composition.
   */
  height(): number {
    return this.getNativeComposition().height();
  }

  /**
   * Set the size of the Composition.
   */
  setContentSize(width: number, height: number): void {
    this.getNativeComposition().setContentSize(width, height);
  }

  /**
   * Returns the number of child layers of this composition.
   */
  numChildren(): number {
    return this.getNativeComposition().numChildren();
  }

  /**
   * Returns the child layer that exists at the specified index.
   * @param index The index position of the child layer.
   * @return The child layer at the specified index position.
   */
  getLayerAt(index: number): PAGLayer {
    return PAGUtils.WarpJPAGLayer(this.getNativeComposition().getLayerAt(index));
  }

  /**
   * Returns the index position of a child layer.
   * @param layer The layer instance to identify.
   * @return The index position of the child layer to identify.
   */
  getLayerIndex(layer: PAGLayer): number {
    return this.getNativeComposition().getLayerIndex(layer.nativeLayer);
  }

  /**
   * Changes the position of an existing child in the container. This affects the layering of child layers.
   * @param layer The child layer for which you want to change the index number.
   * @param index The resulting index number for the child layer.
   */
  setLayerIndex(layer: PAGLayer, index: number): void {
    this.getNativeComposition().setLayerIndex(layer.nativeLayer, index);
  }

  /**
   * Add PAGLayer to current PAGComposition at the top.
   */
  addLayer(pagLayer: PAGLayer): void {
    this.getNativeComposition().addLayer(pagLayer.nativeLayer);
  }

  /**
   * Add PAGLayer to current PAGComposition at the specified index.
   */
  addLayerAt(pagLayer: PAGLayer, index: number): void {
    this.getNativeComposition().addLayerAt(pagLayer.nativeLayer, index);
  }

  /**
   * Check whether current PAGComposition contains the specified pagLayer.
   */
  contains(pagLayer: PAGLayer): boolean {
    return this.getNativeComposition().contains(pagLayer.nativeLayer);
  }

  /**
   * Remove the specified PAGLayer from current PAGComposition.
   */
  removeLayer(pagLayer: PAGLayer): PAGLayer | null {
    let result = this.getNativeComposition().removeLayer(pagLayer.nativeLayer);
    if (result == null) {
      return null;
    }
    return PAGUtils.WarpJPAGLayer(result);
  }

  /**
   * Remove the PAGLayer at specified index from current PAGComposition.
   */
  removeLayerAt(index: number): PAGLayer | null {
    let result = this.getNativeComposition().removeLayerAt(index);
    if (result == null) {
      return null;
    }
    return PAGUtils.WarpJPAGLayer(result);
  }

  /**
   * Remove all PAGLayers from current PAGComposition.
   */
  removeAllLayers(): void {
    this.getNativeComposition().removeAllLayers();
  }

  /**
   * Swap the layers.
   */
  swapLayer(pagLayer1: PAGLayer, pagLayer2: PAGLayer): void {
    this.getNativeComposition().swapLayer(pagLayer1.nativeLayer, pagLayer2.nativeLayer);
  }

  /**
   * Swap the layers at the specified index.
   */
  swapLayerAt(index1: number, index2: number) {
    this.getNativeComposition().swapLayerAt(index1, index2);
  }

  /**
   * The audio data of this composition, which is an AAC audio in an MPEG-4 container.
   */
  audioBytes(): ArrayBuffer | null {
    return this.getNativeComposition().audioBytes();
  }

  /**
   * Indicates when the first frame of the audio plays in the composition's timeline.
   */
  audioStartTime(): number {
    return this.getNativeComposition().audioStartTime();
  }

  /**
   * Returns the audio markers of this composition.
   */
  audioMarkers(): Array<PAGMarker> {
    return this.getNativeComposition().audioMarkers() as Array<PAGMarker>;
  }

  /**
   * Returns an array of layers that match the specified layer name.
   */
  getLayersByName(layerName: string): Array<PAGLayer> {
    return PAGUtils.WarpJPAGLayers(this.getNativeComposition().getLayersByName(layerName));
  }

  /**
   * Returns an array of layers that lie under the specified point. The point is in pixels and from this
   * PAGComposition's local coordinates.
   */
  getLayersUnderPoint(localX: number, localY: number): Array<PAGLayer> {
    return PAGUtils.WarpJPAGLayers(this.getNativeComposition().getLayersUnderPoint(localX, localY));
  }


  getNativeComposition(): JPAGComposition {
    return this.nativeLayer as JPAGComposition;
  }
}
