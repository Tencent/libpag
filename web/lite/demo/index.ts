import { PAGView, types } from '../src/pag';
import type { DebugData } from '../src/types';

const updateDebugInfo = (debugData: DebugData) => {
  document.getElementById('info')!.innerText = `调试数据：
    FPS：${Math.round(debugData.FPS as number)}
    当前帧渲染耗时：${debugData.draw?.toFixed(3)}ms
    PAG文件解码耗时：${debugData.decodePAGFile?.toFixed(3)}ms
    合成MP4耗时：${debugData.coverMP4?.toFixed(3)}ms
  `;
};

window.onload = async () => {
  const arrayBuffer = await fetch('./assets/frames.pag').then((res) => res.arrayBuffer());
  const canvas = document.getElementById('pag') as HTMLCanvasElement;
  const pagView = PAGView.init(arrayBuffer, canvas, {
    renderingMode: types.RenderingMode.WebGL,
  });
  updateDebugInfo(pagView.getDebugData());
  pagView.addListener(types.EventName.onAnimationUpdate, () => {
    updateDebugInfo(pagView.getDebugData());
  });

  document.getElementById('play')?.addEventListener('click', () => {
    pagView.play();
  });
  document.getElementById('pause')?.addEventListener('click', () => {
    pagView.pause();
  });
  document.getElementById('stop')?.addEventListener('click', () => {
    pagView.stop();
  });
  document.getElementById('destroy')?.addEventListener('click', () => {
    pagView.destroy();
  });
};
