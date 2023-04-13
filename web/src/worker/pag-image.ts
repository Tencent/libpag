import { destroyVerify } from '../utils/decorators';
import { isInstanceOf } from '../utils/type-utils';
import { WorkerMessageType } from './events';
import { postMessage } from './utils';

@destroyVerify
export class WorkerPAGImage {
  public static async fromSource(worker: Worker, source: TexImageSource) {
    const width = isInstanceOf(source, globalThis.HTMLVideoElement)
      ? (source as HTMLVideoElement).videoWidth
      : source.width;
    const height = isInstanceOf(source, globalThis.HTMLVideoElement)
      ? (source as HTMLVideoElement).videoHeight
      : source.height;
    const canvas = new OffscreenCanvas(width, height);
    canvas.width = source.width;
    canvas.height = source.height;
    const ctx = canvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    ctx.drawImage(source as CanvasImageSource, 0, 0, source.width, source.height);
    const bitmap = await createImageBitmap(canvas);
    return postMessage(
      worker,
      { name: WorkerMessageType.PAGImage_fromSource, args: [bitmap] },
      (key: number) => new WorkerPAGImage(worker, key),
      [bitmap],
    );
  }

  public worker: Worker;
  public key: number;
  public isDestroyed = false;

  public constructor(worker: Worker, key: number) {
    this.worker = worker;
    this.key = key;
  }

  public destroy(): Promise<void> {
    return postMessage(this.worker, { name: WorkerMessageType.PAGImage_destroy, args: [this.key] }, () => {
      this.isDestroyed = true;
    });
  }
}
