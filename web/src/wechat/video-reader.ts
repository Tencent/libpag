import { MP4_CACHE_PATH } from './constant';
import { removeFile, touchDirectory, writeFile } from './file-utils';
import { ArrayBufferImage } from '@tgfx/wechat/array-buffer-image';

import type { FrameData } from '@tgfx/wechat/array-buffer-image';
import type { FrameDataOptions, VideoDecoder, wx } from './interfaces';
import type { VideoReaderOwner } from '../interfaces';

declare const wx: wx;
declare const setInterval: (callback: () => void, delay: number) => number;

const BUFFER_MAX_SIZE = 6;
const BUFFER_MIN_SIZE = 2;
const GET_FRAME_DATA_INTERVAL = 2; // ms
const PREPARE_TIMEOUT = 10000; // ms
let VideoReaderID = 0;

function FrameDataOptionsToFrameData(id: number, options: FrameDataOptions): FrameData {
  const data = new ArrayBuffer(options.data.byteLength);
  new Uint8Array(data).set(new Uint8Array(options.data));
  return { id, data, width: options.width, height: options.height };
}

function IgnoreDecoderRemovalError() {}

export interface TimeRange {
  start: number;
  end: number;
}

export class VideoReader {
  public static async create(
    mp4Data: Uint8Array,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
    owner: VideoReaderOwner,
  ): Promise<VideoReader> {
    return new VideoReader(mp4Data, width, height, frameRate, staticTimeRanges, owner);
  }

  public isSought = false; // Web SDK use this property to check if video is seeked
  public isPlaying = false; // Web SDK use this property to check if video is playing
  public isDestroyed = false;
  private owner: VideoReaderOwner | null;
  private error: any = null;
  private readonly frameRate: number;
  private currentFrame = -1;
  private mp4Path: string;
  private videoDecoder: VideoDecoder | null = null;
  private decoderReady = false;
  private decoderOperationPending = false;
  private decoderOperationResolve: ((ready: boolean) => void) | null = null;
  private frameDataBuffers: FrameData[] = [];
  private bufferIndex = 0; // next frameData id
  private getFrameDataResolve: ((frameData: FrameData | null) => void) | null = null;
  private getFrameDataLoopTimer: number | null = null;
  private decodeEnabled = true;
  private prepareID = 0;
  private prepareTimer: ReturnType<typeof setTimeout> | null = null;
  private arrayBufferImage = new ArrayBufferImage(new ArrayBuffer(0), 0, 0);
  private readonly endedHandler = this.onDecoderEnded.bind(this);

  public constructor(
    mp4Data: Uint8Array,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
    owner: VideoReaderOwner,
  ) {
    this.frameRate = frameRate;
    this.owner = owner;
    this.mp4Path = `${MP4_CACHE_PATH}${Date.now()}-${VideoReaderID++}.mp4`;
    if (owner.isDestroyed) throw new Error('Video reader owner is destroyed.');
    try {
      touchDirectory(MP4_CACHE_PATH);
      writeFile(this.mp4Path, mp4Data.buffer.slice(mp4Data.byteOffset, mp4Data.byteLength + mp4Data.byteOffset));
      this.isPlaying = owner.isVideoPlaying();
      owner.linkVideoReader(this);
      this.startDecoder();
    } catch (error) {
      this.onDestroy();
      throw error;
    }
  }

  /** Resolves true only after the requested frame is ready; cancellation and failure resolve false. */
  public async prepare(targetFrame: number): Promise<boolean> {
    if (this.isDestroyed || !this.owner || this.owner.isDestroyed) {
      this.cancelPrepare();
      return false;
    }
    if (this.prepareTimer !== null) this.cancelPrepare();
    this.prepareID += 1;
    this.isPlaying = this.owner.isVideoPlaying();
    this.isSought = false;
    this.error = null;
    this.decodeEnabled = true;
    if (targetFrame === this.currentFrame) {
      if (this.frameDataBuffers.length <= BUFFER_MIN_SIZE) this.startGetFrameDataLoop();
      return true;
    }
    const prepareID = this.prepareID;
    this.prepareTimer = setTimeout(this.onPrepareTimeout.bind(this, prepareID), PREPARE_TIMEOUT);
    let prepared = false;
    try {
      const frameData = await this.prepareFrame(targetFrame, prepareID);
      if (!frameData || !this.isPrepareCurrent(prepareID)) return false;
      this.arrayBufferImage.setFrameData(frameData);
      this.currentFrame = targetFrame;
      prepared = true;
      return true;
    } catch (error) {
      if (this.isPrepareCurrent(prepareID)) this.error = error;
      return false;
    } finally {
      if (prepareID === this.prepareID) {
        this.clearPrepareTimer();
        if (!prepared) this.cancelPrepare();
      }
    }
  }

  /** Cancels logical waits without releasing the native start/seek barrier, which WeChat cannot abort. */
  public cancelPrepare(): void {
    this.prepareID += 1;
    this.decodeEnabled = false;
    this.clearPrepareTimer();
    this.clearFrameDataLoop();
    this.settleDecoderOperation(false);
    this.settleFrameData(null);
  }

  public getVideo() {
    return this.arrayBufferImage;
  }

  public getCurrentFrame(): number {
    return this.currentFrame;
  }

  public async play() {
    // Web SDK use this function to play video.
  }

  public pause() {
    // Web SDK use this function to pause video.
  }

  public stop() {
    // Web SDK use this function to stop video.
  }

  public getError(): any {
    return this.error;
  }

  public onDestroy() {
    if (this.isDestroyed) return;
    this.isDestroyed = true;
    this.cancelPrepare();
    this.frameDataBuffers = [];
    this.removeDecoder();
    if (this.mp4Path) {
      removeFile(this.mp4Path);
      this.mp4Path = '';
    }
    const owner = this.owner;
    this.owner = null;
    owner?.unlinkVideoReader(this);
  }

  private async prepareFrame(targetFrame: number, prepareID: number): Promise<FrameData | null> {
    if (this.decoderOperationPending && !(await this.waitForDecoderOperation())) return null;
    if (!this.isPrepareCurrent(prepareID)) return null;
    if (!this.decoderReady) {
      this.startDecoder();
      if (!(await this.waitForDecoderOperation())) return null;
      if (!this.isPrepareCurrent(prepareID)) return null;
    }
    let bufferOffset = -1;
    for (let index = 0; index < this.frameDataBuffers.length; index++) {
      if (this.frameDataBuffers[index].id === targetFrame) {
        bufferOffset = index;
        break;
      }
    }
    if (bufferOffset !== -1) {
      this.frameDataBuffers = this.frameDataBuffers.slice(bufferOffset);
    } else {
      this.frameDataBuffers = [];
      if (targetFrame !== this.bufferIndex) {
        this.isSought = true;
        this.seekDecoder(targetFrame);
        if (!(await this.waitForDecoderOperation())) return null;
        if (!this.isPrepareCurrent(prepareID)) return null;
      }
    }
    return this.getFrameData();
  }

  private isPrepareCurrent(prepareID: number): boolean {
    return prepareID === this.prepareID && !this.isDestroyed && !!this.owner && !this.owner.isDestroyed;
  }

  private onPrepareTimeout(prepareID: number) {
    if (prepareID !== this.prepareID || this.isDestroyed) return;
    this.error = new Error('Video decoder preparation timed out.');
    this.cancelPrepare();
  }

  private clearPrepareTimer() {
    if (this.prepareTimer !== null) {
      clearTimeout(this.prepareTimer);
      this.prepareTimer = null;
    }
  }

  private startDecoder() {
    if (this.decoderOperationPending || this.decoderReady || this.isDestroyed) return;
    try {
      this.videoDecoder = wx.createVideoDecoder();
      this.videoDecoder.on('ended', this.endedHandler);
      this.decoderOperationPending = true;
      this.videoDecoder
        .start({ source: this.mp4Path, mode: 1 })
        .then(
          this.onDecoderOperationReady.bind(this, this.videoDecoder, null),
          this.onDecoderOperationError.bind(this, this.videoDecoder),
        );
    } catch (error) {
      this.removeDecoder();
      throw error;
    }
  }

  private seekDecoder(targetFrame: number) {
    if (!this.videoDecoder || this.decoderOperationPending || this.isDestroyed) return;
    this.decoderOperationPending = true;
    this.clearFrameDataLoop();
    try {
      this.videoDecoder
        .seek(Math.floor((targetFrame / this.frameRate) * 1000))
        .then(
          this.onDecoderOperationReady.bind(this, this.videoDecoder, targetFrame),
          this.onDecoderOperationError.bind(this, this.videoDecoder),
        );
    } catch (error) {
      this.onDecoderOperationError(this.videoDecoder, error);
    }
  }

  private onDecoderOperationReady(decoder: VideoDecoder, targetFrame: number | null) {
    if (this.isDestroyed || this.videoDecoder !== decoder) return;
    this.decoderOperationPending = false;
    this.decoderReady = true;
    this.bufferIndex = targetFrame === null ? 0 : targetFrame;
    this.settleDecoderOperation(true);
    this.startGetFrameDataLoop();
  }

  private onDecoderOperationError(decoder: VideoDecoder, error: any) {
    if (this.isDestroyed || this.videoDecoder !== decoder) return;
    this.error = error;
    this.decoderOperationPending = false;
    this.bufferIndex = -1;
    this.frameDataBuffers = [];
    this.clearFrameDataLoop();
    this.settleDecoderOperation(false);
    this.settleFrameData(null);
    if (!this.decoderReady) this.removeDecoder();
  }

  private onDecoderEnded() {
    if (this.isDestroyed || !this.decodeEnabled || !this.decoderReady || this.decoderOperationPending) return;
    this.seekDecoder(0);
  }

  private waitForDecoderOperation(): Promise<boolean> {
    if (!this.decoderOperationPending) return Promise.resolve(this.decoderReady && this.error === null);
    return new Promise<boolean>(this.setDecoderOperationResolve.bind(this));
  }

  private setDecoderOperationResolve(resolve: (ready: boolean) => void) {
    this.decoderOperationResolve = resolve;
  }

  private settleDecoderOperation(ready: boolean) {
    const resolve = this.decoderOperationResolve;
    this.decoderOperationResolve = null;
    resolve?.(ready);
  }

  private removeDecoder() {
    const decoder = this.videoDecoder;
    this.videoDecoder = null;
    this.decoderReady = false;
    this.decoderOperationPending = false;
    if (!decoder) return;
    try {
      decoder.off('ended', this.endedHandler);
    } catch (error) {
      this.error = error;
    }
    try {
      decoder.remove().catch(IgnoreDecoderRemovalError);
    } catch (error) {
      this.error = error;
    }
  }

  private getFrameData(): Promise<FrameData | null> {
    if (this.frameDataBuffers.length <= BUFFER_MIN_SIZE) this.startGetFrameDataLoop();
    const frameData = this.frameDataBuffers.shift();
    if (frameData) return Promise.resolve(frameData);
    return new Promise<FrameData | null>(this.setFrameDataResolve.bind(this));
  }

  private setFrameDataResolve(resolve: (frameData: FrameData | null) => void) {
    this.getFrameDataResolve = resolve;
  }

  private settleFrameData(frameData: FrameData | null) {
    const resolve = this.getFrameDataResolve;
    this.getFrameDataResolve = null;
    resolve?.(frameData);
  }

  private startGetFrameDataLoop() {
    if (
      this.isDestroyed ||
      !this.decodeEnabled ||
      !this.decoderReady ||
      this.decoderOperationPending ||
      this.getFrameDataLoopTimer !== null
    ) {
      return;
    }
    this.getFrameDataLoopTimer = setInterval(this.getFrameDataLoop.bind(this), GET_FRAME_DATA_INTERVAL);
  }

  private getFrameDataLoop() {
    if (this.isDestroyed || this.decoderOperationPending || !this.videoDecoder) return;
    if (this.owner?.isDestroyed) {
      this.cancelPrepare();
      return;
    }
    if (this.frameDataBuffers.length >= BUFFER_MAX_SIZE) {
      this.clearFrameDataLoop();
      return;
    }
    try {
      const options = this.videoDecoder.getFrameData();
      if (options === null) return;
      const frameData = FrameDataOptionsToFrameData(this.bufferIndex, options);
      this.bufferIndex += 1;
      if (this.getFrameDataResolve) {
        this.settleFrameData(frameData);
      } else {
        this.frameDataBuffers.push(frameData);
      }
    } catch (error) {
      this.error = error;
      this.bufferIndex = -1;
      this.frameDataBuffers = [];
      this.cancelPrepare();
    }
  }

  private clearFrameDataLoop() {
    if (this.getFrameDataLoopTimer !== null) {
      clearInterval(this.getFrameDataLoopTimer);
      this.getFrameDataLoopTimer = null;
    }
  }
}
