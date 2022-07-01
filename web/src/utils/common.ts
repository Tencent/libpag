export const readFile = (file: File): Promise<String | ArrayBuffer | null> =>
  new Promise((resolve) => {
    const reader = new FileReader();
    reader.onload = () => {
      resolve(reader.result);
    };
    reader.onerror = () => {
      console.error((reader.error as DOMException).message);
    };
    reader.readAsArrayBuffer(file);
  });

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
