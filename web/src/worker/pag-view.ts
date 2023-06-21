import { WorkerMessageType } from './events';
import { postMessage } from './utils';
import { destroyVerify } from '../utils/decorators';
import { calculateDisplaySize } from '../utils/canvas';

import type { WorkerPAGFile } from './pag-file';
import type { PAGViewOptions } from '../pag-view';
import type { DebugData, PAGScaleMode } from '../types';

@destroyVerify
export class WorkerPAGView {
  /**
   * Create pag view.
   * @param file pag file.
   * @param canvas target render canvas.
   * @param initOptions pag view options
   * @returns
   */
  public static init(file: WorkerPAGFile, canvas: HTMLCanvasElement, initOptions?: PAGViewOptions) {
    const options = {
      ...{
        useScale: true,
        useCanvas2D: false,
        firstFrame: true,
      },
      ...initOptions,
    };
    if (options.useScale) {
      resizeCanvas(canvas);
    }
    const offscreen = canvas.transferControlToOffscreen();
    return postMessage(
      file.worker,
      { name: WorkerMessageType.PAGView_init, args: [file.key, offscreen, initOptions] },
      (key: number) => new WorkerPAGView(file.worker, key),
      [offscreen],
    );
  }

  public key: number;
  public worker: Worker;
  public isDestroyed = false;

  public constructor(worker: Worker, key: number) {
    this.worker = worker;
    this.key = key;
  }
  /**
   * The duration of current composition in microseconds.
   */
  public duration() {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGView_duration, args: [this.key] },
      (res: number) => res,
    );
  }
  /**
   * Start the animation.
   */
  public play() {
    return postMessage(this.worker, { name: WorkerMessageType.PAGView_play, args: [this.key] }, () => undefined);
  }
  /**
   * Pause the animation.
   */
  public pause(): Promise<void> {
    return postMessage(this.worker, { name: WorkerMessageType.PAGView_pause, args: [this.key] }, () => undefined);
  }
  /**
   * Stop the animation.
   */
  public stop(): Promise<void> {
    return postMessage(this.worker, { name: WorkerMessageType.PAGView_stop, args: [this.key] }, () => undefined);
  }
  /**
   * Set the number of times the animation will repeat. The default is 1, which means the animation
   * will play only once. 0 means the animation will play infinity times.
   */
  public setRepeatCount(repeatCount: number): Promise<void> {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGView_setRepeatCount, args: [this.key, repeatCount] },
      () => undefined,
    );
  }
  /**
   * Returns the current progress of play position, the value is from 0.0 to 1.0. It is applied only
   * when the composition is not null.
   */
  public getProgress() {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGView_getProgress, args: [this.key] },
      (res: number) => res,
    );
  }
  /**
   * Returns the current frame.
   */
  public currentFrame() {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGView_currentFrame, args: [this.key] },
      (res: number) => res,
    );
  }
  /**
   * Set the progress of play position, the value is from 0.0 to 1.0.
   */
  public setProgress(progress: number) {
    return postMessage(
      this.worker,
      {
        name: WorkerMessageType.PAGView_setProgress,
        args: [this.key, progress],
      },
      () => undefined,
    );
  }
  /**
   * Returns the current scale mode.
   */
  public scaleMode() {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGView_scaleMode, args: [this.key] },
      (res: number) => res,
    );
  }
  /**
   * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
   * changes when this method is called.
   */
  public setScaleMode(value: PAGScaleMode): Promise<void> {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGView_setScaleMode, args: [this.key, value] },
      () => undefined,
    );
  }
  /**
   * Call this method to render current position immediately. If the play() method is already
   * called, there is no need to call it. Returns true if the content has changed.
   */
  public flush() {
    return postMessage(this.worker, { name: WorkerMessageType.PAGView_flush, args: [this.key] }, (res: boolean) => res);
  }

  public getDebugData() {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGView_getDebugData, args: [this.key] },
      (res: DebugData) => res,
    );
  }

  public destroy() {
    postMessage(this.worker, { name: WorkerMessageType.PAGView_destroy, args: [this.key] }, () => {
      this.isDestroyed = true;
    });
  }
}

const resizeCanvas = (canvas: HTMLCanvasElement) => {
  const displaySize = calculateDisplaySize(canvas);
  canvas.style.width = `${displaySize.width}px`;
  canvas.style.height = `${displaySize.height}px`;
  canvas.width = displaySize.width * globalThis.devicePixelRatio;
  canvas.height = displaySize.height * globalThis.devicePixelRatio;
};
