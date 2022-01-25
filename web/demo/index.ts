import { PAGInit } from '../src/pag';
import { PAGFile } from '../src/pag-file';
import { Log } from '../src/utils/log';

import { PAG as PAGNamespace, ParagraphJustification } from '../src/types';

let pagView = null;
let pagComposition = null;
let pagFile: PAGFile = null;
let cacheEnabled: boolean;
let videoEnabled: boolean;
let globalCacheScale: boolean;
let videoEl = null;
let PAG: PAGNamespace;

PAGInit({
  locateFile: (file) => '../lib/' + file,
}).then((_PAG) => {
  PAG = _PAG;
  console.log('wasm loaded!', PAG);
  document.getElementById('waiting').style.display = 'none';
  document.getElementById('container').style.display = '';

  // 加载Font
  document.getElementById('btn-upload-font').addEventListener('click', () => {
    document.getElementById('upload-font').click();
  });
  document.getElementById('upload-font').addEventListener('change', (event) => {
    const file = (event.target as HTMLInputElement).files[0];
    document.getElementById('upload-font-text').innerText = `已加载${file.name}`;
    // PAG.PAGFont.registerFont(file);
  });
  // 加载测试字体
  document.getElementById('btn-test-font').addEventListener('click', () => {
    const url = './assets/SourceHanSansSC-Normal.otf';
    fetch(url)
      .then((response) => response.blob())
      .then(async (blob) => {
        const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
        document.getElementById('upload-font-text').innerText = `已加载${file.name}`;
        await PAG.PAGFont.registerFont('SourceHanSansSC', file);
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
    const textDoc = await pagFile.getTextData(0);
    console.log(textDoc);
    textDoc.text = '替换后的文字';
    textDoc.fillColor = { red: 255, green: 255, blue: 255 };
    textDoc.applyFill = true;
    textDoc.backgroundAlpha = 100;
    textDoc.backgroundColor = { red: 255, green: 0, blue: 0 };
    textDoc.baselineShift = 200;
    textDoc.fauxBold = true;
    textDoc.fauxItalic = false;
    textDoc.fontFamily = 'PingFang SC';
    textDoc.fontSize = 100;
    textDoc.justification = ParagraphJustification.CenterJustify;
    textDoc.strokeWidth = 20;
    textDoc.strokeColor = { red: 0, green: 0, blue: 0 };
    textDoc.applyStroke = true;
    textDoc.strokeOverFill = true;
    textDoc.tracking = 600;
    pagFile.replaceText(0, textDoc);
    pagView.play();
  });

  // Get PAGFile duration
  document.getElementById('btn-pagfile-get-duration').addEventListener('click', async () => {
    const duration = await pagFile.duration();
    console.log(`PAGFile duration ${duration}`);
  });

  // PAGFile setDuration
  document.getElementById('btn-pagfile-set-duration').addEventListener('click', async () => {
    const duration = Number((document.getElementById('input-pagfile-duration') as HTMLInputElement).value);
    await pagFile.setDuration(duration);
    console.log(`Set PAGFile duration ${duration} `);
  });

  // Get timeStretchMode
  document.getElementById('btn-pagfile-time-stretch-mode').addEventListener('click', async () => {
    const timeStretchMode = await pagFile.timeStretchMode();
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
    console.log('开始');
  });
  document.getElementById('btn-pause').addEventListener('click', () => {
    pagView.pause();
    console.log('暂停');
  });
  document.getElementById('btn-stop').addEventListener('click', () => {
    pagView.stop();
    console.log('停止');
  });
  document.getElementById('btn-destroy').addEventListener('click', () => {
    pagView.destroy();
    console.log('销毁');
  });

  // 获取进度
  document.getElementById('btn-getProgress').addEventListener('click', async () => {
    console.log(`当前进度：${await pagView.getProgress()}`);
  });

  // 设置进度
  document.getElementById('setProgress').addEventListener('click', async () => {
    let progress = Number((document.getElementById('progress') as HTMLInputElement).value);
    if (!(progress >= 0 && progress <= 1)) {
      alert('请输入0～1之间');
    }
    await pagView.setProgress(progress);
    console.log(`已设置进度：${progress}`);
  });

  // 设置循环次数
  document.getElementById('setRepeatCount').addEventListener('click', () => {
    let repeatCount = Number((document.getElementById('repeatCount') as HTMLInputElement).value);
    pagView.setRepeatCount(repeatCount);
    console.log(`已设置循环次数：${repeatCount}`);
  });

  // maxFrameRate
  document.getElementById('btn-maxFrameRate').addEventListener('click', async () => {
    console.log(`maxFrameRate: ${await pagView.maxFrameRate()}`);
  });
  document.getElementById('setMaxFrameRate').addEventListener('click', () => {
    let maxFrameRate = Number((document.getElementById('maxFrameRate') as HTMLInputElement).value);
    pagView.setMaxFrameRate(maxFrameRate);
  });

  // scaleMode
  document.getElementById('btn-scaleMode').addEventListener('click', async () => {
    console.log(`scaleMode: ${await pagView.scaleMode()}`);
  });
  document.getElementById('setScaleMode').addEventListener('click', () => {
    let scaleMode = Number((document.getElementById('scaleMode') as HTMLSelectElement).value);
    pagView.setScaleMode(scaleMode);
  });

  // videoEnabled
  videoEnabled = true;
  document.getElementById('btn-videoEnabled').addEventListener('click', async () => {
    videoEnabled = await pagView.videoEnabled();
    console.log(`videoEnabled status: ${videoEnabled}`);
  });
  document.getElementById('btn-setVideoEnabled').addEventListener('click', () => {
    pagView.setVideoEnabled(!videoEnabled);
  });

  // cacheEnabled
  cacheEnabled = true;
  document.getElementById('btn-cacheEnabled').addEventListener('click', async () => {
    cacheEnabled = await pagView.cacheEnabled();
    console.log(`cacheEnabled status: ${cacheEnabled}`);
  });
  document.getElementById('btn-setCacheEnabled').addEventListener('click', () => {
    pagView.setCacheEnabled(!cacheEnabled);
  });

  // freeCache
  document.getElementById('btn-freeCache').addEventListener('click', () => {
    pagView.freeCache();
  });

  // cacheScale
  globalCacheScale = true;
  document.getElementById('btn-cacheScale').addEventListener('click', async () => {
    globalCacheScale = await pagView.cacheScale();
    console.log(`cacheScale status: ${globalCacheScale}`);
  });
  document.getElementById('btn-setCacheScale').addEventListener('click', () => {
    let cacheScale = Number((document.getElementById('cacheScale') as HTMLInputElement).value);
    if (!(cacheScale >= 0 && cacheScale <= 1)) {
      alert('请输入0～1之间');
    }
    pagView.setCacheScale(cacheScale);
  });

  // PAGComposition
  // width
  document.getElementById('btn-getWidth').addEventListener('click', async () => {
     Log.log(`width: ${await pagComposition.width()}`)
  });
    // height
  document.getElementById('btn-getHeight').addEventListener('click', async () => {
      Log.log(`height: ${await pagComposition.height()}`)
    });
  // setContentSize
  document.getElementById('btn-setContentSize').addEventListener('click', async () => {
    Log.log(`width: ${await pagComposition.width()}, height: ${await pagComposition.height()}`)
    Log.log(`change Rect`);
    pagComposition.setContentSize(360, 640);
    Log.log(`width: ${await pagComposition.width()}, height: ${await pagComposition.height()}`)
  });
  // btn-numChildren
  document.getElementById('btn-numChildren').addEventListener('click', async () => {
    Log.log(`numChildren: ${await pagComposition.numChildren()}`)
  });
  // btn-getLayerAt
  document.getElementById('btn-getLayerAt').addEventListener('click', async () => {
    const pagLayerWasm = await pagComposition.getLayerAt(0);
    if(!existsLayer(pagLayerWasm)) return;
    const pagLayer = new PAG.PAGLayer(pagLayerWasm);
    Log.log(`getLayerAt index 0, layerName: ${await pagLayer.layerName()}`)
  });
  // btn-getLayerIndex
  document.getElementById('btn-getLayerIndex').addEventListener('click', async () => {
    const pagLayerWasm = await pagComposition.getLayerAt(0);
    if(!existsLayer(pagLayerWasm)) return;
    const index = await pagComposition.getLayerIndex(pagLayerWasm);
    Log.log(`GetLayerIndex: ${index}`);
  });
  // btn-getLayerIndex
  document.getElementById('btn-swapLayerAt').addEventListener('click', async () => {
    swapLayer('swapLayerAt');
  });
  // btn-getLayerIndex
  document.getElementById('btn-swapLayer').addEventListener('click', async () => {
    swapLayer('swapLayer');
  });
  // audioStartTime
  document.getElementById('btn-audioStartTime').addEventListener('click', async () => {
    const audioStartTime = await pagComposition.audioStartTime();
    Log.log('audioStartTime:', audioStartTime);
  });
  // contains
  document.getElementById('btn-contains').addEventListener('click', async () => {
      const pagLayerWasm = await pagComposition.getLayerAt(0);
      if(!existsLayer(pagLayerWasm)) return;
      const isContains = await pagComposition.contains(pagLayerWasm);
      if(isContains){
        Log.log('is contains');
      }
      await pagComposition.removeLayerAt(0);
      const isNotContains = await pagComposition.contains(pagLayerWasm);
      if(!isNotContains){
        Log.log('is not Contains');
      }
    });
  // removeLayerAt
  document.getElementById('btn-removeLayerAt').addEventListener('click', async () => {
    Log.log(`Layers num: ${await pagComposition.numChildren()}`)
    Log.log('start removeLayerAt index 0');
    await pagComposition.removeLayerAt(0);
    Log.log('delete Layer[0] success: Layers num: ', await pagComposition.numChildren());
  });
  // removeAllLayers
  document.getElementById('btn-removeAllLayers').addEventListener('click', async () => {
    Log.log(`Layers num: ${await pagComposition.numChildren()}`)
    Log.log('start removeAllLayers index 0');
    await pagComposition.removeAllLayers();
    Log.log('removeAllLayers success: Layers num: ', await pagComposition.numChildren());
  });

  // addLayer
  document.getElementById('btn-addLayer').addEventListener('click', async () => {
    const pagLayerWasm = await pagComposition.getLayerAt(0);
    if(!existsLayer(pagLayerWasm)) return;
    await pagComposition.removeLayerAt(0);
    Log.log(`Layers num: ${await pagComposition.numChildren()}`);
    const isSuccess: boolean = await pagComposition.addLayer(pagLayerWasm);
    if(isSuccess){
      Log.log(`addLayer success  num: ${await pagComposition.numChildren()}`);
      return;
    }
    Log.log(`addLayer fail`);
  });
  
  // addLayerAt
  document.getElementById('btn-addLayerAt').addEventListener('click', async () => {
    const pagLayerWasm = await pagComposition.getLayerAt(0);
    if(!existsLayer(pagLayerWasm)) return;
    await pagComposition.removeLayerAt(0);
    const pagLayer_1 = new PAG.PAGLayer(pagLayerWasm);
    Log.log(`numChildren : ${await pagComposition.numChildren()},  template layerName: ${await pagLayer_1.layerName()}`);
    const isSuccess: boolean =  await pagComposition.addLayerAt(pagLayerWasm, 0);
    if(isSuccess){
      const pagLayerWasm_1 = await pagComposition.getLayerAt(0);
      const pagLayer = new PAG.PAGLayer(pagLayerWasm_1);
      Log.log(`addLayer success numChildren: ${await pagComposition.numChildren()}, add layerName: ${await pagLayer.layerName()}`);
      return ;
    }
    Log.log(`addLayer fail`);
  });
});

const existsLayer = (pagLayerWasm: object) =>{
  if(pagLayerWasm) return true;
  Log.log('no Layer');
  return false;
}

const swapLayer = async (type: string) => {
  const pagLayerWasm_0 = await pagComposition.getLayerAt(0);
  const pagLayerWasm_1 = await pagComposition.getLayerAt(1);
  if (!pagLayerWasm_0 || !pagLayerWasm_1) {
    Log.log('No layer switching');
    return;
  };
  const pagLayer_0 = new PAG.PAGLayer(pagLayerWasm_0);
  const pagLayer_1 = new PAG.PAGLayer(pagLayerWasm_1);
  Log.log(`layerName0 : ${await pagLayer_0.layerName()}, layerName1 : ${await pagLayer_1.layerName()}`);
  Log.log(`start ${type}...`);
  if (type === 'swapLayer') {
    await pagComposition.swapLayer(pagLayerWasm_0, pagLayerWasm_1);
  } else {
    await pagComposition.swapLayerAt(0, 1);
  }
  const pagLayerWasm_exch_0 = await pagComposition.getLayerAt(0);
  const pagLayerWasm_exch_1 = await pagComposition.getLayerAt(1);
  const pagLayer__exch_0 = new PAG.PAGLayer(pagLayerWasm_exch_0);
  const pagLayer__exch_1 = new PAG.PAGLayer(pagLayerWasm_exch_1);
  Log.log(`layerName0 : ${await pagLayer__exch_0.layerName()}, layerName1 : ${await pagLayer__exch_1.layerName()}`);
}

const createPAGView = async (file) => {
  if (pagFile) pagFile.destroy();
  if (pagView) pagView.destroy();
  pagFile = await PAG.PAGFile.load(file);
  const pagCanvas = document.getElementById('pag') as HTMLCanvasElement;
  pagCanvas.width = 640;
  pagCanvas.height = 640;
  pagView = await PAG.PAGView.init(pagFile, pagCanvas);
  pagView.setRepeatCount(0);
  // 绑定事件监听
  pagView.addListener('onAnimationStart', (event) => {
    console.log('onAnimationStart', event);
  });
  pagView.addListener('onAnimationEnd', (event) => {
    console.log('onAnimationEnd', event);
  });
  pagView.addListener('onAnimationCancel', (event) => {
    console.log('onAnimationCancel', event);
  });
  pagView.addListener('onAnimationRepeat', (event) => {
    console.log('onAnimationRepeat', event);
  });
  document.getElementById('control').style.display = '';
  // 图层编辑
  const editableLayers = await getEditableLayer(PAG, pagFile);
  pagComposition = await pagView.getComposition();
  console.log(editableLayers);
  renderEditableLayer(editableLayers);
  console.log(`已加载 ${file.name}`);

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

const getEditableLayer = async (PAG: PAGNamespace, pagFile) => {
  const editableImageCount = await pagFile.numImages();
  let res = [];
  for (let i = 0; i < editableImageCount; i++) {
    const vectorPagLayer = await pagFile.getLayersByEditableIndex(i, PAG.LayerType.Image);
    for (let j = 0; j < vectorPagLayer.size(); j++) {
      const pagLayerWasm = vectorPagLayer.get(j);
      const pagLayer = new PAG.PAGLayer(pagLayerWasm);
      const uniqueID = await pagLayer.uniqueID();
      const layerType = await pagLayer.layerType();
      const layerName = await pagLayer.layerName();
      const opacity = await pagLayer.opacity();
      const visible = await pagLayer.visible();
      const editableIndex = await pagLayer.editableIndex();
      const duration = await pagLayer.duration();
      const frameRate = await pagLayer.frameRate();
      const localStartTime = await pagLayer.startTime();
      const startTime = await pagLayer.localTimeToGlobal(localStartTime);

      res.push({ uniqueID, layerType, layerName, opacity, visible, editableIndex, frameRate, startTime, duration });
    }
  }
  return res;
};

const renderEditableLayer = (editableLayers) => {
  const box = document.createElement('div');
  box.className = 'mt-24';
  box.innerText = '图层编辑：';
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
  document.body.appendChild(box);
};

// 替换图片
const replaceImage = (element, index) => {
  const inputEl = document.createElement('input');
  inputEl.type = 'file';
  inputEl.style.display = 'none';
  element.appendChild(inputEl);
  inputEl.addEventListener('change', async (event) => {
    const pagImage = await PAG.PAGImage.fromFile((event.target as HTMLInputElement).files[0]);
    const pagFile = await pagView.getComposition();
    await pagFile.replaceImage(index, pagImage);
    await pagView.flush();
    await pagImage.destroy();
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
    const pagImage = await PAG.PAGImage.fromSource(videoEl);
    const pagFile = await pagView.getComposition();
    await pagFile.replaceImage(index, pagImage);
    await pagView.flush();
    await pagImage.destroy();
  });
  inputEl.click();
  element.removeChild(inputEl);
};
