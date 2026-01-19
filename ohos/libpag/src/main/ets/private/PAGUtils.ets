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
import { JPAGFont, JPAGLayer, JPAGText } from 'libpag.so';
import { PAGComposition } from '../PAGComposition';
import { PAGFile } from '../PAGFile';
import { PAGFont } from '../PAGFont';
import { PAGImageLayer } from '../PAGImageLayer';
import { PAGLayer, PAGLayerType } from '../PAGLayer';
import { PAGShapeLayer } from '../PAGShapeLayer';
import { PAGSolidLayer } from '../PAGSolidLayer';
import { PAGText } from '../PAGText';
import { PAGTextLayer } from '../PAGTextLayer';

export namespace PAGUtils {
  export function ToTsMatrix(matrix: Array<number>): Matrix4 {

    return [matrix[0], matrix[1], matrix[2], 0,
      matrix[3], matrix[4], matrix[5], 0,
      matrix[6], matrix[7], matrix[8], 0,
      0, 0, 0, 1];
  }

  export function ToNativeMatrix(matrix: Matrix4): Array<number> {
    return Array.of(matrix[0], matrix[1], matrix[2], matrix[4], matrix[5], matrix[6], matrix[8], matrix[9],
      matrix[10]);
  }

  export function ToTsRect(rect: Array<number>): Rect {
    return {
      left: rect[0],
      top: rect[1],
      right: rect[2],
      bottom: rect[3]
    };
  }

  export function ToNativeRect(rect: Rect): Array<number> {
    return Array.of(rect.left, rect.top, rect.right, rect.bottom);
  }

  export function WarpJPAGLayer(nativeLayer: JPAGLayer): PAGLayer {
    if (nativeLayer.isPAGFile()) {
      return new PAGFile(nativeLayer);
    }
    switch (nativeLayer.layerType()) {
      case PAGLayerType.Solid:
        return new PAGSolidLayer(nativeLayer);
      case PAGLayerType.Text:
        return new PAGTextLayer(nativeLayer);
      case PAGLayerType.Shape:
        return new PAGShapeLayer(nativeLayer);
      case PAGLayerType.Image:
        return new PAGImageLayer(nativeLayer);
      case PAGLayerType.PreCompose:
        return new PAGComposition(nativeLayer);
    }
    return new PAGLayer(nativeLayer);
  }

  export function WarpJPAGLayers(nativeLayers: Array<JPAGLayer>): Array<PAGLayer> {
    let result = new Array<PAGLayer>(nativeLayers.length);
    for (let i = 0; i < nativeLayers.length; i++) {
      result[i] = PAGUtils.WarpJPAGLayer(nativeLayers[i]);
    }
    return result;
  }

  export function ToJPAGFont(font: PAGFont): JPAGFont {
    return new JPAGFont(font.fontFamily, font.fontStyle);
  }

  export function ToPAGFont(font: JPAGFont): PAGFont {
    return new PAGFont(font.fontFamily, font.fontStyle);
  }

  export function IsEmptyMatrix(matrix: Matrix4) {
    for (let index = 0; index < 16; index++) {
      if (matrix[index] != 0) {
        return false;
      }
    }
    return true;
  }

  export function ToPAGText(text: JPAGText): PAGText {
    let result = new PAGText();
    result.applyFill = text.applyFill;
    result.applyStroke = text.applyStroke;
    result.baselineShift = text.baselineShift;
    result.boxText = text.boxText;
    result.boxTextRect = ToTsRect(text.boxTextRect);
    result.firstBaseLine = text.firstBaseLine;
    result.fauxBold = text.fauxBold;
    result.fauxItalic = text.fauxItalic;
    result.fillColor = text.fillColor;
    result.fontFamily = text.fontFamily;
    result.fontStyle = text.fontStyle;
    result.fontSize = text.fontSize;
    result.strokeColor = text.strokeColor;
    result.strokeOverFill = text.strokeOverFill;
    result.strokeWidth = text.strokeWidth;
    result.text = text.text;
    result.justification = text.justification;
    result.leading = text.leading;
    result.tracking = text.tracking;
    result.backgroundColor = text.backgroundColor;
    result.backgroundAlpha = text.backgroundAlpha;
    return result;
  }

  export function ToJPAGText(text: PAGText): JPAGText {
    let result = new JPAGText();
    result.applyFill = text.applyFill;
    result.applyStroke = text.applyStroke;
    result.baselineShift = text.baselineShift;
    result.boxText = text.boxText;
    result.boxTextRect = ToNativeRect(text.boxTextRect);
    result.firstBaseLine = text.firstBaseLine;
    result.fauxBold = text.fauxBold;
    result.fauxItalic = text.fauxItalic;
    result.fillColor = text.fillColor;
    result.fontFamily = text.fontFamily;
    result.fontStyle = text.fontStyle;
    result.fontSize = text.fontSize;
    result.strokeColor = text.strokeColor;
    result.strokeOverFill = text.strokeOverFill;
    result.strokeWidth = text.strokeWidth;
    result.text = text.text;
    result.justification = text.justification;
    result.leading = text.leading;
    result.tracking = text.tracking;
    result.backgroundColor = text.backgroundColor;
    result.backgroundAlpha = text.backgroundAlpha;
    return result;
  }
}
