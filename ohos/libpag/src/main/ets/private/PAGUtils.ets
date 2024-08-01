import { matrix4 } from '@kit.ArkUI';
import { Rect } from '@ohos.arkui.node';
import { JsPAGLayer } from 'libpag.so';
import { PAGComposition } from '../PAGComposition';
import { PAGFile } from '../PAGFile';
import { PAGImageLayer } from '../PAGImageLayer';
import { PAGLayer, PAGLayerType } from '../PAGLayer';
import { PAGShapeLayer } from '../PAGShapeLayer';
import { PAGSolidLayer } from '../PAGSolidLayer';
import { PAGTextLayer } from '../PAGTextLayer';

export namespace PAGUtils {
  export function ToTsMatrix(matrix: Float32Array): matrix4.Matrix4Transit {
    return matrix4.init([matrix[0], matrix[1], matrix[2], 0,
      matrix[3], matrix[4], matrix[5], 0,
      matrix[6], matrix[7], matrix[8], 0,
      0, 0, 0, 1]);
  }

  export function ToNativeMatrix(matrix: matrix4.Matrix4Transit): Float32Array {
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

  export function WarpJsPAGLayer(nativeLayer: JsPAGLayer): PAGLayer {
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

  export function WarpJsPAGLayers(nativeLayers: Array<JsPAGLayer>): Array<PAGLayer> {
    let result = new Array<PAGLayer>(nativeLayers.length);
    for (let i = 0; i < nativeLayers.length; i++) {
      result[i] = PAGUtils.WarpJsPAGLayer(nativeLayers[i]);
    }
    return result;
  }
}