import { WorkerMessageType } from './events';
import * as types from '../types';

import type { ModuleOption, PAGInit } from '../pag';
import type { PAGFile } from '../pag-file';
import type { PAGView } from '../pag-view';
import type { PAGImage } from '../pag-image';

declare global {
  interface Window {
    PAG: types.PAG;
  }
}

export interface WorkerMessage {
  name: string;
  args: any[];
}
const pagFiles: (PAGFile & { [prop: string]: any })[] = [];
const registerPAGFile = (pagFile: PAGFile) => {
  pagFiles.push(pagFile);
  return pagFiles.length - 1;
};
const deletePAGFile = (key: number) => {
  (pagFiles[key] as any) = null;
};

const pagViews: (PAGView & { [prop: string]: any })[] = [];
const registerPAGView = (pagView: PAGView) => {
  pagViews.push(pagView);
  return pagViews.length - 1;
};
const deletePAGView = (key: number) => {
  (pagViews[key] as any) = null;
};

const pagImages: PAGImage[] = [];
const registerPAGImage = (pagImage: PAGImage) => {
  pagImages.push(pagImage);
  return pagImages.length - 1;
};
const deletePAGImage = (key: number) => {
  (pagImages[key] as any) = null;
};

const textDataMap: (types.TextDocument & { [prop: string]: any })[] = [];
const registerTextData = (textData: types.TextDocument) => {
  textDataMap.push(textData);
  return textDataMap.length - 1;
};
const deleteTextData = (key: number) => {
  (textDataMap[key] as any) = null;
};

const workerInit = (init: typeof PAGInit) => {
  onmessage = async (event: MessageEvent<WorkerMessage>) => {
    const messageHandles: { [prop: string]: (event: MessageEvent<WorkerMessage>) => void } = {
      [WorkerMessageType.PAGInit]: (event) => {
        const option: ModuleOption = {};
        if (event.data.args[0]) {
          option.locateFile = () => event.data.args[0].fileUrl;
        }
        init(option).then((module: types.PAG) => {
          self.PAG = module;
          self.postMessage({ name: event.data.name, args: [] });
        });
      },
      [WorkerMessageType.PAGFile_load]: async (event) => {
        const key = registerPAGFile(await self.PAG.PAGFile.load(event.data.args[0]));
        self.postMessage({ name: event.data.name, args: [key] });
      },
      [WorkerMessageType.PAGView_init]: async (event) => {
        const [pagFileKey, canvas, option] = event.data.args;
        const key = registerPAGView((await self.PAG.PAGView.init(pagFiles[pagFileKey], canvas, option)) as PAGView);
        self.postMessage({ name: event.data.name, args: [key] });
      },
      [WorkerMessageType.PAGView_destroy]: async (event) => {
        const [pagViewKey] = event.data.args;
        pagViews[pagViewKey].destroy();
        deletePAGView(pagViewKey);
        self.postMessage({ name: event.data.name, args: [] });
      },
      [WorkerMessageType.PAGFile_getTextData]: async (event) => {
        const [pagFileKey, editableTextIndex] = event.data.args;
        const textData = pagFiles[pagFileKey].getTextData(editableTextIndex) as types.TextDocument & {
          [prop: string]: any;
        };
        let virtualTextData: { [key: string]: any } = {};
        for (const key in textData) {
          if (typeof textData[key] !== 'function') {
            virtualTextData[key] = textData[key];
          }
        }
        virtualTextData.key = registerTextData(textData);
        self.postMessage({ name: event.data.name, args: [virtualTextData] });
      },
      [WorkerMessageType.PAGFile_replaceText]: async (event) => {
        const [pagFileKey, editableTextIndex, virtualTextData] = event.data.args;
        const textData = textDataMap[virtualTextData.key];
        for (const key in virtualTextData) {
          if (key !== 'key') {
            textData[key] = virtualTextData[key];
          }
        }
        pagFiles[pagFileKey].replaceText(editableTextIndex, textData);
        self.postMessage({ name: event.data.name, args: [] });
      },
      [WorkerMessageType.PAGFile_replaceImage]: async (event) => {
        const [pagFileKey, editableImageIndex, pagImageKey] = event.data.args;
        const pagImage = pagImages[pagImageKey];
        pagFiles[pagFileKey].replaceImage(editableImageIndex, pagImage);
        self.postMessage({ name: event.data.name, args: [] });
      },
      [WorkerMessageType.PAGFile_destroy]: async (event) => {
        const [pagFileKey] = event.data.args;
        pagFiles[pagFileKey].destroy();
        deletePAGFile(pagFileKey);
        self.postMessage({ name: event.data.name, args: [] });
      },
      [WorkerMessageType.PAGImage_fromSource]: async (event) => {
        const [bitmap] = event.data.args;
        const key = registerPAGImage(self.PAG.PAGImage.fromSource(bitmap));
        self.postMessage({ name: event.data.name, args: [key] });
      },
      [WorkerMessageType.PAGImage_destroy]: async (event) => {
        const [pagImageKey] = event.data.args;
        pagImages[pagImageKey].destroy();
        deletePAGImage(pagImageKey);
        self.postMessage({ name: event.data.name, args: [] });
      },
      [WorkerMessageType.TextDocument_delete]: async (event) => {
        const [textDataKey] = event.data.args;
        textDataMap[textDataKey].delete();
        deleteTextData(textDataKey);
        self.postMessage({ name: event.data.name, args: [] });
      },
    };

    const name = event.data.name.split('_')[0];

    if (messageHandles[name]) {
      messageHandles[name](event);
      return;
    }

    const [type, fnName] = name.split('.') as [string, string];
    const key = event.data.args[0];
    if (type === 'PAGFile') {
      const pagFile = pagFiles[key];
      if (!pagFile) throw new Error("pagFile doesn't exist");
      const fn = pagFile[fnName] as Function;
      if (!fn) throw new Error(`PAGFile.${fnName} doesn't exist`);
      const res = await fn.call(pagFile, ...event.data.args.slice(1));
      self.postMessage({ name: event.data.name, args: [res] });
      return;
    }
    if (type === 'PAGView') {
      const pagView = pagViews[key];
      if (!pagView) throw new Error("pagView doesn't exist");
      const fn = pagView[fnName] as Function;
      if (!fn) throw new Error(`PAGView.${fnName} doesn't exist`);
      const res = await fn.call(pagView, ...event.data.args.slice(1));
      self.postMessage({ name: event.data.name, args: [res] });
      return;
    }
  };
};

export { workerInit };
