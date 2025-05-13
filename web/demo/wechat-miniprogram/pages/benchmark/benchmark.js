import { PAGInit } from '../../utils/libpag';
import { loadFileByRequest } from '../../utils/request';

const origin = 'https://pag.io/file/';

Page({
  data: {
    pagMap: ['test.pag', 'test.pag', 'test.pag', 'test.pag', 'test.pag', 'test.pag'],
  },
  async onReady() {
    this.PAG = await PAGInit({
      locateFile: (file) => '/utils/' + file,
    });

    for (let index = 0; index < this.data.pagMap.length; index++) {
      const buffer = await loadFileByRequest(`${origin}${this.data.pagMap[index]}`);
      const pagFile = await this.PAG.PAGFile.load(buffer);
      const pagView = await this.PAG.PAGView.init(pagFile, `pag-${index}`, { firstFrame: true }); // 建议传入wxml中定义的canvasId
      pagView.setRepeatCount(0);
      pagView.addListener('onAnimationUpdate', () => {
        wx.createSelectorQuery()
          .select(`#fps-${index}`)
          .fields({ node: true, size: true })
          .exec((res) => {
            const canvas = res[0].node;
            const ctx = canvas.getContext('2d');
            const dpr = wx.getSystemInfoSync().pixelRatio;
            canvas.width = res[0].width * dpr;
            canvas.height = res[0].height * dpr;
            ctx.scale(dpr, dpr);
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            ctx.font = '20px Arial';
            ctx.fillText(pagView.getDebugData().FPS, 0, 20);
          });
      });
      pagView.play();
    }
  },
});
