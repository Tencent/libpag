// index.js
import { PAGView, clearCache } from '../../utils/pag';

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
    repeatCount: 1,
    scaleMap: ['None', 'Stretch', 'LetterBox', 'Zoom'],
    scaleIndex: 2,
    progress: 0,
    debugText: '',
  },
  async load() {
    const canvasQuery = wx.createSelectorQuery();
    canvasQuery
      .select('#pag')
      .node()
      .exec(async (res) => {
        wx.showLoading({ title: '加载中' });
        const canvas = res[0].node;
        const buffer = await loadFileByRequest('https://pag.art/file/frames.pag');
        if (!buffer) throw '加载失败';
        const pagView = PAGView.init(buffer, canvas);
        this.setData({ pagView: pagView, pagLoaded: true });
        const debugData = pagView.getDebugData();
        this.updateDebugData(debugData);
        pagView.addListener('onAnimationUpdate', () => {
          const debugData = pagView.getDebugData();
          this.updateDebugData(debugData);
        });
        wx.hideLoading();
      });
  },
  clear() {
    if (clearCache()) {
      console.log('清理成功');
    }
  },
  play() {
    this.data.pagView.setRepeatCount(this.data.repeatCount);
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
  scalePickerChange(event) {
    this.setData({ scaleIndex: event.detail.value });
    this.data.pagView.setScaleMode(this.data.scaleMap[event.detail.value]);
  },
  repeatCountChange(event) {
    this.setData({ repeatCount: event.detail.value });
  },
  progressChange(event) {
    this.setData({ progress: event.detail.value });
    this.data.pagView.setProgress(this.data.progress);
    this.data.pagView.flush();
  },
  updateDebugData(debugData) {
    const text = `
      调试数据：
      FPS：${Math.round(debugData.FPS)}
      当前帧获取耗时：${Math.round(debugData.getFrame * 100) / 100}ms
      当前帧渲染耗时：${Math.round(debugData.draw * 100) / 100}ms
      PAG文件解码耗时：${Math.round(debugData.decodePAGFile * 100) / 100}ms
      创建目录耗时：${Math.round(debugData.createDir * 100) / 100}ms
      合成MP4耗时：${Math.round(debugData.coverMP4 * 100) / 100}ms
      写入文件耗时：${Math.round(debugData.writeFile * 100) / 100}ms
      创建解码器耗时：${Math.round(debugData.createDecoder * 100) / 100}ms
    `;
    this.setData({ debugText: text });
  },
});
