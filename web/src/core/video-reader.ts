import {
  VIDEO_DECODE_WAIT_FRAME,
  VIDEO_DECODE_SEEK_TIMEOUT_FRAME,
  VIDEO_PLAYBACK_RATE_MAX,
  VIDEO_PLAYBACK_RATE_MIN,
} from '../constant';
import { addListener, removeListener } from '../utils/video-listener';
import { IPHONE, WECHAT, SAFARI_OR_IOS_WEBVIEW } from '../utils/ua';
import type { TimeRange, VideoReader as VideoReaderInterfaces, VideoReaderOwner } from '../interfaces';
import { isInstanceOf } from '../utils/type-utils';

const UHD_RESOLUTION = 3840;
const VIDEO_WAIT_TIMEOUT = 1000;

type RegisterVideoWait = (complete: () => void, fail: (error: any) => void) => () => void;

class VideoReaderOperation {
  public cancelled = false;
  private cancelWait: (() => void) | null = null;

  public cancel() {
    this.cancelled = true;
    this.cancelWait?.();
  }

  public wait(register: RegisterVideoWait, timeout: number, message: string): Promise<boolean> {
    if (this.cancelled) return Promise.resolve(false);
    return new Promise<boolean>(this.startWait.bind(this, register, timeout, message));
  }

  private clearCancellation() {
    this.cancelWait = null;
  }

  private startWait(
    register: RegisterVideoWait,
    timeout: number,
    message: string,
    resolve: (value: boolean) => void,
    reject: (error: any) => void,
  ) {
    const clearCancellation = this.clearCancellation.bind(this);
    let settled = false;
    let cleanup: (() => void) | null = null;
    const timer = setTimeout(onTimeout, timeout);

    function settle(result: boolean, error?: any) {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      clearCancellation();
      cleanup?.();
      if (error !== undefined) {
        reject(error);
      } else {
        resolve(result);
      }
    }

    function complete() {
      settle(true);
    }

    function fail(error: any) {
      settle(false, error ?? new Error(message));
    }

    function onTimeout() {
      fail(new Error(message));
    }

    function cancel() {
      settle(false);
    }

    this.cancelWait = cancel;
    try {
      cleanup = register(complete, fail);
      if (settled) cleanup();
    } catch (error) {
      fail(error);
    }
  }
}

export class VideoReader {
  /**
   * Creates a reader for a video element or MP4 bytes. A Uint8Array source must already own its
   * memory (e.g. copied out of the WASM heap): the reader takes it over without copying again.
   */
  public static async create(
    source: Uint8Array | HTMLVideoElement,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
    owner: VideoReaderOwner,
  ): Promise<VideoReaderInterfaces> {
    return new VideoReader(source, width, height, frameRate, staticTimeRanges, owner);
  }

  public isSought = false;
  public isPlaying = false;
  public bitmap: ImageBitmap | null = null;
  public isDestroyed = false;

  private videoEl: HTMLVideoElement | null = null;
  private frameRate = 0;
  private staticTimeRanges: StaticTimeRanges;
  private disablePlaybackRate = false;
  private error: any = null;
  private owner: VideoReaderOwner | null = null;
  private currentFrame = -1;
  private objectURL: string | null = null;
  private prepareOperation: VideoReaderOperation | null = null;
  private playOperation: VideoReaderOperation | null = null;
  private temporaryPlayback: VideoReaderOperation | null = null;
  private playRequest: VideoReaderOperation | null = null;

  public constructor(
    source: Uint8Array | HTMLVideoElement,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
    owner: VideoReaderOwner,
  ) {
    if (owner.isDestroyed) throw new Error('Cannot create a video reader for a destroyed owner.');
    this.frameRate = frameRate;
    this.staticTimeRanges = new StaticTimeRanges(staticTimeRanges);
    this.disablePlaybackRate = UHD_RESOLUTION < width || UHD_RESOLUTION < height;
    try {
      this.initializeVideo(source, width, height);
      this.isPlaying = owner.isVideoPlaying();
      this.owner = owner;
      owner.linkVideoReader(this);
    } catch (error) {
      this.onDestroy();
      throw error;
    }
  }

  public async prepare(targetFrame: number, playbackRate: number): Promise<boolean> {
    if (this.isDestroyed || this.owner?.isDestroyed || !this.videoEl) return false;
    this.cancelPrepare();
    const operation = new VideoReaderOperation();
    this.prepareOperation = operation;
    this.error = null;
    this.isSought = false;
    let prepared = false;
    try {
      if (!(await this.prepareTargetFrame(operation, targetFrame)) || !this.isActive(operation)) return false;
      if (!(await this.coordinatePlayback(operation, playbackRate)) || !this.isActive(operation)) return false;
      this.currentFrame = targetFrame;
      prepared = true;
      return true;
    } catch (error) {
      if (this.isActive(operation)) this.error = error;
      return false;
    } finally {
      if (this.prepareOperation === operation) {
        this.finishOperation(operation, prepared);
        this.prepareOperation = null;
      }
    }
  }

  public cancelPrepare(): void {
    // Pause only playback that a cancelled prepare started itself (first-frame warmup). When
    // playback is intended (isPlaying), pausing here would reject the in-flight play() promise
    // with AbortError, and per-frame setProgress() invalidations would thrash play/pause and
    // keep postponing playback startup.
    const pausePlayback =
      !this.isPlaying && (this.temporaryPlayback !== null || this.playRequest !== null);
    this.prepareOperation?.cancel();
    this.playOperation?.cancel();
    this.prepareOperation = null;
    this.playOperation = null;
    this.temporaryPlayback = null;
    this.playRequest = null;
    if (pausePlayback && !this.isDestroyed) this.videoEl?.pause();
  }

  public getCurrentFrame(): number {
    return this.currentFrame;
  }

  public getVideo() {
    return this.videoEl;
  }

  public async play(): Promise<void> {
    if (this.isDestroyed || this.owner?.isDestroyed || !this.videoEl) return;
    this.playOperation?.cancel();
    const operation = new VideoReaderOperation();
    this.playOperation = operation;
    this.isPlaying = true;
    let played = false;
    try {
      played = await this.startPlayback(operation);
    } catch (error) {
      if (this.isActive(operation)) this.error = error;
      throw error;
    } finally {
      if (this.playOperation === operation) {
        this.finishOperation(operation, played);
        this.playOperation = null;
      }
    }
  }

  public pause() {
    this.isPlaying = false;
    this.cancelPrepare();
    if (!this.isDestroyed) this.videoEl?.pause();
  }

  public stop() {
    this.pause();
  }

  public getError() {
    return this.error;
  }

  public onDestroy() {
    if (this.isDestroyed) return;
    try {
      this.pause();
    } finally {
      try {
        this.owner?.unlinkVideoReader(this);
      } finally {
        this.owner = null;
        this.releaseVideo();
      }
    }
  }

  private releaseVideo() {
    const video = this.videoEl;
    const objectURL = this.objectURL;
    this.objectURL = null;
    this.videoEl = null;
    this.bitmap = null;
    this.isDestroyed = true;
    if (!objectURL) return;
    try {
      video?.removeAttribute('src');
    } finally {
      URL.revokeObjectURL(objectURL);
    }
  }

  private initializeVideo(source: Uint8Array | HTMLVideoElement, width: number, height: number) {
    if (isInstanceOf(source, globalThis.HTMLVideoElement)) {
      this.videoEl = source as HTMLVideoElement;
      return;
    }
    this.videoEl = document.createElement('video');
    this.videoEl.style.display = 'none';
    this.videoEl.muted = true;
    this.videoEl.playsInline = true;
    this.videoEl.preload = 'auto';
    this.videoEl.width = width;
    this.videoEl.height = height;
    // The source buffer is owned by this reader (see create()); the Blob copies it internally.
    const blob = new Blob([source as Uint8Array], { type: 'video/mp4' });
    this.objectURL = URL.createObjectURL(blob);
    this.videoEl.src = this.objectURL;
    if (IPHONE) {
      // Explicit loading is required on iPhone but can interrupt decoding on Chrome.
      this.videoEl.load();
    }
  }

  private async prepareTargetFrame(operation: VideoReaderOperation, targetFrame: number): Promise<boolean> {
    if (!this.isActive(operation)) return false;
    const video = this.videoEl!;
    const targetTime = targetFrame / this.frameRate;
    const currentTime = video.currentTime;
    const atTarget = !video.seeking && Math.round(currentTime * this.frameRate) === targetFrame;
    if (atTarget && targetFrame === this.currentFrame) return true;
    if (!video.seeking && currentTime === 0 && targetTime === 0) return this.prepareFirstFrame(operation);
    if (atTarget) return true;
    if (this.staticTimeRanges.contains(targetFrame)) return this.seek(operation, targetTime);
    if (
      video.seeking ||
      Math.abs(currentTime - targetTime) >= (1 / this.frameRate) * VIDEO_DECODE_WAIT_FRAME ||
      video.paused
    ) {
      this.isSought = true;
      return this.seek(operation, targetTime);
    }
    return true;
  }

  private async prepareFirstFrame(operation: VideoReaderOperation): Promise<boolean> {
    const video = this.videoEl!;
    const warmup = SAFARI_OR_IOS_WEBVIEW || video.readyState >= HTMLMediaElement.HAVE_FUTURE_DATA;
    if (!(await this.waitUntilReady(operation)) || !this.isActive(operation)) return false;
    if (!warmup) return true;
    // Safari may report canplay before its first frame is available to the texture uploader.
    this.temporaryPlayback = operation;
    if (!(await this.startPlayback(operation, true)) || !this.isActive(operation)) return false;
    if (
      !(await operation.wait(this.registerAnimationFrame.bind(this), VIDEO_WAIT_TIMEOUT, 'Video frame timed out.')) ||
      !this.isActive(operation)
    ) {
      return false;
    }
    if (!this.isPlaying) video.pause();
    this.temporaryPlayback = null;
    return true;
  }

  private async coordinatePlayback(operation: VideoReaderOperation, playbackRate: number): Promise<boolean> {
    if (!this.isActive(operation)) return false;
    const video = this.videoEl!;
    const targetRate = Math.min(Math.max(playbackRate, VIDEO_PLAYBACK_RATE_MIN), VIDEO_PLAYBACK_RATE_MAX);
    if (!this.disablePlaybackRate && video.playbackRate !== targetRate) video.playbackRate = targetRate;
    if (this.isPlaying && !(await this.startPlayback(operation))) return false;
    if (!this.isActive(operation)) return false;
    if (!this.isPlaying) video.pause();
    return true;
  }

  private isActive(operation: VideoReaderOperation): boolean {
    return (
      !this.isDestroyed &&
      !this.owner?.isDestroyed &&
      this.videoEl !== null &&
      !operation.cancelled &&
      (this.prepareOperation === operation || this.playOperation === operation)
    );
  }

  private finishOperation(operation: VideoReaderOperation, succeeded: boolean) {
    if ((!succeeded || !this.isPlaying) && (this.temporaryPlayback === operation || this.playRequest === operation)) {
      this.videoEl?.pause();
    }
    if (this.temporaryPlayback === operation) this.temporaryPlayback = null;
    if (this.playRequest === operation) this.playRequest = null;
    operation.cancel();
  }

  private waitUntilReady(operation: VideoReaderOperation): Promise<boolean> {
    if (!this.isActive(operation)) return Promise.resolve(false);
    if (this.videoEl!.readyState >= HTMLMediaElement.HAVE_FUTURE_DATA) return Promise.resolve(true);
    return operation.wait(
      this.registerVideoEvent.bind(this, 'canplay'),
      VIDEO_WAIT_TIMEOUT,
      'Video canplay timed out.',
    );
  }

  private async seek(operation: VideoReaderOperation, targetTime: number): Promise<boolean> {
    if (!(await this.waitUntilReady(operation)) || !this.isActive(operation)) return false;
    return operation.wait(
      this.registerSeek.bind(this, targetTime),
      (1000 / this.frameRate) * VIDEO_DECODE_SEEK_TIMEOUT_FRAME,
      'Seek operation timed out.',
    );
  }

  private async startPlayback(operation: VideoReaderOperation, temporary = false): Promise<boolean> {
    if (!this.isActive(operation)) return false;
    if ((!temporary && !this.isPlaying) || !this.videoEl!.paused) return true;
    if (WECHAT && window.WeixinJSBridge) {
      if (
        !(await operation.wait(
          this.registerWechatBridge.bind(this),
          VIDEO_WAIT_TIMEOUT,
          'Wechat authorization timed out.',
        )) ||
        !this.isActive(operation)
      ) {
        return false;
      }
    }
    if (document.visibilityState !== 'visible') {
      if (
        !(await operation.wait(
          this.registerVisibility.bind(this),
          VIDEO_WAIT_TIMEOUT,
          'Video visibility timed out.',
        )) ||
        !this.isActive(operation)
      ) {
        return false;
      }
    }
    if (!temporary && !this.isPlaying) return true;
    this.playRequest = operation;
    const played = await operation.wait(this.registerPlay.bind(this), VIDEO_WAIT_TIMEOUT, 'Video play timed out.');
    if (!this.isActive(operation)) return false;
    if (played) this.playRequest = null;
    return played;
  }

  private registerVideoEvent(event: 'canplay' | 'seeked', complete: () => void, fail: (error: any) => void) {
    const video = this.videoEl!;
    function onError() {
      fail(video.error ?? new Error(`Video ${event} failed.`));
    }
    function cleanup() {
      removeListener(video, event, complete);
      removeListener(video, 'error', onError);
    }
    addListener(video, event, complete);
    addListener(video, 'error', onError);
    return cleanup;
  }

  private registerSeek(targetTime: number, complete: () => void, fail: (error: any) => void) {
    const video = this.videoEl!;
    const frameRate = this.frameRate;
    function onSeeked() {
      if (!video.seeking && Math.round(video.currentTime * frameRate) === Math.round(targetTime * frameRate)) {
        complete();
      }
    }
    const cleanup = this.registerVideoEvent('seeked', onSeeked, fail);
    try {
      video.currentTime = targetTime;
    } catch (error) {
      cleanup();
      throw error;
    }
    return cleanup;
  }

  private registerAnimationFrame(complete: () => void) {
    const handle = requestAnimationFrame(complete);
    function cleanup() {
      cancelAnimationFrame(handle);
    }
    return cleanup;
  }

  private registerWechatBridge(complete: () => void) {
    window.WeixinJSBridge.invoke('getNetworkType', {}, complete, complete);
    function cleanup() {}
    return cleanup;
  }

  private registerVisibility(complete: () => void) {
    function onVisibility() {
      if (document.visibilityState === 'visible') complete();
    }
    function cleanup() {
      document.removeEventListener('visibilitychange', onVisibility);
    }
    document.addEventListener('visibilitychange', onVisibility);
    return cleanup;
  }

  private registerPlay(complete: () => void, fail: (error: any) => void) {
    this.videoEl!.play().then(complete, fail);
    function cleanup() {}
    return cleanup;
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
