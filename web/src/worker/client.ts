import { WorkerMessageType } from './events';
import { postMessage } from './utils';
import { VideoReader } from '../core/video-reader';
import { WorkerPAGFile } from './pag-file';
import { WorkerPAGView } from './pag-view';
import { WorkerPAGImage } from './pag-image';

import type { WorkerMessage } from './worker';
import type { ModuleOption } from '../pag';
import type { TimeRange } from '../interfaces';

export const MAX_ACTIVE_WORKER_CONTEXTS = 4;

const videoReaders: VideoReader[] = [];

export interface PAGWorkerOptions {
  /**
   * Link to wasm file and libpag core script.
   */
  locateFile?: (file: 'libpag.wasm' | 'libpag.js') => string;
  /**
   * Configure for worker.
   */
  workerOptions?: WorkerOptions;
}

export const createPAGWorker = (pagWorkerOptions: PAGWorkerOptions = {}) => {
  let scriptUrl = pagWorkerOptions.locateFile ? pagWorkerOptions.locateFile('libpag.js') : 'libpag.js';
  const option: { fileUrl?: string } & ModuleOption = {};
  if (pagWorkerOptions.locateFile) {
    option.fileUrl = pagWorkerOptions.locateFile('libpag.wasm');
  }
  const worker = new Worker(scriptUrl, pagWorkerOptions.workerOptions);
  return postMessage(worker, { name: WorkerMessageType.PAGInit, args: [option] }, () => {
    addGlobalWorkerListener(worker);
    return worker;
  });
};

const addGlobalWorkerListener = (worker: Worker) => {
  const handle = (event: MessageEvent<WorkerMessage>) => {
    if (event.data.name.includes(WorkerMessageType.VideoReader_constructor)) {
      const videoReader = new VideoReader(
        ...(event.data.args as [Uint8Array, number, number, number, TimeRange[], boolean]),
      );
      videoReaders.push(videoReader);
      worker.postMessage({ name: event.data.name, args: [videoReaders.length - 1] });
      return;
    }
    if (event.data.name.includes(WorkerMessageType.VideoReader_prepare)) {
      const [proxyId, targetFrame, playbackRate] = event.data.args as [number, number, number];
      videoReaders[proxyId].prepare(targetFrame, playbackRate).then(() => {
        videoReaders[proxyId].generateBitmap().then((bitmap) => {
          worker.postMessage({ name: event.data.name, args: [bitmap] }, [bitmap]);
        });
      });
    }
    if (event.data.name.includes(WorkerMessageType.VideoReader_play)) {
      videoReaders[event.data.args[0]].play().then((res) => {
        worker.postMessage({ name: event.data.name, args: [res] });
      });
    }
    if (event.data.name.includes(WorkerMessageType.VideoReader_pause)) {
      videoReaders[event.data.args[0]].pause();
    }
    if (event.data.name.includes(WorkerMessageType.VideoReader_stop)) {
      videoReaders[event.data.args[0]].stop();
    }
    if (event.data.name.includes(WorkerMessageType.VideoReader_getError)) {
      worker.postMessage({ name: event.data.name, args: [videoReaders[event.data.args[0]].getError()] });
    }
  };
  worker.addEventListener('message', handle);
};

export { WorkerPAGFile, WorkerPAGView, WorkerPAGImage };
