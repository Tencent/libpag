import { Clock } from '../base/utils/clock';
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
  public static create(videoSequence: VideoSequence) {
    const videoReader = new VideoReader(videoSequence);
    const debugData = videoReader.load(videoSequence);
    return { videoReader: videoReader, debugData: debugData };
  }

  protected destroyed = false;
  protected frameRate = 0;

  private _duration: number;
  private videoElement: HTMLVideoElement | undefined;

  public constructor(videoSequence: VideoSequence) {
    this._duration = videoSequence.frameCount / videoSequence.frameRate;
    this.frameRate = videoSequence.frameRate;
  }

  public getVideoElement(): HTMLVideoElement {
    return this.videoElement as HTMLVideoElement;
  }

  public progress() {
    return Math.round((this.videoElement!.currentTime / this._duration) * 100) / 100;
  }

  public duration() {
    return this._duration;
  }

  public currentTime() {
    return this.videoElement!.currentTime || 0;
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

  public getFrameData(callback: any): any {
    // NOP
  }

  public clearCallback() {
    // NOP
  }

  public destroy() {
    this.removeAllListeners();
    this.videoElement = undefined;
    this.destroyed = true;
  }

  protected load(videoSequence: VideoSequence): any {
    this.videoElement = document.createElement('video');
    this.videoElement.style.display = 'none';
    this.videoElement.muted = true;
    this.videoElement.playsInline = true;
    const clock = new Clock();
    const mp4Data = coverToMp4(videoSequence);
    clock.mark('coverMP4');
    this.videoElement.src = URL.createObjectURL(new Blob([mp4Data], { type: 'video/mp4' }));
    this.videoElement.load();
    return {
      coverMP4: clock.measure('', 'coverMP4'),
    };
  }
}
