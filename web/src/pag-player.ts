import { PAGFile } from './pag-file';
import { PAGSurface } from './pag-surface';
import { PAG, PAGScaleMode } from './types';

export class PAGPlayer {
  public static module: PAG;

  public static create() {
    return this.module._PAGPlayer.create();
  }

  private pagPlayerWasm;

  public constructor(pagPlayerWasm) {
    this.pagPlayerWasm = pagPlayerWasm;
  }
  /**
   * Set the progress of play position, the value is from 0.0 to 1.0.
   */
  public async setProgress(progress: number) {
    return await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._setProgress, this.pagPlayerWasm, progress);
  }
  /**
   * Apply all pending changes to the target surface immediately. Returns true if the content has
   * changed.
   */
  public async flush(): Promise<boolean> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._flush, this.pagPlayerWasm)) as boolean;
  }
  /**
   * Set the progress of play position, And Apply all pending changes to the target surface immediately.
   */
  public async setProgressAndFlush(progress: number) {
    return await PAGPlayer.module.webAssemblyQueue.exec(
      async (progress) => {
        this.pagPlayerWasm._setProgress(progress);
        return await this.pagPlayerWasm._flush();
      },
      this,
      progress,
    );
  }
  /**
   * The duration of current composition in microseconds.
   */
  public async duration(): Promise<number> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._duration, this.pagPlayerWasm)) as number;
  }
  /**
   * Returns the current progress of play position, the value is from 0.0 to 1.0.
   */
  public async getProgress(): Promise<number> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._getProgress,
      this.pagPlayerWasm,
    )) as number;
  }
  /**
   * If set to false, the player skips rendering for video composition.
   */
  public async videoEnabled(): Promise<boolean> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._videoEnabled,
      this.pagPlayerWasm,
    )) as boolean;
  }
  /**
   * Set the value of videoEnabled property.
   */
  public async setVideoEnabled(enabled: boolean) {
    return await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._setVideoEnabled,
      this.pagPlayerWasm,
      enabled,
    );
  }
  /**
   * If set to true, PAGPlayer caches an internal bitmap representation of the static content for each
   * layer. This caching can increase performance for layers that contain complex vector content. The
   * execution speed can be significantly faster depending on the complexity of the content, but it
   * requires extra graphics memory. The default value is true.
   */
  public async cacheEnabled(): Promise<boolean> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._cacheEnabled,
      this.pagPlayerWasm,
    )) as boolean;
  }
  /**
   * Set the value of cacheEnabled property.
   */
  public async setCacheEnabled(enabled: boolean) {
    return await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._setCacheEnabled, this.pagPlayerWasm, enabled);
  }
  /**
   * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
   * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of graphics
   * memory which leads to better performance. The default value is 1.0.
   */
  public async cacheScale(): Promise<number> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._cacheScale, this.pagPlayerWasm)) as number;
  }
  /**
   * Set the value of cacheScale property.
   */
  public async setCacheScale(value: number) {
    return await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._setCacheScale, this.pagPlayerWasm, value);
  }
  /**
   * The maximum frame rate for rendering, ranges from 1 to 60. If set to a value less than the actual
   * frame rate from composition, it drops frames but increases performance. Otherwise, it has no
   * effect. The default value is 60.
   */
  public async maxFrameRate(): Promise<number> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._maxFrameRate,
      this.pagPlayerWasm,
    )) as number;
  }
  /**
   * Set the maximum frame rate for rendering.
   */
  public async setMaxFrameRate(value: number) {
    return await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._setMaxFrameRate, this.pagPlayerWasm, value);
  }
  /**
   * Returns the current scale mode.
   */
  public async scaleMode(): Promise<PAGScaleMode> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._scaleMode,
      this.pagPlayerWasm,
    )) as PAGScaleMode;
  }
  /**
   * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
   * changes when this method is called.
   */
  public async setScaleMode(value: PAGScaleMode) {
    return await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._setScaleMode, this.pagPlayerWasm, value);
  }
  /**
   * Free the cache created by the surface immediately. Can be called to reduce memory pressure.
   */
  public async freeCache() {
    return await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm._freeCache, this.pagPlayerWasm);
  }
  /**
   * Set the PAGSurface object for PAGPlayer to render onto.
   */
  public async setSurface(pagSurface: PAGSurface) {
    return await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._setSurface,
      this.pagPlayerWasm,
      pagSurface.pagSurfaceWasm,
    );
  }
  /**
   * Returns the current PAGComposition for PAGPlayer to render as content.
   */
  public async getComposition() {
    const wasmIns = await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._getComposition,
      this.pagPlayerWasm,
    );
    return new PAGFile(wasmIns);
  }
  /**
   * Sets a new PAGComposition for PAGPlayer to render as content.
   */
  public async setComposition(pagFile: PAGFile) {
    return await PAGPlayer.module.webAssemblyQueue.exec(
      this.pagPlayerWasm._setComposition,
      this.pagPlayerWasm,
      pagFile.wasmIns,
    );
  }
  public async destroy() {
    await PAGPlayer.module.webAssemblyQueue.exec(this.pagPlayerWasm.delete, this.pagPlayerWasm);
  }
}
