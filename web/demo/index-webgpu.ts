import { PAGInit } from '../src/pag';

import type { PAGFile } from '../src/pag-file';
import type { PAGView } from '../src/pag-view';
import type { PAG as PAGNamespace } from '../src/types';

declare global {
  interface Window {
    pagView: any;
  }
}

if (typeof window !== 'undefined') {
  window.onload = async () => {
    const statusEl = document.getElementById('status') as HTMLElement;
    try {
      const PAG: PAGNamespace = await PAGInit({ locateFile: (file: string) => '../lib-webgpu/' + file });
      console.log(`wasm loaded! SDKVersion ${PAG.SDKVersion()}, isWebGPUBackend: ${PAG.isWebGPUBackend()}`);

      const buffer = await fetch('./assets/like.pag').then((res) => res.arrayBuffer());
      const pagFile = (await PAG.PAGFile.load(buffer)) as PAGFile;
      const canvas = document.getElementById('pag') as HTMLCanvasElement;
      const pagView = (await PAG.PAGView.init(pagFile, canvas)) as PAGView;
      pagView.setRepeatCount(0);
      await pagView.play();
      window.pagView = pagView;
      statusEl.innerText = 'WebGPU backend playing: like.pag';
    } catch (error) {
      statusEl.innerText = `PAGInit/WebGPU init failed: ${error}`;
      console.error(error);
    }
  };
}
