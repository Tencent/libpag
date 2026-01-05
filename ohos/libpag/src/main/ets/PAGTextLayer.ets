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

import { JPAGLayer, JPAGTextLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';
import { PAGFont } from './PAGFont';
import { PAGUtils } from './private/PAGUtils';

export class PAGTextLayer extends PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }

  /**
   * Returns the text layer’s fill color.
   */
  fillColor(): number {
    return this.getNativeTextLayer().fillColor();
  }

  /**
   * Set the text layer’s fill color.
   */
  setFillColor(color: number): void {
    this.getNativeTextLayer().setFillColor(color);
  }

  /**
   * Returns the text layer's font.
   */
  font(): PAGFont {
    return PAGUtils.ToPAGFont(this.getNativeTextLayer().font());
  }

  /**
   * Set the text layer's font.
   */
  setFont(font: PAGFont) {
    this.getNativeTextLayer().setFont(PAGUtils.ToJPAGFont(font));
  }

  /**
   * Returns the text layer's font size.
   */
  fontSize(): number {
    return this.getNativeTextLayer().fontSize();
  }

  /**
   * Set the text layer's font size.
   */
  setFontSize(fontSize: number): void {
    this.getNativeTextLayer().setFontSize(fontSize);
  }

  /**
   * Returns the text layer's stroke color.
   */
  strokeColor(): number {
    return this.getNativeTextLayer().strokeColor();
  }

  /**
   * Set the text layer's stroke color.
   */

  setStrokeColor(color: number): void {
    this.getNativeTextLayer().setStrokeColor(color);
  }

  /**
   * Returns the text layer's text.
   */
  text(): string {
    return this.getNativeTextLayer().text();
  }

  /**
   * Set the text layer's text.
   */
  setText(text: string): void {
    this.getNativeTextLayer().setText(text);
  }

  /**
   * Reset the text layer to its default text data.
   */
  reset(): void {
    this.getNativeTextLayer().reset();
  }

  private getNativeTextLayer(): JPAGTextLayer {
    return this.nativeLayer as JPAGTextLayer;
  }
}
