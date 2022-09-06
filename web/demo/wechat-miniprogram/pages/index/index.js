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
    const buffer = await loadFileByRequest('https://pag.art/file/particle_video.pag');
    if (!buffer) throw '加载失败';
    this.pagFile = await this.PAG.PAGFile.load(buffer);
    this.pagView = await this.PAG.PAGView.init(this.pagFile, canvas);
    console.log('pagView', this.pagView);
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
  videoErrorCallback(e) {
    console.log('视频错误信息:');
    console.log(e.detail.errMsg);
  }
});
