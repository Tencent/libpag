import { VideoSequence } from '../base/video-sequence';
import { destroyVerify } from '../decorators';
import { coverToMp4 } from '../generator/mp4-box-helper';
import { getWechatNetwork } from './utils';
import { addListener, removeAllListeners, removeListener } from './video-listener';

declare global {
  interface Window {
    WeixinJSBridge?: any;
  }
}

type K = keyof HTMLVideoElementEventMap;

const IS_WECHAT = navigator && /MicroMessenger/i.test(navigator.userAgent);

const playVideoElement = async (videoElement: HTMLVideoElement) => {
  if (IS_WECHAT && window.WeixinJSBridge) {
    await getWechatNetwork();
  }
  try {
    await videoElement.play();
  } catch (error: any) {
    throw new Error(error.message);
  }
};

@destroyVerify
export class VideoReader {
  protected destroyed = false;

  private videoElement: HTMLVideoElement | undefined;

  public constructor(videoSequence: VideoSequence) {
    this.load(videoSequence);
  }

  public getVideoElement(): HTMLVideoElement {
    return this.videoElement as HTMLVideoElement;
  }

  public progress() {
    return Math.round((this.videoElement!.currentTime / this.videoElement!.duration) * 100) / 100;
  }

  public duration() {
    return this.videoElement!.duration;
  }

  public currentTime() {
    return this.videoElement!.currentTime;
  }

  public start() {
    return playVideoElement(this.videoElement as HTMLVideoElement);
  }

  public pause() {
    this.videoElement?.pause();
  }

  public seek(time: number) {
    return new Promise<void>((resolve) => {
      const seekCallback = () => {
        removeListener(this.videoElement as HTMLVideoElement, 'seeked', seekCallback);
        resolve();
      };
      addListener(this.videoElement as HTMLVideoElement, 'seeked', seekCallback);
      this.videoElement!.currentTime = time;
    });
  }

  public addListener(event: K, handler: (this: HTMLVideoElement, ev: HTMLVideoElementEventMap[K]) => void) {
    addListener(this.videoElement as HTMLVideoElement, event, handler);
  }

  public removeAllListeners() {
    removeAllListeners(this.videoElement as HTMLVideoElement);
  }

  public getFrameData(): any {
    // NOP
  }

  public destroy() {
    this.removeAllListeners();
    this.videoElement = undefined;
    this.destroyed = true;
  }

  protected load(videoSequence: VideoSequence) {
    this.videoElement = document.createElement('video');
    this.videoElement.style.display = 'none';
    this.videoElement.muted = true;
    this.videoElement.playsInline = true;
    this.videoElement.src = URL.createObjectURL(new Blob([coverToMp4(videoSequence)], { type: 'video/mp4' }));
    this.videoElement.load();
  }
}
