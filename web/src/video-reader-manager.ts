import { PAGModule } from './pag-module';
import type { VideoReader, VideoReaderOwner } from './interfaces';

export type VideoPreparationResult = 'ready' | 'failed' | 'cancelled';

export interface VideoFrameTarget {
  id: number;
  frame: number;
  playbackRate: number;
}

/** Owns the video resources and requested frames for one composition. */
export class VideoReaderManager {
  public static HasVideo(wasmIns: any): boolean {
    return PAGModule._videoInfoManager._HasVideo(wasmIns) as boolean;
  }

  public wasmIns: any;
  public videoIDs: number[] = [];
  public isDestroyed = false;
  private owner: VideoReaderOwner;
  private videoReaderMap = new Map<number, VideoReader>();
  private targets = new Map<number, VideoFrameTarget>();
  private initialization: Promise<boolean> | null = null;
  private prepareVersion = 0;
  private cancelInitialization!: () => void;
  private cancelled: Promise<undefined>;

  public constructor(wasmIns: any, owner: VideoReaderOwner) {
    this.owner = owner;
    this.cancelled = new Promise(this.setCancellation.bind(this));
    this.wasmIns = PAGModule._videoInfoManager._Make(wasmIns);
    if (!this.wasmIns) throw new Error('create VideoReaderManager fail!');
    try {
      this.videoIDs = this.wasmIns._getVideoIDs() as number[];
      if (this.wasmIns._hasTimeRangeOverlap()) {
        console.error('The current file references the same video on overlapping timelines. Offset the timelines of these layers.');
      }
    } catch (error) {
      this.wasmIns.delete();
      throw error;
    }
  }

  /** Initializes readers once without depending on module-global player ownership. */
  public initialize(): Promise<boolean> {
    if (this.isDestroyed) return Promise.resolve(false);
    if (!this.initialization) this.initialization = this.createReaders();
    return this.initialization;
  }

  /** Captures all targets before any asynchronous decoding and retains unconfirmed requests. */
  public captureTargets(): VideoFrameTarget[] {
    if (this.isDestroyed) return [];
    const plan: VideoFrameTarget[] = [];
    for (const id of this.videoIDs) {
      const frame = this.wasmIns._getTargetFrameByID(id) as number;
      const previous = this.targets.get(id);
      const target = { id, frame: frame < 0 ? (previous?.frame ?? 0) : frame,
        playbackRate: this.wasmIns._getPlaybackRateByID(id) as number };
      this.targets.set(id, target);
      plan.push(target);
    }
    return plan;
  }

  /** Checks the current timeline again immediately before entering WASM. */
  public matchesTargets(plan: VideoFrameTarget[]): boolean {
    if (this.isDestroyed) return false;
    const current = this.captureTargets();
    if (current.length !== plan.length) return false;
    for (let index = 0; index < current.length; index++) {
      const target = current[index];
      if (target.id !== plan[index].id || target.frame !== plan[index].frame ||
          target.playbackRate !== plan[index].playbackRate) return false;
    }
    return true;
  }

  /** Prepares all videos despite individual failures, but stops when the request is cancelled. */
  public async prepareTargets(plan: VideoFrameTarget[]): Promise<VideoPreparationResult> {
    const version = this.prepareVersion;
    let failed = false;
    for (const target of plan) {
      if (this.isDestroyed || version !== this.prepareVersion) return 'cancelled';
      const reader = this.videoReaderMap.get(target.id);
      if (!reader) {
        failed = true;
        continue;
      }
      reader.isPlaying = this.owner.isVideoPlaying();
      const prepared = await reader.prepare(target.frame, target.playbackRate);
      if (this.isDestroyed || version !== this.prepareVersion) return 'cancelled';
      if (!prepared) failed = true;
    }
    if (this.isDestroyed || version !== this.prepareVersion) return 'cancelled';
    return failed ? 'failed' : 'ready';
  }

  /** Supplies the reader to the native decoder during the owner's WASM call. */
  public getVideoReaderByID(id: number): VideoReader | undefined {
    return this.isDestroyed ? undefined : this.videoReaderMap.get(id);
  }

  /** Invalidates in-flight frame preparation without destroying reusable decoders. */
  public cancelPrepare(): void {
    this.prepareVersion++;
    for (const reader of this.videoReaderMap.values()) reader.cancelPrepare();
  }

  /** Cancels all waits and releases both initialized and subsequently resolving readers. */
  public destroy(): void {
    if (this.isDestroyed) return;
    this.isDestroyed = true;
    this.cancelInitialization();
    this.cancelPrepare();
    try {
      for (const reader of this.videoReaderMap.values()) reader.onDestroy();
    } finally {
      this.videoReaderMap.clear();
      this.targets.clear();
      this.wasmIns.delete();
    }
  }

  private setCancellation(resolve: (value: undefined) => void): void {
    this.cancelInitialization = resolve.bind(undefined, undefined);
  }

  private acceptReader(reader: VideoReader): VideoReader | undefined {
    if (this.isDestroyed || this.owner.isDestroyed) {
      reader.onDestroy();
      return undefined;
    }
    return reader;
  }

  private async createReaders(): Promise<boolean> {
    try {
      for (const id of this.videoIDs) {
        if (this.isDestroyed || this.owner.isDestroyed) return false;
        const data = this.wasmIns._getMp4DataByID(id) as Uint8Array | null;
        if (!data) throw new Error(`Missing MP4 data for video ${id}`);
        const pending = PAGModule.VideoReader.create(data.slice(), this.wasmIns._getWidthByID(id),
          this.wasmIns._getHeightByID(id), this.wasmIns._getFrameRateByID(id),
          this.wasmIns._getStaticTimeRangesByID(id), this.owner).then(this.acceptReader.bind(this));
        const reader = await Promise.race([pending, this.cancelled]);
        if (!reader) return false;
        if (this.isDestroyed || this.owner.isDestroyed) {
          reader.onDestroy();
          return false;
        }
        this.videoReaderMap.set(id, reader);
      }
      return !this.isDestroyed;
    } catch (error) {
      this.destroy();
      throw error;
    }
  }
}
