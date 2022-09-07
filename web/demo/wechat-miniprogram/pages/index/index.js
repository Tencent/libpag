// index.js
import { PAGInit } from '../../utils/libpag';

const loadFileByRequest = async (url) => {
  return new Promise((resolve) => {
    wx.request({
      url,
      method: 'GET',
      responseType: 'arraybuffer',
      success(res) {
        if (res.statusCode !== 200) {
          resolve(null);
        }
        resolve(res.data);
      },
      fail() {
        resolve(null);
      },
    });
  });
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
    const canvas = this.data.canvas;
    const buffer = await loadFileByRequest('https://pag.art/file/test.pag');
    if (!buffer) throw '加载失败';
    this.pagFile = await this.PAG.PAGFile.load(buffer);
    this.pagView = await this.PAG.PAGView.init(this.pagFile, canvas);
    this.setData({ pagLoaded: true });
    wx.hideLoading();
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
    if (!this.textDoc) {
      this.textDoc = this.pagFile.getTextData(0);
    }
    this.textDoc.text = event.detail.value;
    this.pagFile.replaceText(0, this.textDoc);
    this.pagView.flush();
  },
  async replaceImage() {
    const canvas = this.data.canvas;
    const image = await new Promise((resolve) => {
      wx.getImageInfo({
        src: 'https://pag.art/img/ae.png',
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
});
