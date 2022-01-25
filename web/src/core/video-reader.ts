import { VIDEO_DECODE_WAIT_FRAME } from '../constant';
import { convertMp4 } from '../h264/h264';
import { addListener, removeListener, removeAllListeners } from '../utils/video-listener';

export class VideoReader {
  private videoEl: HTMLVideoElement = null;
  private readonly frameRate: number;
  private lastFlush: number;
  private hadPlay = false;

  public constructor(width, height, frameRate, h264Headers, h264Frames, ptsList) {
    this.videoEl = document.createElement('video');
    this.videoEl.style.display = 'none';
    this.videoEl.muted = true;
    this.videoEl.playsInline = true;
    addListener(this.videoEl, 'timeupdate', this.onTimeupdate.bind(this));
    this.frameRate = frameRate;
    this.lastFlush = -1;
    const mp4 = convertMp4(h264Frames, h264Headers, width, height, this.frameRate, ptsList);
    const blob = new Blob([mp4], { type: 'video/mp4' });
    this.videoEl.src = URL.createObjectURL(blob);
  }

  public prepareAsync(targetFrame: number) {
    return new Promise((resolve) => {
      const { currentTime } = this.videoEl;
      const targetTime = targetFrame / this.frameRate;
      this.lastFlush = targetTime;
      if (currentTime === 0 && targetTime === 0) {
        if (this.hadPlay) {
          // Video 初始化过
          resolve(true);
        } else {
          // Video 首帧，等待初始化完成
          const canplayCallback = () => {
            window.requestAnimationFrame(() => {
              this.videoEl.pause();
              this.hadPlay = true;
              resolve(true);
            });
            removeListener(this.videoEl, 'playing', canplayCallback);
          };
          addListener(this.videoEl, 'playing', canplayCallback);
          this.videoEl.play();
        }
      } else {
        if (targetTime === currentTime) {
          // 当前画面
          resolve(true);
        } else if (Math.abs(currentTime - targetTime) < (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME) {
          // 在可容忍的帧数偏差内
          if (this.videoEl.paused) {
            this.videoEl.play();
          }
          resolve(true);
        } else {
          // 对其 Video 时间轴
          let isCallback = false;
          const timeupdateCallback = () => {
            removeListener(this.videoEl, 'timeupdate', timeupdateCallback);
            this.videoEl.play();
            isCallback = true;
            resolve(true);
          };
          addListener(this.videoEl, 'timeupdate', timeupdateCallback);
          this.videoEl.currentTime = targetTime;
          // 超时未返回
          setTimeout(() => {
            if (!isCallback) {
              removeListener(this.videoEl, 'timeupdate', timeupdateCallback);
              resolve(true);
            }
          }, (1000 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME);
        }
      }
    });
  }

  public renderToTexture(GL, textureID) {
    if (this.videoEl.readyState < 2) return;
    const gl = GL.currentContext.GLctx as WebGLRenderingContext;
    gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.videoEl);
  }

  public onDestroy() {
    removeAllListeners(this.videoEl, 'playing');
    removeAllListeners(this.videoEl, 'timeupdate');
    this.videoEl = null;
  }

  private onTimeupdate() {
    if (this.lastFlush < 0) return;
    const { currentTime } = this.videoEl;
    if (currentTime - this.lastFlush >= (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME && !this.videoEl.paused) {
      this.videoEl.pause();
    }
  }
}
