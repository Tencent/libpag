import { VIDEO_DECODE_WAIT_FRAME } from '../constant';
import { addListener, removeListener, removeAllListeners } from '../utils/video-listener';
import { IS_WECHAT } from '../utils/ua';
import { Log } from '../utils/log';
import { EmscriptenGL } from '../types';

export interface TimeRange {
  start: number;
  end: number;
}

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

const playVideoElement = async (videoElement: HTMLVideoElement) => {
  if (IS_WECHAT && window.WeixinJSBridge) {
    await getWechatNetwork();
  }
  try {
    await videoElement.play();
  } catch (error: any) {
    Log.error(error.message);
    throw new Error(
      'Failed to decode video, please play PAG after user gesture. Or your can load a software decoder to decode the video.',
    );
  }
};

export class VideoReader {
  private videoEl: HTMLVideoElement | null;
  private readonly frameRate: number;
  private lastFlush: number;
  private hadPlay = false;
  private staticTimeRanges: StaticTimeRanges;

  public constructor(mp4Data: Uint8Array, frameRate: number, staticTimeRanges: TimeRange[]) {
    this.videoEl = document.createElement('video');
    this.videoEl.style.display = 'none';
    this.videoEl.muted = true;
    this.videoEl.playsInline = true;
    this.videoEl.load();
    addListener(this.videoEl, 'timeupdate', this.onTimeupdate.bind(this));
    this.frameRate = frameRate;
    this.lastFlush = -1;
    const blob = new Blob([mp4Data], { type: 'video/mp4' });
    this.videoEl.src = URL.createObjectURL(blob);
    this.staticTimeRanges = new StaticTimeRanges(staticTimeRanges);
  }

  public async prepare(targetFrame: number) {
    if (!this.videoEl) {
      console.error('Video element is null!');
      return false;
    }
    const { currentTime } = this.videoEl;
    const targetTime = targetFrame / this.frameRate;
    this.lastFlush = targetTime;
    if (currentTime === 0 && targetTime === 0) {
      if (this.hadPlay) {
        return true;
      } else {
        // Wait for initialization to complete
        await playVideoElement(this.videoEl);
        // Pause video at first frame.
        await new Promise<void>((resolve) => {
          window.requestAnimationFrame(() => {
            if (!this.videoEl) {
              console.error('Video Element is null!');
            } else {
              this.videoEl.pause();
              this.hadPlay = true;
            }
            resolve();
          });
        });
        return true;
      }
    } else {
      if (Math.round(targetTime * this.frameRate) === Math.round(currentTime * this.frameRate)) {
        // Current frame
        return true;
      } else if (this.staticTimeRanges.contains(targetFrame)) {
        // Static frame
        return await this.seek(targetTime, false);
      } else if (Math.abs(currentTime - targetTime) < (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME) {
        // Within tolerable frame rate deviation
        if (this.videoEl.paused) {
          await playVideoElement(this.videoEl);
        }
        return true;
      } else {
        // Seek and play
        return await this.seek(targetTime);
      }
    }
  }

  public renderToTexture(GL: EmscriptenGL, textureID: number) {
    if (!this.videoEl || this.videoEl.readyState < 2) return;
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.videoEl);
  }

  public onDestroy() {
    if (!this.videoEl) {
      throw new Error('Video element is null!');
    }

    removeAllListeners(this.videoEl, 'playing');
    removeAllListeners(this.videoEl, 'timeupdate');
    this.videoEl = null;
  }

  private onTimeupdate() {
    if (!this.videoEl || this.lastFlush < 0) return;
    const { currentTime } = this.videoEl;
    if (currentTime - this.lastFlush >= (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME && !this.videoEl.paused) {
      this.videoEl.pause();
    }
  }

  private seek(targetTime: number, play = true) {
    return new Promise<boolean>((resolve) => {
      let isCallback = false;
      let timer: any = null;
      const canplayCallback = async () => {
        if (!this.videoEl) {
          console.error('Video element is null!');
          resolve(false);
          return;
        }
        removeListener(this.videoEl, 'seeked', canplayCallback);
        if (play && this.videoEl.paused) {
          await playVideoElement(this.videoEl);
        } else if (!play && !this.videoEl.paused) {
          this.videoEl.pause();
        }
        isCallback = true;
        clearTimeout(timer);
        timer = null;
        resolve(true);
      };
      if (!this.videoEl) {
        console.error('Video element is null!');
        resolve(false);
        return;
      }
      addListener(this.videoEl, 'seeked', canplayCallback);
      this.videoEl!.currentTime = targetTime;
      // Timeout
      timer = setTimeout(() => {
        if (!isCallback) {
          if (!this.videoEl) {
            console.error('Video element is null!');
            resolve(false);
            return;
          } else {
            removeListener(this.videoEl, 'seeked', canplayCallback);
            resolve(false);
          }
        }
      }, (1000 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME);
    });
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
