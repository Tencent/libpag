import { LayerType, PAG } from './types';
import { wasmAwaitRewind } from './utils/decorators';

@wasmAwaitRewind
export class PAGLayer {
  public static module: PAG;

  public wasmIns: any;

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
  public destroy(): void {
    this.wasmIns.delete();
  }
}
