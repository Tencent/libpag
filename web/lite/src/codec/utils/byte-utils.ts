export const memcpy = (dst: ArrayBuffer, dstOffset: number, src: ArrayBuffer, srcOffset: number, num: number) => {
  if (
    dstOffset >= dst.byteLength ||
    srcOffset >= src.byteLength ||
    src.byteLength - srcOffset > dst.byteLength - dstOffset ||
    num > src.byteLength
  )
    return;
  const dstUint8Array = new Uint8Array(dst);
  const srcUint8Array = new Uint8Array(src, srcOffset, num);
  dstUint8Array.set(srcUint8Array, dstOffset);
};

export const concatUint8Arrays = (arrays: Array<Uint8Array>) => {
  let totalLength = 0;
  for (const arr of arrays) {
    totalLength += arr.byteLength;
  }
  const result = new Uint8Array(totalLength);
  let offset = 0;
  for (const arr of arrays) {
    result.set(arr, offset);
    offset += arr.byteLength;
  }
  return result;
};
