export const writeBufferToWasm = (module: EmscriptenModule, data: ArrayLike<number> | ArrayBufferLike) => {
  const uint8Array = new Uint8Array(data);
  const numBytes = uint8Array.byteLength;
  const dataPtr = module._malloc(numBytes);
  const dataOnHeap = new Uint8Array(module.HEAPU8.buffer, dataPtr, numBytes);
  dataOnHeap.set(uint8Array);
  return { byteOffset: dataPtr, length: numBytes, free: () => module._free(dataPtr) };
};

export const readBufferFromWasm = (
  module: EmscriptenModule,
  data: ArrayLike<number> | ArrayBufferLike,
  handle: (byteOffset: number, length: number) => boolean,
) => {
  const uint8Array = new Uint8Array(data);
  const dataPtr = module._malloc(uint8Array.byteLength);
  if (!handle(dataPtr, uint8Array.byteLength)) return { data: null, free: () => module._free(dataPtr) };
  const dataOnHeap = new Uint8Array(module.HEAPU8.buffer, dataPtr, uint8Array.byteLength);
  uint8Array.set(dataOnHeap);
  return { data: uint8Array, free: () => module._free(dataPtr) };
};
