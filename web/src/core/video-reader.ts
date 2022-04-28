import { VIDEO_DECODE_WAIT_FRAME } from '../constant';
import { addListener, removeListener, removeAllListeners } from '../utils/video-listener';
import { IS_WECHAT } from '../utils/ua';
import { Log } from '../utils/log';
import { ErrorCode } from '../utils/error-map';
import { EmscriptenGL } from '../types';

export interface TimeRange {
  start: number;
  end: number;
}

const playVideoElement = (videoElement: HTMLVideoElement) => {
  return new Promise((resolve) => {
    const play = () => {
      videoElement
        .play()
        .then(() => {
          resolve(true);
        })
        .catch((event: DOMException) => {
          Log.error(event.message);
          resolve(false);
        });
    };

    if (IS_WECHAT && window.WeixinJSBridge) {
      window.WeixinJSBridge.invoke(
        'getNetworkType',
        {},
        () => {
          play();
        },
        () => {
          play();
        },
      );
    } else {
      play();
    }
  });
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
    addListener(this.videoEl, 'timeupdate', this.onTimeupdate.bind(this));
    this.frameRate = frameRate;
    this.lastFlush = -1;
    const blob = new Blob([mp4Data], { type: 'video/mp4' });
    this.videoEl.src = URL.createObjectURL(blob);
    this.staticTimeRanges = new StaticTimeRanges(staticTimeRanges);
  }

  public prepare(targetFrame: number) {
    return new Promise((resolve) => {
      if (this.videoEl === null) {
        Log.errorByCode(ErrorCode.VideoElementNull);
      } else {
        const { currentTime } = this.videoEl;
        const targetTime = targetFrame / this.frameRate;
        this.lastFlush = targetTime;
        if (currentTime === 0 && targetTime === 0) {
          if (this.hadPlay) {
            resolve(true);
          } else {
            // Wait for initialization to complete
            playVideoElement(this.videoEl).then(() => {
              window.requestAnimationFrame(() => {
                if (this.videoEl === null) {
                  Log.errorByCode(ErrorCode.VideoElementNull);
                } else {
                  this.videoEl.pause();
                  this.hadPlay = true;
                  resolve(true);
                }
              });
            });
          }
        } else {
          if (Math.round(targetTime * this.frameRate) === Math.round(currentTime * this.frameRate)) {
            // Current frame
            resolve(true);
          } else if (this.staticTimeRanges.contains(targetFrame)) {
            // Static frame
            this.seek(resolve, targetTime, false);
          } else if (Math.abs(currentTime - targetTime) < (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME) {
            // Within tolerable frame rate deviation
            if (this.videoEl.paused) {
              playVideoElement(this.videoEl);
            }
            resolve(true);
          } else {
            // Seek and play
            this.seek(resolve, targetTime);
          }
        }
      }
    });
  }

  public renderToTexture(GL: EmscriptenGL, textureID: number) {
    if (!this.videoEl || this.videoEl.readyState < 2) return;
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.videoEl);
  }

  public onDestroy() {
    if (this.videoEl === null) {
      Log.errorByCode(ErrorCode.VideoElementNull);
    } else {
      removeAllListeners(this.videoEl, 'playing');
      removeAllListeners(this.videoEl, 'timeupdate');
      this.videoEl = null;
    }
  }

  private onTimeupdate() {
    if (!this.videoEl || this.lastFlush < 0) return;
    const { currentTime } = this.videoEl;
    if (currentTime - this.lastFlush >= (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME && !this.videoEl.paused) {
      this.videoEl.pause();
    }
  }

  private seek(resolve: (value: unknown) => void, targetTime: number, play = true) {
    let isCallback = false;
    const canplayCallback = () => {
      if (this.videoEl === null) {
        Log.errorByCode(ErrorCode.VideoElementNull);
      } else {
        removeListener(this.videoEl, 'canplay', canplayCallback);
        if (play && this.videoEl.paused) {
          playVideoElement(this.videoEl);
        } else if (!play && !this.videoEl.paused) {
          this.videoEl.pause();
        }
        isCallback = true;
        resolve(true);
      }
    };
    if (this.videoEl === null) {
      resolve(false);
      Log.errorByCode(ErrorCode.VideoElementNull);
      return;
    }
    addListener(this.videoEl, 'canplay', canplayCallback);
    this.videoEl!.currentTime = targetTime;
    // Timeout
    setTimeout(() => {
      if (!isCallback) {
        if (this.videoEl === null) {
          resolve(false);
          Log.errorByCode(ErrorCode.VideoElementNull);
        } else {
          removeListener(this.videoEl, 'canplay', canplayCallback);
          resolve(true);
        }
      }
    }, (1000 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME);
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
