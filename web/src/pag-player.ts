import {PAGModule} from './pag-module';
import {PAGFile} from './pag-file';
import {PAGSurface} from './pag-surface';
import {destroyVerify} from './utils/decorators';
import {getWasmIns, layer2typeLayer} from './utils/type-utils';
import {Matrix} from './core/matrix';

import {PAGComposition} from './pag-composition';
import type {PAGLayer} from './pag-layer';
import {type PAGScaleMode, type Rect, VecArray} from './types';
import type {VideoReader} from './interfaces';
import {VideoReaderManager, type VideoFrameTarget} from './video-reader-manager';
import {WebAssemblyQueue} from './utils/queue';

type PlayerOperation = 'prepare' | 'flush';

@destroyVerify
export class PAGPlayer {
    public static create(): PAGPlayer {
        const wasmIns = new PAGModule._PAGPlayer();
        if (!wasmIns) throw new Error('Create PAGPlayer fail!');
        return new PAGPlayer(wasmIns);
    }

    private static ExecuteQueuedOperation(this: PAGPlayer, operation: PlayerOperation,
        callback: ((res: boolean) => void) | null, version: number): Promise<boolean> | boolean {
        if (this.isDestroyed) return false;
        return this.executeOperation(operation, callback, version);
    }

    public wasmIns;
    public isDestroyed = false;
    public videoReaders: VideoReader[] = [];

    protected pagComposition: PAGComposition | null = null;
    protected videoReaderManager: VideoReaderManager | null = null;
    private operationQueue = new WebAssemblyQueue();
    private compositionVersion = 0;
    private frameVersion = 0;
    private videoPlaying = false;

    public constructor(wasmIns: any) {
        this.wasmIns = wasmIns;
    }

    /**
     * Set the progress of play position, the value is from 0.0 to 1.0.
     */
    public setProgress(progress: number): void {
        const previous = this.wasmIns._currentFrame();
        this.wasmIns._setProgress(progress);
        if (previous !== this.wasmIns._currentFrame()) this.invalidateVideoFrame();
    }

    /**
     * Apply all pending changes to the target surface immediately. Returns true if the content has
     * changed.
     */
    public flush(): Promise<boolean> {
        return this.enqueueOperation('flush', null);
    }

    /**
     * [Internal] Apply all pending changes to the target surface immediately. Returns true if the content has
     * changed.
     */
    public async flushInternal(callback: (res: boolean) => void) {
        const res = await this.enqueueOperation('flush', callback);
        // Log video reader errors but don't throw - video decoding failures shouldn't stop playback
        for (const videoReader of this.videoReaders) {
            const error = await videoReader.getError();
            if (error !== null) {
                console.warn('[PAGPlayer] VideoReader error:', error);
            }
        }
        return res;
    }

    /**
     * The duration of current composition in microseconds.
     */
    public duration(): number {
        return this.wasmIns._duration() as number;
    }

    /**
     * Returns the current progress of play position, the value is from 0.0 to 1.0.
     */
    public getProgress(): number {
        return this.wasmIns._getProgress() as number;
    }

    /**
     * Returns the current frame.
     */
    public currentFrame(): number {
        return this.wasmIns._currentFrame() as number;
    }

    /**
     * If set to false, the player skips rendering for video composition.
     */
    public videoEnabled(): boolean {
        return this.wasmIns._videoEnabled() as boolean;
    }

    /**
     * Set the value of videoEnabled property.
     */
    public setVideoEnabled(enabled: boolean): void {
        if (this.videoEnabled() !== enabled) this.invalidateVideoFrame();
        this.wasmIns._setVideoEnabled(enabled);
        if (!enabled) {
            for (const reader of this.videoReaders) reader.pause();
        }
    }

    /**
     * If set to true, PAGPlayer caches an internal bitmap representation of the static content for each
     * layer. This caching can increase performance for layers that contain complex vector content. The
     * execution speed can be significantly faster depending on the complexity of the content, but it
     * requires extra graphics memory. The default value is true.
     */
    public cacheEnabled(): boolean {
        return this.wasmIns._cacheEnabled() as boolean;
    }

    /**
     * Set the value of cacheEnabled property.
     */
    public setCacheEnabled(enabled: boolean): void {
        this.wasmIns._setCacheEnabled(enabled);
    }

    /**
     * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
     * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of graphics
     * memory which leads to better performance. The default value is 1.0.
     */
    public cacheScale(): number {
        return this.wasmIns._cacheScale() as number;
    }

    /**
     * Set the value of cacheScale property.
     */
    public setCacheScale(value: number): void {
        this.wasmIns._setCacheScale(value);
    }

    /**
     * The maximum frame rate for rendering, ranges from 1 to 60. If set to a value less than the actual
     * frame rate from composition, it drops frames but increases performance. Otherwise, it has no
     * effect. The default value is 60.
     */
    public maxFrameRate(): number {
        return this.wasmIns._maxFrameRate() as number;
    }

    /**
     * Set the maximum frame rate for rendering.
     */
    public setMaxFrameRate(value: number): void {
        this.wasmIns._setMaxFrameRate(value);
    }

    /**
     * Returns the current scale mode.
     */
    public scaleMode(): PAGScaleMode {
        return this.wasmIns._scaleMode() as PAGScaleMode;
    }

    /**
     * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
     * changes when this method is called.
     */
    public setScaleMode(value: PAGScaleMode): void {
        this.wasmIns._setScaleMode(value);
    }

    /**
     * Set the PAGSurface object for PAGPlayer to render onto.
     */
    public setSurface(pagSurface: PAGSurface | null): void {
        this.wasmIns._setSurface(getWasmIns(pagSurface));
    }

    /**
     *
     * Returns the current PAGComposition for PAGPlayer to render as content.
     */
    public getComposition(): PAGComposition {
        const wasmIns = this.wasmIns._getComposition();
        if (!wasmIns) throw new Error('Get composition fail!');
        if (wasmIns._isPAGFile()) {
            return new PAGFile(wasmIns);
        }
        return new PAGComposition(wasmIns);
    }

    /**
     *
     * Sets a new PAGComposition for PAGPlayer to render as content.
     */

    public setComposition(pagComposition: PAGComposition | null) {
        this.destroyVideoReaderManager();
        this.pagComposition = pagComposition;
        this.wasmIns._setComposition(getWasmIns(pagComposition));
    }

    /**
     * Returns the PAGSurface object for PAGPlayer to render onto.
     */
    public getSurface(): PAGSurface {
        const wasmIns = this.wasmIns._getSurface();
        if (!wasmIns) throw new Error('Get surface fail!');
        return new PAGSurface(wasmIns);
    }

    /**
     * Returns a copy of current matrix.
     */
    public matrix(): Matrix {
        const wasmIns = this.wasmIns._matrix();
        if (!wasmIns) throw new Error('Get matrix fail!');
        return new Matrix(wasmIns);
    }

    /**
     * Set the transformation which will be applied to the composition. The scaleMode property
     * will be set to PAGScaleMode::None when this method is called.
     */
    public setMatrix(matrix: Matrix) {
        this.wasmIns._setMatrix(matrix.wasmIns);
    }

    /**
     * Set the progress of play position to next frame. It is applied only when the composition is not
     * null.
     */
    public nextFrame() {
        this.wasmIns._nextFrame();
        this.invalidateVideoFrame();
    }

    /**
     * Set the progress of play position to previous frame. It is applied only when the composition is
     * not null.
     */
    public preFrame() {
        this.wasmIns._preFrame();
        this.invalidateVideoFrame();
    }

    /**
     * If true, PAGPlayer clears the whole content of PAGSurface before drawing anything new to it.
     * The default value is true.
     */
    public autoClear(): boolean {
        return this.wasmIns._autoClear() as boolean;
    }

    /**
     * Sets the autoClear property.
     */
    public setAutoClear(value: boolean) {
        this.wasmIns._setAutoClear(value);
    }

    /**
     * Returns a rectangle that defines the original area of the layer, which is not transformed by
     * the matrix.
     */
    public getBounds(pagLayer: PAGLayer): Rect {
        return this.wasmIns._getBounds(pagLayer.wasmIns) as Rect;
    }

    /**
     * Returns an array of layers that lie under the specified point. The point is in pixels and from
     *
     * this PAGComposition's local coordinates.
     */
    public getLayersUnderPoint(localX: number, localY: number) {
        const wasmIns = this.wasmIns._getLayersUnderPoint(localX, localY);
        if (!wasmIns) throw new Error(`Get layers under point, x: ${localX} y:${localY} fail!`);
        const layerArray = VecArray.create();
        for (const wasmIn of wasmIns) {
            layerArray.push(layer2typeLayer(wasmIn));
        }
        return layerArray;
    }

    /**
     * Evaluates the PAGLayer to see if it overlaps or intersects with the specified point. The point
     *
     * is in the coordinate space of the PAGSurface, not the PAGComposition that contains the
     * PAGLayer. It always returns false if the PAGLayer or its parent (or parent's parent...) has not
     * been added to this PAGPlayer. The pixelHitTest parameter indicates whether or not to check
     * against the actual pixels of the object (true) or the bounding box (false). Returns true if the
     * PAGLayer overlaps or intersects with the specified point.
     */
    public hitTestPoint(pagLayer: PAGLayer, surfaceX: number, surfaceY: number, pixelHitTest = false): boolean {
        return this.wasmIns._hitTestPoint(pagLayer.wasmIns, surfaceX, surfaceY, pixelHitTest) as boolean;
    }

    /**
     * The time cost by rendering in microseconds.
     */
    public renderingTime(): number {
        return this.wasmIns._renderingTime() as number;
    }

    /**
     * The time cost by image decoding in microseconds.
     */
    public imageDecodingTime(): number {
        return this.wasmIns._imageDecodingTime() as number;
    }

    /**
     * The time cost by presenting in microseconds.
     */
    public presentingTime(): number {
        return this.wasmIns._presentingTime() as number;
    }

    /**
     * The memory cost by graphics in bytes.
     */
    public graphicsMemory(): number {
        return this.wasmIns._graphicsMemory() as number;
    }

    /**
     * Prepares video frames and native resources at the current progress without presenting a frame.
     * Initial video loading may take time. Invalidated requests finish without using stale resources.
     */
    public async prepare(): Promise<void> {
        await this.enqueueOperation('prepare', null);
    }

    public destroy() {
        this.destroyVideoReaderManager();
        this.wasmIns.delete();
        this.isDestroyed = true;
    }

    /**
     * Link VideoReader to PAGPlayer.
     */
    public linkVideoReader(videoReader: VideoReader) {
        this.videoReaders.push(videoReader);
    }

    /**
     * Unlink VideoReader from PAGPlayer.
     */
    public unlinkVideoReader(videoReader: VideoReader) {
        const index = this.videoReaders.indexOf(videoReader);
        if (index !== -1) {
            this.videoReaders.splice(index, 1);
        }
    }

    /** Returns the playback intent inherited by newly created video readers. */
    public isVideoPlaying(): boolean {
        return this.videoPlaying;
    }

    /** Applies playback intent to current readers and readers created by future preparation. */
    public setVideoPlaying(playing: boolean): void {
        if (this.videoPlaying !== playing) this.invalidateVideoFrame();
        this.videoPlaying = playing;
        for (const reader of this.videoReaders) {
            reader.isPlaying = playing;
            if (!playing) reader.pause();
        }
    }

    /** Cancels pending composition work and releases all associated video resources. */
    public destroyVideoReaderManager(): void {
        this.compositionVersion++;
        this.frameVersion++;
        const manager = this.videoReaderManager;
        this.videoReaderManager = null;
        if (manager) {
            if (PAGModule.videoReaderManager === manager) PAGModule.videoReaderManager = undefined;
            manager.destroy();
        }
    }

    private invalidateVideoFrame(): void {
        this.frameVersion++;
        this.videoReaderManager?.cancelPrepare();
    }

    private needsVideoPreparation(): boolean {
        return PAGModule._useSoftwareDecoder === false && this.videoEnabled() && this.pagComposition !== null;
    }

    private enqueueOperation(operation: PlayerOperation, callback: ((res: boolean) => void) | null): Promise<boolean> {
        return this.operationQueue.exec(PAGPlayer.ExecuteQueuedOperation, this,
            operation, callback, this.compositionVersion);
    }

    private async executeOperation(operation: PlayerOperation, callback: ((res: boolean) => void) | null,
        compositionVersion: number): Promise<boolean> {
        while (!this.isDestroyed && this.compositionVersion === compositionVersion) {
            const version = this.frameVersion;
            const needsVideo = this.needsVideoPreparation();
            let manager: VideoReaderManager | null = null;
            let plan: VideoFrameTarget[] = [];
            if (needsVideo) {
                if (!this.videoReaderManager && VideoReaderManager.HasVideo(this.pagComposition!.wasmIns)) {
                    this.videoReaderManager = new VideoReaderManager(this.pagComposition!.wasmIns, this);
                }
                manager = this.videoReaderManager;
                if (manager) {
                    try {
                        if (!await manager.initialize()) return false;
                    } catch (error) {
                        if (this.videoReaderManager === manager) this.videoReaderManager = null;
                        if (this.compositionVersion !== compositionVersion) return false;
                        throw error;
                    }
                    if (this.compositionVersion !== compositionVersion) return false;
                    plan = manager.captureTargets();
                    const preparation = await manager.prepareTargets(plan);
                    if (this.compositionVersion !== compositionVersion) return false;
                    if (version !== this.frameVersion || !manager.matchesTargets(plan)) continue;
                    if (preparation === 'cancelled') return false;
                    // Frame failures must still reach native rendering so other layers and its fallback path can run.
                }
            }
            const result = await PAGModule.webAssemblyQueue.exec<boolean | undefined>(this.invokeNative, this,
                operation, callback, compositionVersion, version, manager, plan, needsVideo);
            if (result !== undefined) return result;
        }
        return false;
    }

    private invokeNative(operation: PlayerOperation, callback: ((res: boolean) => void) | null,
        compositionVersion: number, frameVersion: number, manager: VideoReaderManager | null,
        plan: VideoFrameTarget[], needsVideo: boolean): boolean | undefined {
        if (this.isDestroyed || this.compositionVersion !== compositionVersion) return false;
        if (frameVersion !== this.frameVersion || needsVideo !== this.needsVideoPreparation() ||
            (manager && !manager.matchesTargets(plan))) return undefined;
        const previousPlayer = PAGModule.currentPlayer;
        const previousManager = PAGModule.videoReaderManager;
        PAGModule.currentPlayer = this;
        PAGModule.videoReaderManager = manager ?? undefined;
        try {
            if (operation === 'prepare') {
                this.wasmIns._prepare();
                return true;
            }
            const result = this.wasmIns._flush() as boolean;
            if (callback) callback(result);
            return result;
        } finally {
            PAGModule.currentPlayer = previousPlayer;
            PAGModule.videoReaderManager = previousManager;
        }
    }

}
