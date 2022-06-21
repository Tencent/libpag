import { PAGModule } from '../binding';
import { Vector } from '../types';

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

export const proxyVector = <T extends { wasmIns: any }>(
  vector: Vector<any>,
  constructor: new (wasmIns: any) => T,
): Vector<T> => {
  const proxy = new Proxy(vector, {
    get(target, property, receiver) {
      switch (property) {
        case 'get':
          return (index: number) => {
            const wasmIns = rewindData(target.get, target, index);
            return wasmIns ? new constructor(wasmIns) : wasmIns;
          };
        case 'push_back':
          return (value: T) => {
            rewindData(target.push_back, target, value.wasmIns);
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
