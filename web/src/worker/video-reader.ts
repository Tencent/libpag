import { WorkerMessageType } from './events';
import { postMessage } from './utils';

import type { EmscriptenGL } from '../types';
import { BitmapImage } from '../core/bitmap-image';

export class WorkerVideoReader {
  public bitmap: ImageBitmap | null = null;
  public isSought = false;
  public isPlaying = false;

  private proxyId: number;
  private bitmapImage: BitmapImage = new BitmapImage(null);

  public constructor(proxyId: number) {
    this.proxyId = proxyId;
  }

  public prepare(targetFrame: number, playbackRate: number) {
    return new Promise<void>((resolve) => {
      postMessage(
        self,
        { name: WorkerMessageType.VideoReader_prepare, args: [this.proxyId, targetFrame, playbackRate] },
        (res) => {
          this.bitmapImage.setBitmap(res);
          resolve();
        },
      );
    });
  }

  public getVideo() {
    return this.bitmapImage;
  }

  public renderToTexture(GL: EmscriptenGL, textureID: number) {
    if (!this.bitmap) return;
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.bitmap);
  }

  public onDestroy() {
    self.postMessage({ name: 'VideoReader.onDestroy', args: [this.proxyId] });
  }

  public play() {
    return new Promise<void>((resolve) => {
      postMessage(self, { name: WorkerMessageType.VideoReader_play, args: [this.proxyId] }, () => {
        resolve();
      });
    });
  }

  public pause() {
    postMessage(self, { name: WorkerMessageType.VideoReader_pause, args: [this.proxyId] }, () => {});
  }

  public stop() {
    postMessage(self, { name: WorkerMessageType.VideoReader_stop, args: [this.proxyId] }, () => {});
  }

  public getError() {
    return new Promise<any>((resolve) => {
      postMessage(self, { name: WorkerMessageType.VideoReader_getError, args: [this.proxyId] }, (res) => {
        resolve(res);
      });
    });
  }
}
