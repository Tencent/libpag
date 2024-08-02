import { Matrix4, Rect } from '@ohos.arkui.node';
import { JPAGLayer } from 'libpag.so';
import { PAGComposition } from '../PAGComposition';
import { PAGFile } from '../PAGFile';
import { PAGImageLayer } from '../PAGImageLayer';
import { PAGLayer, PAGLayerType } from '../PAGLayer';
import { PAGShapeLayer } from '../PAGShapeLayer';
import { PAGSolidLayer } from '../PAGSolidLayer';
import { PAGTextLayer } from '../PAGTextLayer';

export namespace PAGUtils {
  export function ToTsMatrix(matrix: Float32Array): Matrix4 {

    return [matrix[0], matrix[1], matrix[2], 0,
      matrix[3], matrix[4], matrix[5], 0,
      matrix[6], matrix[7], matrix[8], 0,
      0, 0, 0, 1];
  }

  export function ToNativeMatrix(matrix: Matrix4): Float32Array {
    return Float32Array.of(matrix[0], matrix[1], matrix[2], matrix[4], matrix[5], matrix[6], matrix[8], matrix[9],
      matrix[10]);
  }

  export function ToTsRect(rect: Float32Array): Rect {
    return {
      left: rect[0],
      top: rect[1],
      right: rect[2],
      bottom: rect[3]
    };
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

  export function IsEmptyMatrix(matrix: Matrix4) {
    for (let index = 0; index < 16; index++) {
      if (matrix[index] != 0) {
        return false;
      }
    }
    return true;
  }
}