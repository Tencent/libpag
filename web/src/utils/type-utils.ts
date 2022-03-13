import { Vector } from '../types';
import { Log } from './log';

export class VectorArray<T> extends Array<T> {
  public delete() {}
}

export const vectorClass2array = <T extends { isDelete: () => boolean }>(
  vector: Vector<any>,
  constructor: { new (wasmIns: any): T },
): VectorArray<T> => {
  const array: VectorArray<T> = new VectorArray<T>(vector.size());
  array.delete = (check: boolean = false) => {
    if (check) {
      for (let i = 0; i < array.length; i++) {
        if (array[i] && array[i].isDelete() === false) {
          Log.warn('There have been some wasm objects not be deleted!');
          break;
        }
      }
    }
    vector.delete();
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

export const vectorBasics2array = <T>(vector: Vector<T>) => {
  const array = new VectorArray<T>(vector.size());
  for (let i = 0; i < vector.size(); i++) {
    array[i] = vector.get(i);
  }
  array.delete = () => {
    vector.delete();
  };
  return array;
};
