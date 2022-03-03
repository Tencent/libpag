import { PAGInit } from '../src/pag';
import { PAGFile } from '../src/pag-file';
import { PAGImage } from '../src/pag-image';
import { PAGLayer } from '../src/pag-layer';
import * as types from '../src/types';

let PAG: types.PAG;

window.onload = async () => {
  PAG = await PAGInit({ locateFile: (file: string) => '../lib/' + file });
  console.log('====== wasm loaded! ======', PAG);
  console.log('====== PAGImage test ======');
  await PAGImageTest();
  console.log('====== PAGLayer test ======');
  await PAGLayerTest();
};

let pagImage: PAGImage;
const PAGImageTest = async () => {
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
    console.error('PAGImage setScaleMode failed!');
  }
  const matrix = pagImage.matrix();
  console.log('PAGImage matrix: ', matrix);
  matrix.set(types.MatrixIndex.a, 2);
  pagImage.setMatrix(matrix);
  if (pagImage.matrix().a === 2) {
    console.log(`PAGImage setMatrix succeed!`);
  } else {
    console.error(`PAGImage setMatrix failed!`);
  }
};

let pagLayer: PAGLayer;
const PAGLayerTest = async () => {
  const arrayBuffer = await fetch('./assets/test2.pag').then((res) => res.arrayBuffer());
  const pagFile = PAGFile.loadFromBuffer(arrayBuffer);
  pagLayer = pagFile.getLayerAt(0);
  console.log('PAGLayer: ', pagLayer);
  console.log('PAGLayer uniqueID: ', pagLayer.uniqueID());
  console.log('PAGLayer layerType: ', pagLayer.layerType());
  console.log('PAGLayer layerName: ', pagLayer.layerName());
  const matrix = pagLayer.matrix();
  console.log('PAGLayer matrix: ', matrix);
  matrix.set(types.MatrixIndex.a, 2);
  pagLayer.setMatrix(matrix);
  if (pagLayer.matrix().a === 2) {
    console.log(`PAGLayer setMatrix succeed!`);
  } else {
    console.error(`PAGLayer setMatrix failed!`);
  }
  pagLayer.resetMatrix();
  if (pagLayer.matrix().a === 1) {
    console.log(`PAGLayer resetMatrix succeed!`);
  } else {
    console.error(`PAGLayer resetMatrix failed!`);
  }
  console.log('PAGLayer getTotalMatrix: ', pagLayer.getTotalMatrix());
  console.log('PAGLayer alpha: ', pagLayer.alpha());
  pagLayer.setAlpha(0);
  if (pagLayer.alpha() === 0) {
    console.log(`PAGLayer setAlpha succeed!`);
  } else {
    console.error(`PAGLayer setAlpha failed!`);
  }
  console.log('PAGLayer visible: ', pagLayer.visible());
  pagLayer.setVisible(false);
  if (pagLayer.visible() === false) {
    console.log(`PAGLayer setVisible succeed!`);
  } else {
    console.error(`PAGLayer setVisible failed!`);
  }
  console.log('PAGLayer editableIndex: ', pagLayer.editableIndex());
  console.log('PAGLayer parent: ', pagLayer.parent());
  console.log('PAGLayer markers: ', pagLayer.markers());
  console.log('PAGLayer localTimeToGlobal: ', pagLayer.localTimeToGlobal(0));
  console.log('PAGLayer globalToLocalTime: ', pagLayer.globalToLocalTime(0));
  console.log('PAGLayer duration: ', pagLayer.duration());
  console.log('PAGLayer frameRate: ', pagLayer.frameRate());
  console.log('PAGLayer startTime: ', pagLayer.startTime());
  pagLayer.setStartTime(1000000);  
  if (pagLayer.startTime() === 1000000) {
    console.log(`PAGLayer setStartTime succeed!`);
  } else {
    console.error(`PAGLayer setStartTime failed!`);
  }
  console.log('PAGLayer currentTime: ', pagLayer.currentTime());
  pagLayer.setCurrentTime(1000000);  
  if (pagLayer.currentTime() === 1000000) {
    console.log(`PAGLayer setCurrentTime succeed!`);
  } else {
    console.error(`PAGLayer setCurrentTime failed!`);
  }
  console.log('PAGLayer getProgress: ', pagLayer.getProgress());
  pagLayer.setProgress(0.5008333333333334);    
  if (pagLayer.getProgress() === 0.5008333333333334) {
    console.log(`PAGLayer setProgress succeed!`);
  } else {
    console.error(`PAGLayer setProgress failed!`);
  }
  pagLayer.preFrame();
  if (pagLayer.getProgress() === 0.4925) {
    console.log(`PAGLayer preFrame succeed!`);
  } else {
    console.error(`PAGLayer preFrame failed!`);
  }
  pagLayer.nextFrame();
  if (pagLayer.getProgress() === 0.5008333333333334) {
    console.log(`PAGLayer nextFrame succeed!`);
  } else {
    console.error(`PAGLayer nextFrame failed!`);
  }
  console.log('PAGLayer getBounds: ', pagLayer.getBounds());
  console.log('PAGLayer trackMatteLayer: ', pagLayer.trackMatteLayer());
  console.log('PAGLayer excludedFromTimeline: ', pagLayer.excludedFromTimeline());
  pagLayer.setExcludedFromTimeline(true);
  if (pagLayer.excludedFromTimeline() === true) {
    console.log(`PAGLayer excludedFromTimeline succeed!`);
  } else {
    console.error(`PAGLayer excludedFromTimeline failed!`);
  }
  console.log('PAGLayer isPAGFile: ', pagLayer.isPAGFile());
};
