import { PAGFile } from './pag-file';
import { PAGSurface } from './pag-surface';
import { PAG, PAGScaleMode } from './types';
import { wasmAwaitRewind, wasmAsyncMethod } from './utils/decorators';

@wasmAwaitRewind
export class PAGPlayer {
  public static module: PAG;

  public static create(): PAGPlayer {
    const wasmIns = new PAGPlayer.module._PAGPlayer();
    return new PAGPlayer(wasmIns);
  }

  public wasmIns;

  public constructor(wasmIns) {
    this.wasmIns = wasmIns;
  }
  /**
   * Set the progress of play position, the value is from 0.0 to 1.0.
   */
  public setProgress(progress: number): void {
    this.wasmIns._setProgress(progress);
  }
  /**
   * Apply all pending changes to the target surface immediately. Returns true if the content has
   * changed.
   */
  @wasmAsyncMethod
  public async flush(): Promise<boolean> {
    return (await PAGPlayer.module.webAssemblyQueue.exec(this.wasmIns._flush, this.wasmIns)) as boolean;
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
   * If set to false, the player skips rendering for video composition.
   */
  public videoEnabled(): boolean {
    return this.wasmIns._videoEnabled() as boolean;
  }
  /**
   * Set the value of videoEnabled property.
   */
  public setVideoEnabled(enabled: boolean): void {
    this.wasmIns._setVideoEnabled(enabled);
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
  public setSurface(pagSurface: PAGSurface): void {
    this.wasmIns._setSurface(pagSurface.wasmIns);
  }
  /**
   * Returns the current PAGComposition for PAGPlayer to render as content.
   */
  public getComposition(): PAGFile {
    const wasmIns = this.wasmIns._getComposition();
    return new PAGFile(wasmIns);
  }
  /**
   * Sets a new PAGComposition for PAGPlayer to render as content.
   */
  public setComposition(pagFile: PAGFile) {
    this.wasmIns._setComposition(pagFile.wasmIns);
  }

  public destroy() {
    this.wasmIns.delete();
  }
}
