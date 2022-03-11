import { Vector } from '../types';

export class VectorArray<T> extends Array<T> {
  public delete() {}
}

export const vector2array = <T extends { destroy: () => void }>(
  vector: Vector<any>,
  constructor: { new (wasmIns: any): T },
): VectorArray<T> => {
  const array: VectorArray<T> = new VectorArray<T>(vector.size());
  array.delete = () => {
    const set = new Set(array);
    set.forEach((value: T) => {
      value.destroy();
    });
  };
  const proxy = new Proxy(array, {
    get(target, property, receiver) {
      const propNum = typeof property === 'string' ? Number(property) : -1;
      if (propNum > -1 && propNum < target.length) {
        if (!target[propNum]) {
          target[propNum] = new constructor(vector.get(propNum));
        }
        return target[propNum];
      } else {
        return Reflect.get(target, property, receiver);
      }
    },
    set(target, property, value, receiver) {
      const propNum = typeof property === 'string' ? Number(property) : -1;
      if (propNum > -1 && !(value instanceof constructor)) {
        throw new TypeError(`Type '${typeof value}' is not assignable to type '${constructor.name}'.`);
      }
      return Reflect.set(target, property, value, receiver);
    },
  });
  return proxy;
};
