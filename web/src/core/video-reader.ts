import {
  VIDEO_DECODE_WAIT_FRAME,
  VIDEO_DECODE_SEEK_TIMEOUT_FRAME,
  VIDEO_PLAYBACK_RATE_MAX,
  VIDEO_PLAYBACK_RATE_MIN,
} from '../constant';
import { addListener, removeListener, removeAllListeners } from '../utils/video-listener';
import { IPHONE, WECHAT, SAFARI_OR_IOS_WEBVIEW, WORKER } from '../utils/ua';
import { PAGModule } from '../pag-module';
import { WorkerMessageType } from '../worker/events';
import { WorkerVideoReader } from '../worker/video-reader';
import { postMessage } from '../worker/utils';

import type { TimeRange, VideoReader as VideoReaderInterfaces } from '../interfaces';
import type { PAGPlayer } from '../pag-player';
import { isInstanceOf } from '../utils/type-utils';

const UHD_RESOLUTION = 3840;

// Get video initiated token on Wechat browser.
const getWechatNetwork = () => {
  return new Promise<void>((resolve) => {
    window.WeixinJSBridge.invoke(
      'getNetworkType',
      {},
      () => {
        resolve();
      },
      () => {
        resolve();
      },
    );
  });
};

const waitVideoCanPlay = (videoElement: HTMLVideoElement) => {
  return new Promise((resolve) => {
    const canplayHandle = () => {
      removeListener(videoElement, 'canplay', canplayHandle);
      clearTimeout(timer);
      resolve(true);
    };
    addListener(videoElement, 'canplay', canplayHandle);
    const timer = setTimeout(() => {
      removeListener(videoElement, 'canplay', canplayHandle);
      resolve(false);
    }, 1000);
  });
};

export class VideoReader {
  public static async create(
    source: Uint8Array | HTMLVideoElement,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
  ): Promise<VideoReaderInterfaces> {
    if (WORKER) {
      const proxyId = await new Promise<number>((resolve) => {
        // TODO: source as HTMLVideoElement in WebWorker version.
        const uint8Array = source as Uint8Array;
        const buffer = uint8Array.buffer.slice(uint8Array.byteOffset, uint8Array.byteOffset + uint8Array.byteLength);
        postMessage(
          self,
          {
            name: WorkerMessageType.VideoReader_constructor,
            args: [buffer, width, height, frameRate, staticTimeRanges, true],
          },
          (res) => {
            resolve(res);
          },
          [buffer],
        );
      });
      const videoReader = new WorkerVideoReader(proxyId);
      PAGModule.currentPlayer?.linkVideoReader(videoReader);
      return videoReader;
    }
    return new VideoReader(source, width, height, frameRate, staticTimeRanges);
  }

  public isSought = false;
  public isPlaying = false;
  public bitmap: ImageBitmap | null = null;

  private videoEl: HTMLVideoElement | null = null;
  private frameRate = 0;
  private canplay = false;
  private staticTimeRanges: StaticTimeRanges | null = null;
  private disablePlaybackRate = false;
  private error: any = null;
  private player: PAGPlayer | null = null;
  private width = 0;
  private height = 0;
  private bitmapCanvas: OffscreenCanvas | null = null;
  private bitmapCtx: OffscreenCanvasRenderingContext2D | null = null;

  public constructor(
    source: Uint8Array | HTMLVideoElement,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
    isWorker = false,
  ) {
    if (isInstanceOf(source, globalThis.HTMLVideoElement)) {
      this.videoEl = source as HTMLVideoElement;
      this.canplay = true;
    } else {
      this.videoEl = document.createElement('video');
      this.videoEl.style.display = 'none';
      this.videoEl.muted = true;
      this.videoEl.playsInline = true;
      this.videoEl.preload = 'auto'; // use load() will make a bug on Chrome.
      this.videoEl.width = width;
      this.videoEl.height = height;
      waitVideoCanPlay(this.videoEl).then(() => {
        this.canplay = true;
      });
      const blob = new Blob([source as Uint8Array], { type: 'video/mp4' });
      this.videoEl.src = URL.createObjectURL(blob);
      if (IPHONE) {
        // use load() will make a bug on Chrome.
        this.videoEl.load();
      }
    }
    this.frameRate = frameRate;
    this.width = width;
    this.height = height;
    this.staticTimeRanges = new StaticTimeRanges(staticTimeRanges);
    if (UHD_RESOLUTION < width || UHD_RESOLUTION < height) {
      this.disablePlaybackRate = true;
    }
    if (!isWorker) {
      this.linkPlayer(PAGModule.currentPlayer);
    }
  }

  public async prepare(targetFrame: number, playbackRate: number) {
    this.setError(null); // reset error
    this.isSought = false; // reset seek status
    const { currentTime } = this.videoEl!;
    const targetTime = targetFrame / this.frameRate;
    if (currentTime === 0 && targetTime === 0) {
      if (!this.canplay && !SAFARI_OR_IOS_WEBVIEW) {
        await waitVideoCanPlay(this.videoEl!);
      } else {
        try {
          await this.play();
        } catch (e) {
          this.setError(e);
        }
        await new Promise<void>((resolve) => {
          requestAnimationFrame(() => {
            this.pause();
            resolve();
          });
        });
      }
    } else {
      if (Math.round(targetTime * this.frameRate) === Math.round(currentTime * this.frameRate)) {
        // Current frame
      } else if (this.staticTimeRanges?.contains(targetFrame)) {
        // Static frame
        await this.seek(targetTime, false);
        return;
      } else if (Math.abs(currentTime - targetTime) < (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME) {
        // Within tolerable frame rate deviation
      } else {
        // Seek and play
        this.isSought = true;
        await this.seek(targetTime);
        return;
      }
    }

    const targetPlaybackRate = Math.min(Math.max(playbackRate, VIDEO_PLAYBACK_RATE_MIN), VIDEO_PLAYBACK_RATE_MAX);
    if (!this.disablePlaybackRate && this.videoEl!.playbackRate !== targetPlaybackRate) {
      this.videoEl!.playbackRate = targetPlaybackRate;
    }

    if (this.isPlaying && this.videoEl!.paused) {
      try {
        await this.play();
      } catch (e) {
        this.setError(e);
      }
    }
  }

  public getVideo() {
    return this.videoEl;
  }

  // Only work in web worker version
  public async generateBitmap() {
    // Batter than createImageBitmap from video element in benchmark
    if (!this.bitmapCanvas) {
      this.bitmapCanvas = new OffscreenCanvas(this.width, this.height);
      this.bitmapCanvas!.width = this.width;
      this.bitmapCanvas!.height = this.height;
      this.bitmapCtx = this.bitmapCanvas.getContext('2d') as OffscreenCanvasRenderingContext2D | null;
    }
    this.bitmapCtx?.fillRect(0, 0, this.width, this.height);
    this.bitmapCtx?.drawImage(this.videoEl as HTMLVideoElement, 0, 0, this.width, this.height);
    this.bitmap = await createImageBitmap(this.bitmapCanvas);
    return this.bitmap;
  }

  public async play() {
    if (!this.videoEl!.paused) return;
    if (WECHAT && window.WeixinJSBridge) {
      await getWechatNetwork();
    }
    if (document.visibilityState !== 'visible') {
      const visibilityHandle = () => {
        if (document.visibilityState === 'visible') {
          if (this.videoEl) this.videoEl.play();
          window.removeEventListener('visibilitychange', visibilityHandle);
        }
      };
      window.addEventListener('visibilitychange', visibilityHandle);
      throw new Error('The play() request was interrupted because the document was hidden!');
    }
    await this.videoEl?.play();
  }

  public pause() {
    this.isPlaying = false;
    if (this.videoEl!.paused) return;
    this.videoEl?.pause();
  }

  public stop() {
    this.isPlaying = false;
    if (!this.videoEl!.paused) {
      this.videoEl?.pause();
    }
    this.videoEl!.currentTime = 0;
  }

  public getError() {
    return this.error;
  }

  public onDestroy() {
    if (this.player) {
      this.player.unlinkVideoReader(this);
    }
    removeAllListeners(this.videoEl!, 'playing');
    removeAllListeners(this.videoEl!, 'timeupdate');
    this.videoEl = null;
    this.bitmapCanvas = null;
    this.bitmapCtx = null;
  }

  private seek(targetTime: number, play = true) {
    return new Promise<void>((resolve, reject) => {
      if (!this.videoEl) {
        reject(new Error('Video element is not initialized.'));
        return;
      }

      const onSeeked = () => {
        removeListener(this.videoEl!, 'seeked', onSeeked);
        clearTimeout(seekTimeout);
        if (play) {
          this.videoEl?.play().catch((e) => {
            this.setError(e);
          });
        } else if (!play && !this.videoEl!.paused)  {
          this.videoEl?.pause();
        }
        resolve();
      };
  
      const onCanPlay = () => {
        removeListener(this.videoEl!, 'canplay', onCanPlay);
        // Now that we have enough data, perform the seek.
        this.videoEl!.currentTime = targetTime;
        addListener(this.videoEl!, 'seeked', onSeeked);
      };
  
      const seekTimeout = setTimeout(() => {
        removeListener(this.videoEl!, 'canplay', onCanPlay);
        removeListener(this.videoEl!, 'seeked', onSeeked);
        reject(new Error('Seek operation timed out.'));
      }, (1000 / this.frameRate) * VIDEO_DECODE_SEEK_TIMEOUT_FRAME);
  
      // Check if we need to wait for 'canplay' event before seeking.
      if (this.videoEl.readyState < HTMLMediaElement.HAVE_FUTURE_DATA) {
        addListener(this.videoEl, 'canplay', onCanPlay);
      } else {
        // We already have enough data to perform the seek.
        this.videoEl!.currentTime = targetTime;
        addListener(this.videoEl, 'seeked', onSeeked);
      }
    });
  }

  private setError(e: any) {
    this.error = e;
  }

  private linkPlayer(player: PAGPlayer | null) {
    this.player = player;
    if (player) {
      player.linkVideoReader(this);
    }
  }
}

export class StaticTimeRanges {
  private timeRanges: TimeRange[];

  public constructor(timeRanges: TimeRange[]) {
    this.timeRanges = timeRanges;
  }

  public contains(targetFrame: number) {
    if (this.timeRanges.length === 0) return false;
    for (let timeRange of this.timeRanges) {
      if (timeRange.start <= targetFrame && targetFrame < timeRange.end) {
        return true;
      }
    }
    return false;
  }
}
