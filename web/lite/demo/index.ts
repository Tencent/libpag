import { PAGView, types } from '../src/pag';

window.onload = async () => {
  const arrayBuffer = await fetch('./assets/frames.pag').then((res) => res.arrayBuffer());
  const canvas = document.getElementById('pag') as HTMLCanvasElement;
  canvas.width = 720;
  canvas.height = 720;
  const pagView = PAGView.init(arrayBuffer, canvas, {
    renderingMode: types.RenderingMode.WebGL,
  });
  pagView.addListener(types.EventName.onAnimationUpdate, () => {
    console.log(pagView.getProgress());
  });
  pagView.play();
};
