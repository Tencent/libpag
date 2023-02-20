import type { PAG } from '../types';

export const malloc = (module: PAG, data: ArrayBufferLike) => {
  const dataUint8Array = new Uint8Array(data);
  const numBytes = dataUint8Array.byteLength;
  const dataPtr = module._malloc(numBytes);
  const dataOnHeap = new Uint8Array(module.HEAPU8.buffer, dataPtr, numBytes);
  dataOnHeap.set(dataUint8Array);
  return { byteOffset: dataPtr, length: numBytes, free: () => module._free(dataPtr) };
};
