import { Vector } from '../types';

export const proxyVector = <T extends { wasmIns: any }>(
  vector: Vector<any>,
  constructor: { new (wasmIns: any): T },
): Vector<T> => {
  const proxy = new Proxy(vector, {
    get(target, property, receiver) {
      switch (property) {
        case 'get':
          return (index: number) => {
            const wasmIns = target.get(index);
            return wasmIns ? new constructor(wasmIns) : wasmIns;
          };
        case 'push_back':
          return (value: T) => {
            target.push_back(value.wasmIns);
            return undefined;
          };
        default:
          return Reflect.get(target, property, receiver);
      }
    },
  });
  return proxy;
};
