import { PAGInit } from '../src/pag';
import { PAGImage } from '../src/pag-image';
import * as types from '../src/types';

let PAG: types.PAG;

window.onload = async () => {
  PAG = await PAGInit({ locateFile: (file: string) => '../lib/' + file });
  console.log('====== wasm loaded! ======', PAG);

  console.log('====== PAGImage test ======');
  PAGImageLoad();
};

let pagImage: PAGImage;
const PAGImageLoad = async () => {
  const imageBlob = await fetch('./assets/cat.png').then((res) => res.blob());
  pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
  if (!!pagImage.wasmIns) {
    console.log('PAGImage load succeed!');
  } else {
    throw new Error('PAGImage load failed!');
  }
  console.log('PAGImage width:', pagImage.width());
  console.log('PAGImage height:', pagImage.height());
  console.log('PAGImage scaleMode:', pagImage.scaleMode());
  pagImage.setScaleMode(types.ScaleMode.Zoom);
  if (pagImage.scaleMode() === types.ScaleMode.Zoom) {
    console.log(`PAGImage setScaleMode succeed!`);
  } else {
    throw new Error('PAGImage setScaleMode failed!');
  }
  const matrix = pagImage.matrix();
  console.log('PAGImage matrix: ', matrix);
  matrix.set(types.MatrixIndex.a, 2);
  pagImage.setMatrix(matrix);
  if (pagImage.matrix().a === 2) {
    console.log(`PAGImage setMatrix succeed!`);
  } else {
    console.log(`PAGImage setMatrix failed!`);
  }
};
