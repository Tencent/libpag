import {
  createPAGWorker,
  MAX_ACTIVE_WORKER_CONTEXTS,
  WorkerPAGFile,
  WorkerPAGImage,
  WorkerPAGView,
} from '../src/worker/client';

window.onload = async () => {
  const PAG_TOTAL_NUM = Math.min(4, 16);
  let PAG_NUM_IN_WORKER = Math.min(2, MAX_ACTIVE_WORKER_CONTEXTS);

  const url = './assets/test.pag';
  const buffer = await fetch(url).then((res) => res.arrayBuffer());
  const pagFilesPromises = [];
  let count = 0;
  let currentPAGWorker: Worker | null = null;

  for (let index = 0; index < PAG_TOTAL_NUM; index++) {
    if (count % PAG_NUM_IN_WORKER === 0) {
      /**
       * Create a new worker for PAG.
       * libpag.js is the PAG core script.
       * You can configure libpag.js path to select script to run in worker.
       */
      currentPAGWorker = await createPAGWorker({
        locateFile: (file) => {
          if (file === 'libpag.wasm') {
            return '../lib/libpag.wasm';
          }
          if (file === 'libpag.js') {
            return './libpag.js';
          }
          return '../lib/' + file;
        },
      });
    }
    pagFilesPromises.push(WorkerPAGFile.load(currentPAGWorker as Worker, buffer));
    count++;
  }

  const pagFiles = await Promise.all(pagFilesPromises);
  const textData = await pagFiles[0].getTextData(0);
  textData.text = 'Hello World!';
  await pagFiles[0].replaceText(0, textData);

  const canvases = pagFiles.map(() => {
    const canvas = document.createElement('canvas');
    canvas.width = 300;
    canvas.height = 300;
    document.body.appendChild(canvas);
    return canvas;
  });

  const image = await new Promise<HTMLImageElement>((resolve) => {
    const img = new Image();
    img.onload = () => {
      resolve(img);
    };
    img.src = './assets/cat.png';
  });
  const pagImage = await WorkerPAGImage.fromSource(pagFiles[0].worker, image);
  pagFiles[0].replaceImage(0, pagImage);
  pagImage.destroy();

  const pagViews = await Promise.all(
    pagFiles.map((pagFile, index) => WorkerPAGView.init(pagFile, canvases[index], { firstFrame: false })),
  );
  pagViews.forEach((pagView) => {
    pagView.setRepeatCount(0);
    pagView.play();
  });

  console.log(`duration: ${await pagViews[0].duration()}us`);
  console.log(`progress: ${await pagViews[0].getProgress()}`);
  console.log(`currentFrame: ${await pagViews[0].currentFrame()}`);
  pagViews[0].setScaleMode(3);
  console.log(`scaleMode: ${await pagViews[0].scaleMode()}`);

  setTimeout(async () => {
    try {
      await pagViews[0].stop();
      await pagFiles[0].destroy();
      await pagViews[0].destroy();
      await textData.delete();
    } catch (e) {
      console.log(e);
    }
  }, 5000);
  setTimeout(() => {
    pagViews[1].pause();
  }, 5000);
};
