import { PAGModule } from '../binding';
import { LayerType, Vector } from '../types';

import type { PAGLayer } from '../pag-layer';
import type { PAGImageLayer } from '../pag-image-layer';
import type { PAGSolidLayer } from '../pag-solid-layer';
import type { PAGTextLayer } from '../pag-text-layer';

const rewindData = (fn: (...args: any[]) => any, scope: any, ...args: any[]) => {
  if (PAGModule.Asyncify.currData !== null) {
    const currData = PAGModule.Asyncify.currData;
    PAGModule.Asyncify.currData = null;
    const ret = fn.call(scope, ...args);
    PAGModule.Asyncify.currData = currData;
    return ret;
  } else {
    return fn.call(scope, ...args);
  }
};

export const proxyVector = <T extends (...args: any) => any>(
  vector: Vector<any>,
  process: T,
): Vector<ReturnType<T>> => {
  const proxy = new Proxy(vector, {
    get(target, property, receiver) {
      switch (property) {
        case 'get':
          return (index: number) => {
            const wasmIns = rewindData(target.get, target, index);
            return process(wasmIns);
          };
        case 'push_back':
          return (value: ReturnType<T>) => {
            rewindData(target.push_back, target, value.wasmIns || value);
            return undefined;
          };
        case 'size':
          return () => {
            return rewindData(target.size, target);
          };
        default:
          return Reflect.get(target, property, receiver);
      }
    },
  });
  return proxy;
};

export const isOffscreenCanvas = (element: any) => window.OffscreenCanvas && element instanceof OffscreenCanvas;

export const layer2typeLayer = (wasmIns: any): PAGSolidLayer | PAGTextLayer | PAGImageLayer | PAGLayer => {
  switch (wasmIns._layerType()) {
    case LayerType.Solid:
      return new PAGModule.PAGSolidLayer(wasmIns);
    case LayerType.Text:
      return new PAGModule.PAGTextLayer(wasmIns);
    case LayerType.Image:
      return new PAGModule.PAGImageLayer(wasmIns);
    default:
      return new PAGModule.PAGLayer(wasmIns);
  }
};

export const getLayerTypeName = (layerType:LayerType)=> {
  switch (layerType) {
    case LayerType.Solid:
      return 'Solid';
    case LayerType.Text:
      return 'Text';
    case LayerType.Shape:
      return 'Shape';
    case LayerType.Image:
      return 'Image';
    case LayerType.PreCompose:
      return 'PreCompose';
    default:
      return 'Unknown';
  }
}
