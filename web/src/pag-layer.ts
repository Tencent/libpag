import { PAGComposition } from './pag-composition';
import { destroyVerify, wasmAwaitRewind } from './utils/decorators';
import { Matrix } from './core/matrix';
import { layer2typeLayer, proxyVector } from './utils/type-utils';

import type { LayerType, Marker, Rect } from './types';

@destroyVerify
@wasmAwaitRewind
export class PAGLayer {
  public wasmIns: any;
  public isDestroyed: any;

  public constructor(wasmIns: any) {
    this.wasmIns = wasmIns;
  }
  /**
   * Returns a globally unique id for this object.
   */
  public uniqueID(): number {
    return this.wasmIns._uniqueID() as number;
  }
  /**
   * Returns the type of layer.
   */
  public layerType(): LayerType {
    return this.wasmIns._layerType() as LayerType;
  }
  /**
   * Returns the name of the layer.
   */
  public layerName(): string {
    return this.wasmIns._layerName() as string;
  }
  /**
   * A matrix object containing values that alter the scaling, rotation, and translation of the
   * layer. Altering it does not change the animation matrix, and it will be concatenated to current
   * animation matrix for displaying.
   */
  public matrix(): Matrix {
    const wasmIns = this.wasmIns._matrix();
    if (!wasmIns) throw new Error('Get matrix fail!');
    return new Matrix(wasmIns);
  }

  public setMatrix(matrix: Matrix) {
    this.wasmIns._setMatrix(matrix.wasmIns);
  }
  /**
   * Resets the matrix to its default value.
   */
  public resetMatrix() {
    this.wasmIns._resetMatrix();
  }
  /**
   * The final matrix for displaying, it is the combination of the matrix property and current
   * matrix from animation.
   */
  public getTotalMatrix(): Matrix {
    const wasmIns = this.wasmIns._getTotalMatrix();
    if (!wasmIns) throw new Error('Get total matrix fail!');
    return new Matrix(this.wasmIns._getTotalMatrix());
  }
  /**
   * Returns the current alpha of the layer if previously set.
   */
  public alpha(): number {
    return this.wasmIns._alpha() as number;
  }
  /**
   * Set the alpha of the layer, which will be concatenated to the current animation opacity for
   * displaying.
   */
  public setAlpha(opacity: Number): void {
    this.wasmIns._setAlpha(opacity);
  }
  /**
   * Whether or not the layer is visible.
   */

  public visible(): boolean {
    return this.wasmIns._visible() as boolean;
  }
  /**
   * Set the visible of the layer.
   */
  public setVisible(visible: boolean): void {
    this.wasmIns._setVisible(visible);
  }
  /**
   * Ranges from 0 to PAGFile.numTexts - 1 if the layer type is text, or from 0 to PAGFile.numImages
   * -1 if the layer type is image, otherwise returns -1.
   */
  public editableIndex(): number {
    return this.wasmIns._editableIndex() as number;
  }
  /**
   * Returns the parent PAGComposition of current PAGLayer.
   */
  public parent(): PAGComposition {
    const wasmIns = this.wasmIns._parent();
    if (!wasmIns) throw new Error('Get total matrix fail!');
    return new PAGComposition(wasmIns);
  }
  /**
   * Returns the markers of this layer.
   */
  public markers() {
    const wasmIns = this.wasmIns._markers();
    if (!wasmIns) throw new Error('Get markers fail!');
    return proxyVector(wasmIns, (wasmIns: any) => wasmIns as Marker);
  }
  /**
   * Converts the time from the PAGLayer's (local) timeline to the PAGSurface (global) timeline. The
   * time is in microseconds.
   */
  public localTimeToGlobal(localTime: number): number {
    return this.wasmIns._localTimeToGlobal(localTime) as number;
  }
  /**
   * Converts the time from the PAGSurface (global) to the PAGLayer's (local) timeline timeline. The
   * time is in microseconds.
   */
  public globalToLocalTime(globalTime: number): number {
    return this.wasmIns._globalToLocalTime(globalTime) as number;
  }
  /**
   * The duration of the layer in microseconds, indicates the length of the visible range.
   */
  public duration(): number {
    return this.wasmIns._duration() as number;
  }
  /**
   * Returns the frame rate of this layer.
   */
  public frameRate(): number {
    return this.wasmIns._frameRate() as number;
  }
  /**
   * The start time of the layer in microseconds, indicates the start position of the visible range
   * in parent composition. It could be negative value.
   */
  public startTime(): number {
    return this.wasmIns._startTime() as number;
  }
  /**
   * Set the start time of the layer, in microseconds.
   */
  public setStartTime(time: number) {
    this.wasmIns._setStartTime(time);
  }
  /**
   * The current time of the layer in microseconds, the layer is invisible if currentTime is not in
   * the visible range (startTime <= currentTime < startTime + duration).
   */
  public currentTime(): number {
    return this.wasmIns._currentTime() as number;
  }
  /**
   * Set the current time of the layer, in microseconds.
   */
  public setCurrentTime(time: number) {
    this.wasmIns._setCurrentTime(time);
  }
  /**
   * Returns the current progress of play position, the value ranges from 0.0 to 1.0.
   */
  public getProgress(): number {
    return this.wasmIns._getProgress() as number;
  }
  /**
   * Set the progress of play position, the value ranges from 0.0 to 1.0. A value of 0.0 represents
   * the frame at startTime. A value of 1.0 represents the frame at the end of duration.
   */
  public setProgress(percent: number) {
    this.wasmIns._setProgress(percent);
  }
  /**
   * Set the progress of play position to the previous frame.
   */
  public preFrame() {
    this.wasmIns._preFrame();
  }
  /**
   * Set the progress of play position to the next frame.
   */
  public nextFrame() {
    this.wasmIns._nextFrame();
  }
  /**
   * Returns a rectangle that defines the original area of the layer, which is not transformed by
   * the matrix.
   */
  public getBounds(): Rect {
    return this.wasmIns._getBounds() as Rect;
  }
  /**
   * Returns trackMatte layer of this layer.
   */
  public trackMatteLayer(): PAGLayer {
    const wasmIns = this.wasmIns._trackMatteLayer();
    if (!wasmIns) throw new Error('Get track matte layer fail!');
    return layer2typeLayer(wasmIns);
  }
  /**
   * Indicate whether this layer is excluded from parent's timeline. If set to true, this layer's
   * current time will not change when parent's current time changes.
   */
  public excludedFromTimeline(): boolean {
    return this.wasmIns._excludedFromTimeline() as boolean;
  }
  /**
   * Set the excludedFromTimeline flag of this layer.
   */
  public setExcludedFromTimeline(value: boolean): void {
    this.wasmIns._setExcludedFromTimeline(value);
  }
  /**
   * Returns true if this layer is a PAGFile.
   */
  public isPAGFile(): boolean {
    return this.wasmIns._isPAGFile() as boolean;
  }
  /**
   * Returns this layer as a type layer.
   */
  public asTypeLayer() {
    return layer2typeLayer(this);
  }

  public isDelete(): boolean {
    return this.wasmIns.isDelete();
  }

  public destroy(): void {
    this.wasmIns.delete();
    this.isDestroyed = true;
  }
}
