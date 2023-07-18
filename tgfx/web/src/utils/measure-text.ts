export const measureText = (imageData: ImageData) => {
  const imageDataInt32Array = new Int32Array(imageData.data.buffer);
  let left = getLeftPixel(imageDataInt32Array, imageData.width, imageData.height);
  let top = getTopPixel(imageDataInt32Array, imageData.width, imageData.height);
  let right = getRightPixel(imageDataInt32Array, imageData.width, imageData.height);
  let bottom = getBottomPixel(imageDataInt32Array, imageData.width, imageData.height);
  return { left, top, right, bottom };
};

export const getLeftPixel = (imageDataArray: Int32Array, width: number, height: number) => {
  const verticalCount = imageDataArray.length / width;
  const acrossCount = imageDataArray.length / height;
  for (let i = 0; i < acrossCount; i++) {
    for (let j = 0; j < verticalCount; j++) {
      if (imageDataArray[i + j * width] !== 0) return i;
    }
  }
  return acrossCount;
};

export const getTopPixel = (imageDataArray: Int32Array, width: number, height: number) => {
  const verticalCount = imageDataArray.length / width;
  const acrossCount = imageDataArray.length / height;
  for (let i = 0; i < verticalCount; i++) {
    for (let j = 0; j < acrossCount; j++) {
      if (imageDataArray[i * width + j] !== 0) return i;
    }
  }
  return verticalCount;
};

export const getRightPixel = (imageDataArray: Int32Array, width: number, height: number) => {
  const verticalCount = imageDataArray.length / width;
  const acrossCount = imageDataArray.length / height;
  for (let i = acrossCount - 1; i > 0; i--) {
    for (let j = verticalCount - 1; j > 0; j--) {
      if (imageDataArray[i + width * j] !== 0) return i;
    }
  }
  return 0;
};

export const getBottomPixel = (imageDataArray: Int32Array, width: number, height: number) => {
  const verticalCount = imageDataArray.length / width;
  const acrossCount = imageDataArray.length / height;
  for (let i = verticalCount - 1; i > 0; i--) {
    for (let j = acrossCount - 1; j > 0; j--) {
      if (imageDataArray[i * width + j] !== 0) return i;
    }
  }
  return 0;
};