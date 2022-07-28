// index.js
import { PAGView, types } from '../../utils/pag-wx.esm';

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
    pagView: null,
    pagLoaded: false,
    repeatCount: 0,
    scaleMap: ['None', 'Stretch', 'LetterBox', 'Zoom'],
    scaleIndex: 2,
    progress: 0,
  },
  async load() {
    const canvasQuery = wx.createSelectorQuery();
    canvasQuery
      .select('#pag')
      .node()
      .exec(async (res) => {
        wx.showLoading({ title: '加载中' });
        const canvas = res[0].node;

        const buffer = await loadFileByRequest('https://tencent-effect-1251316161.cos.ap-shanghai.myqcloud.com/particle_video.pag');
        if (!buffer) throw('加载失败');
        this.data.pagView = PAGView.init(buffer, canvas);
        console.log('pagView', this.data.pagView);
        this.setData({ pagLoaded: true });
        wx.hideLoading();
      });
  },
  setProgress() {
    this.data.pagView.setProgress(this.data.progress);
  },
  play() {
    this.data.pagView.play();
  },
  pause() {
    this.data.pagView.pause();
  },
  stop() {
    this.data.pagView.stop();
  },
  destroy() {
    this.data.pagView.destroy();
  },
  clearCache() {
    this.data.pagView.clearCache();
  },
  scalePickerChange(event) {
    this.setData({ scaleIndex: event.detail.value });
  },
  repeatCountChange(event) {
    this.setData({ repeatCount: event.detail.value });
    this.data.pagView.setRepeatCount(event.detail.value);
  },
  progressChange(event) {
    this.setData({ progress: event.detail.value });
  },
});
