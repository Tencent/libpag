import { LayerType, PAG } from './types';

export class PAGLayer {
  public static module: PAG;

  public wasmIns;

  public constructor(wasmIns) {
    this.wasmIns = wasmIns;
  }
  /**
   * Returns a globally unique id for this object.
   */
  public async uniqueID(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._uniqueID, this.wasmIns)) as number;
  }
  /**
   * Returns the type of layer.
   */
  public async layerType(): Promise<LayerType> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._layerType, this.wasmIns)) as LayerType;
  }
  /**
   * Returns the name of the layer.
   */
  public async layerName(): Promise<string> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._layerName, this.wasmIns)) as string;
  }
  /**
   * Returns the opacity of the layer.
   */
  public async opacity(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._opacity, this.wasmIns)) as number;
  }
  /**
   * Set the opacity of the layer. it will be concatenated to current animation opacity for
   * displaying.
   */
  public async setOpacity(opacity: Number): Promise<void> {
    await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._opacity, this.wasmIns, opacity);
  }
  /**
   * Whether or not the layer is visible.
   */
  public async visible(): Promise<boolean> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._visible, this.wasmIns)) as boolean;
  }
  /**
   * Set the visible of the layer.
   */
  public async setVisible(visible: boolean): Promise<void> {
    await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._setVisible, this.wasmIns, visible);
  }
  /**
   * Ranges from 0 to PAGFile.numTexts - 1 if the layer type is text, or from 0 to PAGFile.numImages
   * -1 if the layer type is image, otherwise returns -1.
   */
  public async editableIndex(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._editableIndex, this.wasmIns)) as number;
  }
  /**
   * The duration of the layer in microseconds, indicates the length of the visible range.
   */
  public async duration(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._duration, this.wasmIns)) as number;
  }
  /**
   * Returns the frame rate of this layer.
   */
  public async frameRate(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._frameRate, this.wasmIns)) as number;
  }
  /**
   * The start time of the layer in microseconds, indicates the start position of the visible range
   * in parent composition. It could be negative value.
   */
  public async startTime(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns._startTime, this.wasmIns)) as number;
  }
  /**
   * Converts the time from the PAGLayer's (local) timeline to the PAGSurface (global) timeline. The
   * time is in microseconds.
   */
  public async localTimeToGlobal(localTime: number): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(
      this.wasmIns._localTimeToGlobal,
      this.wasmIns,
      localTime,
    )) as number;
  }
  /**
   * Converts the time from the PAGSurface (global) to the PAGLayer's (local) timeline timeline. The
   * time is in microseconds.
   */
  public async globalToLocalTime(globalTime: number): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(
      this.wasmIns._globalToLocalTime,
      this.wasmIns,
      globalTime,
    )) as number;
  }
  public async destroy() {
    await PAGLayer.module.webAssemblyQueue.exec(this.wasmIns.delete, this.wasmIns);
  }
}
