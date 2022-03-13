import { PAGComposition } from './pag-composition';
import { PAGFile } from './pag-file';
import { PAGLayer } from './pag-layer';
import { PAGSurface } from './pag-surface';
import { Matrix, PAG, PAGScaleMode, Rect, Vector } from './types';
import { wasmAwaitRewind, wasmAsyncMethod } from './utils/decorators';
import { vectorClass2array } from './utils/type-utils';

@wasmAwaitRewind
export class PAGPlayer {
  public static module: PAG;

  public static create(): PAGPlayer {
    const wasmIns = new PAGPlayer.module._PAGPlayer();
    return new PAGPlayer(wasmIns);
  }

  public wasmIns;

  public constructor(wasmIns: any) {
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
    return new PAGFile(this.wasmIns._getComposition());
  }
  /**
   * Sets a new PAGComposition for PAGPlayer to render as content.
   */
  public setComposition(pagComposition: PAGComposition) {
    this.wasmIns._setComposition(pagComposition.wasmIns);
  }
  /**
   * Returns the PAGSurface object for PAGPlayer to render onto.
   */
  public getSurface(): PAGSurface {
    return new PAGSurface(this.wasmIns._getSurface());
  }
  /**
   * Returns a copy of current matrix.
   */
  public matrix(): Matrix {
    return this.wasmIns._matrix() as Matrix;
  }
  /**
   * Set the transformation which will be applied to the composition. The scaleMode property
   * will be set to PAGScaleMode::None when this method is called.
   */
  public setMatrix(matrix: Matrix) {
    this.wasmIns._setMatrix(matrix);
  }
  /**
   * Set the progress of play position to next frame. It is applied only when the composition is not
   * null.
   */
  public nextFrame() {
    this.wasmIns._nextFrame();
  }
  /**
   * Set the progress of play position to previous frame. It is applied only when the composition is
   * not null.
   */
  public preFrame() {
    this.wasmIns._preFrame();
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
   * this PAGComposition's local coordinates.
   */
  public getLayersUnderPoint(): PAGLayer[] {
    return vectorClass2array(this.wasmIns._getLayersUnderPoint() as Vector<any>, PAGLayer);
  }
  /**
   * Evaluates the PAGLayer to see if it overlaps or intersects with the specified point. The point
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

  public destroy() {
    this.wasmIns.delete();
  }
}
