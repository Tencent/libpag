import { LayerType, PAG } from './types';

export class PAGLayer {
  public static module: PAG;

  private pagLayerWasm;

  public constructor(pagLayerWasm) {
    this.pagLayerWasm = pagLayerWasm;
  }
  /**
   * Returns a globally unique id for this object.
   */
  public async uniqueID(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._uniqueID, this.pagLayerWasm)) as number;
  }
  /**
   * Returns the type of layer.
   */
  public async layerType(): Promise<LayerType> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._layerType, this.pagLayerWasm)) as LayerType;
  }
  /**
   * Returns the name of the layer.
   */
  public async layerName(): Promise<string> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._layerName, this.pagLayerWasm)) as string;
  }
  /**
   * Returns the opacity of the layer.
   */
  public async opacity(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._opacity, this.pagLayerWasm)) as number;
  }
  /**
   * Set the opacity of the layer. it will be concatenated to current animation opacity for
   * displaying.
   */
  public async setOpacity(opacity: Number): Promise<void> {
    await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._opacity, this.pagLayerWasm, opacity);
  }
  /**
   * Whether or not the layer is visible.
   */
  public async visible(): Promise<boolean> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._visible, this.pagLayerWasm)) as boolean;
  }
  /**
   * Set the visible of the layer.
   */
  public async setVisible(visible: boolean): Promise<void> {
    await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._setVisible, this.pagLayerWasm, visible);
  }
  /**
   * Ranges from 0 to PAGFile.numTexts - 1 if the layer type is text, or from 0 to PAGFile.numImages
   * -1 if the layer type is image, otherwise returns -1.
   */
  public async editableIndex(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._editableIndex, this.pagLayerWasm)) as number;
  }
  /**
   * The duration of the layer in microseconds, indicates the length of the visible range.
   */
  public async duration(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._duration, this.pagLayerWasm)) as number;
  }
  /**
   * Returns the frame rate of this layer.
   */
  public async frameRate(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._frameRate, this.pagLayerWasm)) as number;
  }
  /**
   * The start time of the layer in microseconds, indicates the start position of the visible range
   * in parent composition. It could be negative value.
   */
  public async startTime(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._startTime, this.pagLayerWasm)) as number;
  }
  /**
   * Converts the time from the PAGLayer's (local) timeline to the PAGSurface (global) timeline. The
   * time is in microseconds.
   */
  public async localTimeToGlobal(localTime: number): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(
      this.pagLayerWasm._localTimeToGlobal,
      this.pagLayerWasm,
      localTime,
    )) as number;
  }
  /**
   * Converts the time from the PAGSurface (global) to the PAGLayer's (local) timeline timeline. The
   * time is in microseconds.
   */
  public async globalToLocalTime(globalTime: number): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(
      this.pagLayerWasm._globalToLocalTime,
      this.pagLayerWasm,
      globalTime,
    )) as number;
  }
  public async destroy() {
    await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm.delete, this.pagLayerWasm);
  }
}
