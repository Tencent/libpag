// index.js
import { PAGInit, clearCache } from '../../utils/libpag';
import { loadFileByRequest } from '../../utils/request';

let debugData = {
  FPS: 0,
  decodeTime: 0,
  flushTime: 0,
  renderingTime: 0,
  imageDecodingTime: 0,
  presentingTime: 0,
};

Page({
  data: {
    PAG: null,
    pagView: null,
    pagFile: null,
    pagLoaded: false,
    repeatCount: 1,
    scaleMap: ['None', 'Stretch', 'LetterBox', 'Zoom'],
    scaleIndex: 2,
    progress: 0,
    debugText: '',
    canvas: null,
    mp4Path: '',
    replaceText: 'hello PAG!',
  },
  async onReady() {
    this.PAG = await PAGInit({
      locateFile: (file) => '/utils/' + file,
    });

    wx.createSelectorQuery()
      .select('#pag')
      .node()
      .exec((res) => {
        this.setData({ canvas: res[0].node });
      });
  },
  async load() {
    wx.showLoading({
      title: '加载中',
    });
    if (!this.data.canvas) {
      console.error('canvas not ready');
      wx.hideLoading();
      return;
    }
    const buffer = await loadFileByRequest('https://pag.io/file/test.pag');
    if (!buffer) throw '加载失败';
    const time = wx.getPerformance().now();
    this.pagFile = await this.PAG.PAGFile.load(buffer);
    debugData = { ...debugData, decodeTime: wx.getPerformance().now() - time };
    this.pagView = await this.PAG.PAGView.init(this.pagFile, 'pag'); // 建议传入wxml中定义的canvasId
    this.updateDebugData(this.pagView);
    this.pagView.addListener('onAnimationUpdate', () => {
      this.updateDebugData(this.pagView);
    });
    this.setData({ pagLoaded: true });
    wx.hideLoading();
  },
  clear() {
    clearCache();
  },
  play() {
    this.pagView.play();
  },
  pause() {
    this.pagView.pause();
  },
  stop() {
    this.pagView.stop();
  },
  destroy() {
    this.pagView.destroy();
  },
  repeatCountChange(event) {
    this.pagView.setRepeatCount(event.detail.value);
    this.setData({ repeatCount: event.detail.value });
  },
  progressChange(event) {
    this.pagView.setProgress(event.detail.value);
    this.pagView.flush();
    this.setData({ progress: event.detail.value });
  },
  scalePickerChange(event) {
    this.pagView.setScaleMode(event.detail.value);
    this.pagView.flush();
    this.setData({ scaleIndex: event.detail.value });
  },
  replaceTextChange(event) {
    const textDoc = this.pagFile.getTextData(0);
    textDoc.text = event.detail.value;
    this.pagFile.replaceText(0, textDoc);
    this.pagView.flush();
  },
  async replaceImage() {
    const canvas = this.data.canvas;
    const image = await new Promise((resolve) => {
      wx.getImageInfo({
        src: 'https://pag.io/img/ae.png',
        success: async (res) => {
          console.log(res.path);
          const image = await new Promise((resolve) => {
            const image = canvas.createImage();
            image.onload = () => {
              resolve(image);
            };
            image.src = res.path;
          });
          resolve(image);
        },
      });
    });
    const pagImage = this.PAG.PAGImage.fromSource(image);
    this.pagFile.replaceImage(0, pagImage);
    this.pagView.flush();
  },
  async replaceVideo() {
    wx.downloadFile({
      url: 'https://pag.io/file/circle.mp4',
      success: (res) => {
        console.log(res.tempFilePath);
        this.videoDecoder = wx.createVideoDecoder();
        this.videoDecoder
          .start({
            source: res.tempFilePath,
            mode: 1,
          })
          .then(() => {
            this.pagView.addListener('onAnimationUpdate', () => {
              console.log('onAnimationUpdate');
              this.replaceVideoFrame();
            });
          });
      },
      fail(res) {
        console.log(res.errMsg);
      },
    });
  },
  replaceVideoFrame() {
    const frameData = this.videoDecoder.getFrameData();
    if (frameData === null) {
      setTimeout(() => {
        this.replaceVideoFrame();
      }, 10);
      return;
    }
    console.log('get frame data', frameData);
    const pagImage = this.PAG.PAGImage.fromArrayBuffer(frameData.data, frameData.width, frameData.height);
    this.pagFile.replaceImage(0, pagImage);
  },
  registerFont() {
    wx.showLoading({
      title: '字体加载中',
    });
    wx.loadFontFace({
      global: true,
      family: 'SourceHanSerifCN',
      source: 'https://pag.io/file/SourceHanSerifCN-Regular.ttf',
      scopes: ['webview', 'native'],
      success: (res) => {
        wx.hideLoading();
        console.log(res.status);
        const textDoc = this.pagFile.getTextData(0);
        textDoc.fontFamily = 'SourceHanSerifCN';
        this.pagFile.replaceText(0, textDoc);
        this.pagView.flush();
      },
      fail: (res) => {
        wx.hideLoading();
        console.error(res.status);
      },
    });
  },
  benchmark() {
    wx.redirectTo({
      url: '/pages/benchmark/benchmark',
    });
  },
  updateDebugData(pagView) {
    debugData = {
      ...debugData,
      ...pagView.getDebugData(),
      renderingTime: pagView.player.renderingTime() / 1000,
      imageDecodingTime: pagView.player.imageDecodingTime() / 1000,
      presentingTime: pagView.player.presentingTime() / 1000,
    };
    this.updateDebugText(debugData);
  },
  updateDebugText(debugData) {
    const text = `
      调试数据：
      FPS：${Math.round(debugData.FPS)}
      PAG文件解码耗时：${debugData.decodeTime.toFixed(2)}ms
      当前帧渲染耗时：${debugData.flushTime.toFixed(2)}ms
      RenderingTime: ${debugData.renderingTime.toFixed(2)}ms
      ImageDecodingTime: ${debugData.imageDecodingTime.toFixed(2)}ms
      PresentingTime: ${debugData.presentingTime.toFixed(2)}ms
    `;
    this.setData({ debugText: text });
  },
});
