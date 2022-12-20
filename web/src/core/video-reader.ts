import {
  VIDEO_DECODE_WAIT_FRAME,
  VIDEO_DECODE_SEEK_TIMEOUT_FRAME,
  VIDEO_PLAYBACK_RATE_MAX,
  VIDEO_PLAYBACK_RATE_MIN,
} from '../constant';
import { addListener, removeListener, removeAllListeners } from '../utils/video-listener';
import { IPHONE, WECHAT, APPLE } from '../utils/ua';
import { PAGModule } from '../pag-module';

import type { EmscriptenGL } from '../types';
import type { TimeRange } from '../interfaces';

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
  public static isIOS = () => {
    return IPHONE;
  };

  public static isAndroidMiniprogram = () => {
    return false;
  };

  public isSought = false;
  public isPlaying = false;

  private videoEl: HTMLVideoElement | null;
  private readonly frameRate: number;
  private canplay = false;
  private staticTimeRanges: StaticTimeRanges;
  private disablePlaybackRate = false;
  private error: any = null;

  public constructor(
    mp4Data: Uint8Array,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
  ) {
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
    const blob = new Blob([mp4Data], { type: 'video/mp4' });
    this.videoEl.src = URL.createObjectURL(blob);
    this.frameRate = frameRate;
    this.staticTimeRanges = new StaticTimeRanges(staticTimeRanges);
    if (UHD_RESOLUTION < width || UHD_RESOLUTION < height) {
      this.disablePlaybackRate = true;
    }
    PAGModule.currentPlayer?.bindingVideoReader(this);
  }

  public async prepare(targetFrame: number, playbackRate: number) {
    this.setError(null); // reset error
    this.isSought = false; // reset seek status
    if (!this.videoEl) {
      this.setError(new Error("Video element doesn't exist!"));
      return;
    }
    const { currentTime } = this.videoEl;
    const targetTime = targetFrame / this.frameRate;
    if (currentTime === 0 && targetTime === 0) {
      if (!this.canplay && !APPLE) {
        await waitVideoCanPlay(this.videoEl);
      } else {
        await this.play();
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
      } else if (this.staticTimeRanges.contains(targetFrame)) {
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
    if (!this.disablePlaybackRate && this.videoEl.playbackRate !== targetPlaybackRate) {
      this.videoEl.playbackRate = targetPlaybackRate;
    }

    if (this.isPlaying && this.videoEl.paused) {
      try {
        await this.play();
      } catch (e) {
        this.setError(e);
      }
    }
  }

  public renderToTexture(GL: EmscriptenGL, textureID: number) {
    if (!this.videoEl || this.videoEl.readyState < 2) return;
    if (this.getError() !== null) return;
    try {
      const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
      gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
      gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl.RGBA, gl.UNSIGNED_BYTE, this.videoEl);
    } catch (e) {
      this.setError(e);
    }
  }

  public async play() {
    if (!this.videoEl) {
      throw new Error("Video element doesn't exist!");
    }
    if (!this.videoEl.paused) return;
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
    if (!this.videoEl) {
      throw new Error("Video element doesn't exist!");
    }
    removeAllListeners(this.videoEl, 'playing');
    removeAllListeners(this.videoEl, 'timeupdate');
    this.videoEl = null;
  }

  private seek(targetTime: number, play = true) {
    return new Promise<void>((resolve) => {
      let isCallback = false;
      let timer: any = null;
      const setVideoState = async () => {
        if (play && this.videoEl!.paused) {
          try {
            await this.play();
          } catch (e) {
            this.setError(e);
          }
        } else if (!play && !this.videoEl!.paused) {
          this.videoEl?.pause();
        }
      };
      const seekCallback = async () => {
        if (!this.videoEl) {
          this.setError(new Error("Video element doesn't exist!"));
          resolve();
          return;
        }
        removeListener(this.videoEl, 'seeked', seekCallback);
        await setVideoState();
        isCallback = true;
        clearTimeout(timer);
        timer = null;
        resolve();
      };
      if (!this.videoEl) {
        this.setError(new Error("Video element doesn't exist!"));
        resolve();
        return;
      }
      addListener(this.videoEl, 'seeked', seekCallback);
      this.videoEl!.currentTime = targetTime;
      // Timeout
      timer = setTimeout(() => {
        if (!isCallback) {
          if (!this.videoEl) {
            this.setError(new Error("Video element doesn't exist!"));
            resolve();
            return;
          } else {
            removeListener(this.videoEl, 'seeked', seekCallback);
            setVideoState();
            resolve();
          }
        }
      }, (1000 / this.frameRate) * VIDEO_DECODE_SEEK_TIMEOUT_FRAME);
    });
  }

  private setError(e: any) {
    this.error = e;
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
