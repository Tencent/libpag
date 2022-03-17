import { PAGInit } from '../src/pag';
import { PAGFile } from '../src/pag-file';
import { PAGView } from '../src/pag-view';
import { AudioPlayer } from './module/audio-player';
import { LayerType, PAG as PAGNamespace, PAGViewListenerEvent, ParagraphJustification } from '../src/types';
import { PAGComposition } from '../src/pag-composition';

declare global {
  interface Window {
    VConsole: any;
  }
}

let pagView: PAGView = null;
let pagFile: PAGFile = null;
let cacheEnabled: boolean;
let videoEnabled: boolean;
let globalCacheScale: number;
let videoEl: HTMLVideoElement = null;
let pagComposition: PAGComposition = null;
let audioEl: AudioPlayer = null;
let PAG: PAGNamespace;
let canvasElementSize = 640;
let isMobile = false;

window.onload = async () => {
  PAG = await PAGInit({ locateFile: (file) => '../lib/' + file });
  // Mobile
  isMobile = /Mobi|Android|iPhone/i.test(navigator.userAgent);
  if (isMobile) {
    document
      .querySelector('meta[name="viewport"]')
      ?.setAttribute(
        'content',
        'viewport-fit=cover, width=device-width, initial-scale=1.0, minimum-scale=1.0, maximum-scale=1.0, user-scalable=no',
      );
    await loadScript('https://unpkg.com/vconsole@latest/dist/vconsole.min.js');
    const vconsole = new window.VConsole();
    canvasElementSize = 320;
    const canvas = document.getElementById('pag') as HTMLCanvasElement;
    canvas.width = canvasElementSize;
    canvas.height = canvasElementSize;
    const tablecloth = document.getElementById('tablecloth');
    tablecloth.style.width = `${canvasElementSize}px`;
    tablecloth.style.height = `${canvasElementSize}px`;
  }

  console.log('wasm loaded!', PAG);

  document.getElementById('waiting').style.display = 'none';
  document.getElementById('container').style.display = isMobile ? 'block' : '';

  // 加载测试字体
  document.getElementById('btn-test-font').addEventListener('click', () => {
    const url = './assets/SourceHanSerifCN-Regular.ttf';
    fetch(url)
      .then((response) => response.blob())
      .then(async (blob) => {
        const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
        await PAG.PAGFont.registerFont('SourceHanSerifCN', file);
        console.log(`已加载${file.name}`);
      });
  });

  // 加载PAG
  document.getElementById('btn-upload-pag').addEventListener('click', () => {
    document.getElementById('upload-pag').click();
  });
  document.getElementById('upload-pag').addEventListener('change', (event) => {
    createPAGView((event.target as HTMLInputElement).files[0]);
  });
  document.getElementById('btn-test-vector-pag').addEventListener('click', () => {
    const url = './assets/like.pag';
    fetch(url)
      .then((response) => response.blob())
      .then((blob) => {
        const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
        createPAGView(file);
      });
  });
  document.getElementById('btn-test-video-pag').addEventListener('click', () => {
    const url = './assets/particle_video.pag';
    fetch(url)
      .then((response) => response.blob())
      .then((blob) => {
        const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
        createPAGView(file);
      });
  });
  document.getElementById('btn-test-text-pag').addEventListener('click', async () => {
    const url = './assets/test2.pag';
    const response = await fetch(url);
    const blob = await response.blob();
    const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
    await createPAGView(file);
    const textDoc = pagFile.getTextData(0);
    console.log(textDoc);
    textDoc.text = '替换后的文字🤔';
    textDoc.fillColor = { red: 255, green: 255, blue: 255 };
    textDoc.applyFill = true;
    textDoc.backgroundAlpha = 100;
    textDoc.backgroundColor = { red: 255, green: 0, blue: 0 };
    textDoc.baselineShift = 200;
    textDoc.fauxBold = true;
    textDoc.fauxItalic = false;
    textDoc.fontFamily = 'SourceHanSerifCN';
    textDoc.fontSize = 100;
    textDoc.justification = ParagraphJustification.CenterJustify;
    textDoc.strokeWidth = 20;
    textDoc.strokeColor = { red: 0, green: 0, blue: 0 };
    textDoc.applyStroke = true;
    textDoc.strokeOverFill = true;
    textDoc.tracking = 600;
    pagFile.replaceText(0, textDoc);
    console.log(pagFile.getTextData(0));
    await pagView.flush();
  });

  // Get PAGFile duration
  document.getElementById('btn-pagfile-get-duration').addEventListener('click', () => {
    const duration = pagFile.duration();
    console.log(`PAGFile duration ${duration}`);
  });

  // PAGFile setDuration
  document.getElementById('btn-pagfile-set-duration').addEventListener('click', () => {
    const duration = Number((document.getElementById('input-pagfile-duration') as HTMLInputElement).value);
    pagFile.setDuration(duration);
    console.log(`Set PAGFile duration ${duration} `);
  });

  // Get timeStretchMode
  document.getElementById('btn-pagfile-time-stretch-mode').addEventListener('click', () => {
    const timeStretchMode = pagFile.timeStretchMode();
    console.log(`PAGFile timeStretchMode ${timeStretchMode} `);
  });

  document.getElementById('btn-pagfile-set-time-stretch-mode').addEventListener('click', () => {
    const mode = Number((document.getElementById('select-time-stretch-mode') as HTMLSelectElement).value);
    pagFile.setTimeStretchMode(mode);
    console.log(`Set PAGFile timeStretchMode ${mode}`);
  });

  // 控制
  document.getElementById('btn-play').addEventListener('click', () => {
    pagView.play();
    audioEl.play();
    console.log('开始');
  });
  document.getElementById('btn-pause').addEventListener('click', () => {
    pagView.pause();
    audioEl.pause();
    console.log('暂停');
  });
  document.getElementById('btn-stop').addEventListener('click', () => {
    pagView.stop();
    audioEl.stop();
    console.log('停止');
  });
  document.getElementById('btn-destroy').addEventListener('click', () => {
    pagView.destroy();
    audioEl.destroy();
    console.log('销毁');
  });

  // 获取进度
  document.getElementById('btn-getProgress').addEventListener('click', () => {
    console.log(`当前进度：${pagView.getProgress()}`);
  });

  // 设置进度
  document.getElementById('setProgress').addEventListener('click', () => {
    let progress = Number((document.getElementById('progress') as HTMLInputElement).value);
    if (!(progress >= 0 && progress <= 1)) {
      alert('请输入0～1之间');
    }
    pagView.setProgress(progress);
    console.log(`已设置进度：${progress}`);
  });

  // 设置循环次数
  document.getElementById('setRepeatCount').addEventListener('click', () => {
    let repeatCount = Number((document.getElementById('repeatCount') as HTMLInputElement).value);
    pagView.setRepeatCount(repeatCount);
    console.log(`已设置循环次数：${repeatCount}`);
  });

  // maxFrameRate
  document.getElementById('btn-maxFrameRate').addEventListener('click', () => {
    console.log(`maxFrameRate: ${pagView.maxFrameRate()}`);
  });
  document.getElementById('setMaxFrameRate').addEventListener('click', () => {
    let maxFrameRate = Number((document.getElementById('maxFrameRate') as HTMLInputElement).value);
    pagView.setMaxFrameRate(maxFrameRate);
  });

  // scaleMode
  document.getElementById('btn-scaleMode').addEventListener('click', () => {
    console.log(`scaleMode: ${pagView.scaleMode()}`);
  });
  document.getElementById('setScaleMode').addEventListener('click', () => {
    let scaleMode = Number((document.getElementById('scaleMode') as HTMLSelectElement).value);
    pagView.setScaleMode(scaleMode);
  });

  // videoEnabled
  videoEnabled = true;
  document.getElementById('btn-videoEnabled').addEventListener('click', () => {
    videoEnabled = pagView.videoEnabled();
    console.log(`videoEnabled status: ${videoEnabled}`);
  });
  document.getElementById('btn-setVideoEnabled').addEventListener('click', () => {
    pagView.setVideoEnabled(!videoEnabled);
  });

  // cacheEnabled
  cacheEnabled = true;
  document.getElementById('btn-cacheEnabled').addEventListener('click', () => {
    cacheEnabled = pagView.cacheEnabled();
    console.log(`cacheEnabled status: ${cacheEnabled}`);
  });
  document.getElementById('btn-setCacheEnabled').addEventListener('click', () => {
    pagView.setCacheEnabled(!cacheEnabled);
  });

  // PAGComposition
  document.getElementById('btn-composition').addEventListener('click', () => {
    testPAGCompositionAPi();
  });

  // freeCache
  document.getElementById('btn-freeCache').addEventListener('click', () => {
    pagView.freeCache();
  });

  // cacheScale
  globalCacheScale = 1;
  document.getElementById('btn-cacheScale').addEventListener('click', () => {
    globalCacheScale = pagView.cacheScale();
    console.log(`cacheScale status: ${globalCacheScale}`);
  });
  document.getElementById('btn-setCacheScale').addEventListener('click', () => {
    let cacheScale = Number((document.getElementById('cacheScale') as HTMLInputElement).value);
    if (!(cacheScale >= 0 && cacheScale <= 1)) {
      alert('请输入0～1之间');
    }
    pagView.setCacheScale(cacheScale);
  });
};

const existsLayer = (pagLayer: object) => {
  if (pagLayer) return true;
  console.log('no Layer');
  return false;
};

// PAGComposition api test
const testPAGComposition = {
  rect: () => {
    console.log(`test result: width: ${pagComposition.width()}, height: ${pagComposition.height()}`);
  },
  setContentSize: () => {
    pagComposition.setContentSize(360, 640);
    console.log(`test setContentSize result: width: ${pagComposition.width()}, height: ${pagComposition.height()}`);
  },
  numChildren: () => {
    console.log(`test numChildren: ${pagComposition.numChildren()}`);
  },
  getLayerAt: () => {
    const pagLayer = pagComposition.getLayerAt(0);
    if (!existsLayer(pagLayer)) return;
    console.log(`test getLayerAt index 0, layerName: ${pagLayer.layerName()}`);
  },
  getLayersByName: () => {
    const pagLayer = pagComposition.getLayerAt(0);
    if (!existsLayer(pagLayer)) return;
    const layerName = pagLayer.layerName();
    const vectorPagLayer = pagComposition.getLayersByName(layerName);
    for (let j = 0; j < vectorPagLayer.size(); j++) {
      const pagLayerWasm = vectorPagLayer.get(j);
      const pagLayer_1 = new PAG.PAGLayer(pagLayerWasm);
      console.log(`test getLayersByName: layerName: ${pagLayer_1.layerName()}`);
    }
  },
  audioStartTime: () => {
    const audioStartTime = pagComposition.audioStartTime();
    console.log('test audioStartTime:', audioStartTime);
  },
  audioMarkers: () => {
    const audioMarkers = pagComposition.audioMarkers();
    console.log(`test audioMarkers: size`, audioMarkers.size());
  },
  audioBytes: () => {
    const audioBytes = pagComposition.audioBytes();
    console.log('test audioBytes：', audioBytes);
  },
  getLayerIndex: () => {
    const pagLayer = pagComposition.getLayerAt(0);
    const index = pagComposition.getLayerIndex(pagLayer);
    console.log(`test GetLayerIndex: ${index}`);
  },
  swapLayerAt: () => {
    swapLayer('swapLayerAt');
  },
  swapLayer: () => {
    swapLayer('swapLayer');
  },
  contains: () => {
    const pagLayer = pagComposition.getLayerAt(0);
    const isContains = pagComposition.contains(pagLayer);
    if (isContains) {
      console.log('test contains');
    }
  },
  addLayer: () => {
    const pagLayer = pagComposition.getLayerAt(0);
    pagComposition.removeLayerAt(0);
    const oldNum = pagComposition.numChildren();
    const isSuccess: boolean = pagComposition.addLayer(pagLayer);
    if (isSuccess) {
      console.log(`test addLayer success: old num ${oldNum} current num ${pagComposition.numChildren()}`);
    }
  },
  removeLayerAt: () => {
    const oldNum = pagComposition.numChildren();
    pagComposition.removeLayerAt(0);
    console.log(
      `test delete Layer[0] success: old LayersNum: ${oldNum} current LayersNum ${pagComposition.numChildren()}`,
    );
  },
  removeAllLayers: () => {
    const oldNum = pagComposition.numChildren();
    pagComposition.removeAllLayers();
    console.log(
      `test removeAllLayers success: old LayersNum${oldNum} current LayersNum ${pagComposition.numChildren()}`,
    );
  },
};
const testPAGCompositionAPi = () => {
  console.log(`-------------------TEST PAGCompositionAPI START--------------------- `);
  for (let i in testPAGComposition) {
    if (testPAGComposition.hasOwnProperty(i)) {
      testPAGComposition[i]();
    }
  }
  console.log(`-------------------TEST PAGCompositionAPI END--------------------- `);
};

const swapLayer = (type: string) => {
  const pagLayer_0 = pagComposition.getLayerAt(0);
  const pagLayer_1 = pagComposition.getLayerAt(1);
  if (!pagLayer_0 || !pagLayer_1) {
    console.log('No layer switching');
    return;
  }
  const pagLayer_name_0 = pagLayer_0.layerName();
  const pagLayer_name_1 = pagLayer_1.layerName();
  if (type === 'swapLayer') {
    pagComposition.swapLayer(pagLayer_0, pagLayer_1);
  } else {
    pagComposition.swapLayerAt(0, 1);
  }
  const pagLayer_exch_0 = pagComposition.getLayerAt(0);
  const pagLayer_exch_1 = pagComposition.getLayerAt(1);
  const pagLayer__exch_0 = pagLayer_exch_0.layerName();
  const pagLayer__exch_1 = pagLayer_exch_1.layerName();
  console.log(
    `test ${type}:  oldLayerName_0=${pagLayer_name_0}, oldLayerName_1=${pagLayer_name_1} exchange LayerName_0=${pagLayer__exch_0}, LayerName_1=${pagLayer__exch_1} `,
  );
};

const createPAGView = async (file) => {
  if (pagFile) pagFile.destroy();
  if (pagView) pagView.destroy();
  const decodeTime = performance.now();
  pagFile = await PAG.PAGFile.load(file);
  document.getElementById('decode-time').innerText = `PAG File decode time: ${Math.floor(
    performance.now() - decodeTime,
  )}ms`;
  const pagCanvas = document.getElementById('pag') as HTMLCanvasElement;
  pagCanvas.width = canvasElementSize;
  pagCanvas.height = canvasElementSize;
  const initializedTime = performance.now();
  pagView = await PAG.PAGView.init(pagFile, pagCanvas);
  document.getElementById('initialized-time').innerText = `PAG View initialized time: ${Math.floor(
    performance.now() - initializedTime,
  )}ms`;
  pagView.setRepeatCount(0);
  // 绑定事件监听
  pagView.addListener(PAGViewListenerEvent.onAnimationStart, (event) => {
    console.log('onAnimationStart', event);
  });
  pagView.addListener(PAGViewListenerEvent.onAnimationEnd, (event) => {
    console.log('onAnimationEnd', event);
  });
  pagView.addListener(PAGViewListenerEvent.onAnimationCancel, (event) => {
    console.log('onAnimationCancel', event);
  });
  pagView.addListener(PAGViewListenerEvent.onAnimationRepeat, (event) => {
    console.log('onAnimationRepeat', event);
    audioEl.stop();
    audioEl.play();
  });
  let lastProgress = 0;
  let lastFlushedTime = null;
  let flushCount = 0; // Every 3 times update FPSinfo.
  pagView.addListener(PAGViewListenerEvent.onAnimationPlay, (event) => {
    console.log('onAnimationPlay', event);
    lastFlushedTime = performance.now();
  });
  pagView.addListener(PAGViewListenerEvent.onAnimationPause, (event) => {
    console.log('onAnimationPause', event);
  });
  pagView.addListener(PAGViewListenerEvent.onAnimationFlushed, (pagView: PAGView) => {
    // console.log('onAnimationFlushed', pagView);
    const progress = pagView.getProgress();
    const time = performance.now();
    if (progress !== lastProgress) {
      flushCount += 1;
      lastProgress = progress;
    }
    if (flushCount === 3) {
      document.getElementById('fps').innerText = `PAG View FPS: ${Math.floor(1000 / ((time - lastFlushedTime) / 3))}`;
      lastFlushedTime = time;
      flushCount = 0;
    }
  });
  document.getElementById('control').style.display = '';
  // 图层编辑
  const editableLayers = getEditableLayer(PAG, pagFile);
  renderEditableLayer(editableLayers);
  console.log(`已加载 ${file.name}`);
  pagComposition = pagView.getComposition();
  audioEl = new AudioPlayer(pagComposition.audioBytes());
  return pagView;
};

const loadVideoReady = (el, src) => {
  return new Promise((resolve) => {
    const listener = () => {
      el.removeEventListener('canplay', listener);
      console.log('canplay');
      resolve(true);
    };
    el.addEventListener('canplay', listener);
    el.src = src;
  });
};

const setVideoTime = (el, time) => {
  return new Promise((resolve) => {
    const listener = () => {
      el.removeEventListener('timeupdate', listener);
      console.log('timeupdate');
      resolve(true);
    };
    el.addEventListener('timeupdate', listener);
    el.currentTime = time;
  });
};

const getEditableLayer = (PAG: PAGNamespace, pagFile: PAGFile) => {
  const editableImageCount = pagFile.numImages();
  let res: any[] = [];
  for (let i = 0; i < editableImageCount; i++) {
    const imageLayers = pagFile.getLayersByEditableIndex(i, LayerType.Image);
    imageLayers.forEach((layer) => {
      const uniqueID = layer.uniqueID();
      const layerType = layer.layerType();
      const layerName = layer.layerName();
      const alpha = layer.alpha();
      const visible = layer.visible();
      const editableIndex = layer.editableIndex();
      const duration = layer.duration();
      const frameRate = layer.frameRate();
      const localStartTime = layer.startTime();
      const startTime = layer.localTimeToGlobal(localStartTime);
      res.push({ uniqueID, layerType, layerName, alpha, visible, editableIndex, frameRate, startTime, duration });
    });
  }
  return res;
};

const renderEditableLayer = (editableLayers) => {
  const editLayerContent = document.getElementById('editLayer-content');
  const childNodes = editLayerContent.childNodes;
  if (childNodes.length > 0) {
    childNodes.forEach((node) => editLayerContent.removeChild(node));
  }
  const box = document.createElement('div');
  box.className = 'mt-24';
  box.innerText = 'Editable layer:';
  editableLayers.forEach((layer) => {
    const item = document.createElement('div');
    item.className = 'mt-24';
    item.innerText = `editableIndex: ${layer.editableIndex} startTime: ${layer.startTime} duration: ${layer.duration}`;
    const replaceImageBtn = document.createElement('button');
    replaceImageBtn.addEventListener('click', () => {
      replaceImage(item, layer.editableIndex);
    });
    replaceImageBtn.style.marginLeft = '12px';
    replaceImageBtn.innerText = '替换图片';
    item.appendChild(replaceImageBtn);
    const replaceVideoBtn = document.createElement('button');
    replaceVideoBtn.addEventListener('click', () => {
      replaceVideo(item, layer.editableIndex);
    });
    replaceVideoBtn.style.marginLeft = '12px';
    replaceVideoBtn.innerText = '替换视频';
    item.appendChild(replaceVideoBtn);
    box.appendChild(item);
  });
  editLayerContent.appendChild(box);
};

// 替换图片
const replaceImage = (element, index) => {
  const inputEl = document.createElement('input');
  inputEl.type = 'file';
  inputEl.style.display = 'none';
  element.appendChild(inputEl);
  inputEl.addEventListener('change', async (event) => {
    const pagImage = await PAG.PAGImage.fromFile((event.target as HTMLInputElement).files[0]);
    const pagFile = pagView.getComposition();
    pagFile.replaceImage(index, pagImage);
    await pagView.flush();
    pagImage.destroy();
  });
  inputEl.click();
  element.removeChild(inputEl);
};

// 替换视频
const replaceVideo = (element, index) => {
  const inputEl = document.createElement('input');
  inputEl.type = 'file';
  inputEl.style.display = 'none';
  element.appendChild(inputEl);
  inputEl.addEventListener('change', async (event) => {
    if (!videoEl) videoEl = document.createElement('video');
    await loadVideoReady(videoEl, URL.createObjectURL((event.target as HTMLInputElement).files[0]));
    await setVideoTime(videoEl, 0.05);
    const pagImage = PAG.PAGImage.fromSource(videoEl);
    const pagFile = pagView.getComposition();
    pagFile.replaceImage(index, pagImage);
    await pagView.flush();
    pagImage.destroy();
  });
  inputEl.click();
  element.removeChild(inputEl);
};

const loadScript = (url) => {
  return new Promise((resolve, reject) => {
    const scriptEl = document.createElement('script');
    scriptEl.type = 'text/javascript';
    scriptEl.onload = () => {
      resolve(true);
    };
    scriptEl.onerror = (e) => {
      reject(e);
    };
    scriptEl.src = url;
    document.body.appendChild(scriptEl);
  });
};
