import { isInstanceOf } from './type-utils';

export const readFile = (file: File) =>
  new Promise<ArrayBuffer | null>((resolve) => {
    const reader = new FileReader();
    reader.onload = () => {
      resolve(reader.result as ArrayBuffer | null);
    };
    reader.onerror = () => {
      console.error((reader.error as DOMException).message);
    };
    reader.readAsArrayBuffer(file);
  });

export const transferToArrayBuffer = (data: File | Blob | ArrayBuffer) => {
  if (isInstanceOf(data, globalThis.File)) {
    return readFile(data as File);
  } else if (isInstanceOf(data, globalThis.Blob)) {
    return readFile(new File([data as Blob], ''));
  } else if (isInstanceOf(data, globalThis.ArrayBuffer)) {
    return Promise.resolve(data as ArrayBuffer);
  }
  return Promise.resolve(null);
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
