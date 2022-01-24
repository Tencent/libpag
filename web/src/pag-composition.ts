import { PAG } from './types';
import { PAGLayer } from './pag-layer';

export class PAGComposition extends PAGLayer {
  public static module: PAG;

  public constructor(wasmIns) {
    super(wasmIns);
  }

  public async width(): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._width, this.wasmIns)) as number;
  }
  public async height(): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._height, this.wasmIns)) as number;
  }
  /**
   * Returns the number of child layers of this composition.
   */
  public async numChildren(): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._numChildren, this.wasmIns)) as number;
  }
  /**
   * Set the width and height of the Composition.
   */
  public async setContentSize(width: number, height: number): Promise<void> {
    await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._setContentSize, this.wasmIns, width, height);
  }
  /**
   * Returns the child layer that exists at the specified index.
   * @param index The index position of the child layer.
   * @returns The child layer at the specified index position.
   */
  public async getLayerAt(index: number): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._getLayerAt, this.wasmIns, index)) as number;
  }
  /**
   * Returns the index position of a child layer.
   * @param pagLayer The layer instance to identify.
   * @returns The index position of the child layer to identify.
   */
  public async getLayerIndex(index: number): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._getLayerIndex, this.wasmIns, index)) as number;
  }

  /**
   * Swap the layers at the specified index.
   */
  public async swapLayer(pagLayer1: number, pagLayer2: number): Promise<void> {
    await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._swapLayer, this.wasmIns, pagLayer1, pagLayer2);
  }
  /**
   * Swap the layers at the specified index.
   */
  public async swapLayerAt(index1: number, index2: number): Promise<void> {
    await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._swapLayerAt, this.wasmIns, index1, index2);
  }
  /**
   * Remove the specified PAGLayer from current PAGComposition.
   */
  public async removeLayerAt(index: number): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._removeLayerAt, this.wasmIns, index)) as number;
  }
  /**
   * Remove all PAGLayers from current PAGComposition.
   */
  public async removeAllLayers(): Promise<void> {
    await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._removeAllLayers, this.wasmIns);
  }
  /**
   * Indicates when the first frame of the audio plays in the composition's timeline.
   */
  public async audioStartTime(): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._audioStartTime, this.wasmIns)) as number;
  }
  /**
   * Add a PAGLayer to current PAGComposition at the top. If you add a layer that already has a
   * different PAGComposition object as a parent, the layer is removed from the other PAGComposition
   * object.
   */
  public async addLayer(layer: any): Promise<boolean> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._addLayer, this.wasmIns, layer)) as boolean;
  }
  /**
   * Add a PAGLayer to current PAGComposition at the top. If you add a layer that already has a
   * different PAGComposition object as a parent, the layer is removed from the other PAGComposition
   * object.
   */
  public async addLayerAt(layer: any, index: number): Promise<boolean> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._addLayerAt, this.wasmIns, layer, index)) as boolean;
  }
  /**
   * The audio data of this composition.
   */
  public async audioBytes(): Promise<any> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._audioBytes, this.wasmIns)) as any;
  }
}
