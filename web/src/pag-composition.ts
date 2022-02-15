import { PAG, Marker } from './types';
import { PAGLayer } from './pag-layer';
import { wasmAwaitRewind } from './utils/decorators';

@wasmAwaitRewind
export class PAGComposition extends PAGLayer {
  public static module: PAG;

  public constructor(wasmIns) {
    super(wasmIns);
  }
  /**
   * Returns the width of the Composition.
   */
  public width(): number {
    return this.wasmIns._width() as number;
  }
  /**
   * Returns the height of the Composition.
   */
  public height(): number {
    return this.wasmIns._height() as number;
  }
  /**
   * Returns the number of child layers of this composition.
   */
  public numChildren(): number {
    return this.wasmIns._numChildren() as number;
  }
  /**
   * Set the width and height of the Composition.
   */
  public setContentSize(width: number, height: number): void {
    this.wasmIns._setContentSize(width, height);
  }
  /**
   * Returns the child layer that exists at the specified index.
   * @param index The index position of the child layer.
   * @returns The child layer at the specified index position.
   */
  public getLayerAt(index: number): PAGLayer {
    return this.wasmIns._getLayerAt(index) as PAGLayer;
  }
  /**
   * Returns an array of layers that match the specified layer name.
   */
  public getLayersByName(layerName: string): Array<PAGLayer> {
    return this.wasmIns._getLayersByName(layerName) as Array<PAGLayer>;
  }
  /**
   * Returns the index position of a child layer.
   * @param pagLayer The layer instance to identify.
   * @returns The index position of the child layer to identify.
   */
  public getLayerIndex(index: number): PAGLayer {
    return this.wasmIns._getLayerIndex(index) as PAGLayer;
  }
  /**
   * Swap the layers at the specified index.
   */
  public swapLayer(pagLayer1: PAGLayer, pagLayer2: PAGLayer): void {
    this.wasmIns._swapLayer(pagLayer1, pagLayer2);
  }
  /**
   * Swap the layers at the specified index.
   */
  public swapLayerAt(index1: number, index2: number): void {
    this.wasmIns._swapLayerAt(index1, index2);
  }
  /**
   * Check whether current PAGComposition contains the specified pagLayer.
   */
  public contains(pagLayer: PAGLayer): boolean {
    return this.wasmIns._contains(pagLayer);
  }
  /**
   * Add a PAGLayer to current PAGComposition at the top. If you add a layer that already has a
   * different PAGComposition object as a parent, the layer is removed from the other PAGComposition
   * object.
   */
  public addLayer(layer: PAGLayer): boolean {
    return this.wasmIns._addLayer(layer) as boolean;
  }
  /**
   * Add a PAGLayer to current PAGComposition at the top. If you add a layer that already has a
   * different PAGComposition object as a parent, the layer is removed from the other PAGComposition
   * object.
   */
  public addLayerAt(layer: PAGLayer, index: number): boolean {
    return this.wasmIns._addLayerAt(layer, index) as boolean;
  }
  /**
  * Indicates when the first frame of the audio plays in the composition's timeline.
  */
  public audioStartTime(): number {
    return this.wasmIns._audioStartTime() as number;
  }
  /**
   * Returns the audio markers of this composition.
   */
  public audioMarkers(): Array<Marker> {
    return this.wasmIns._audioMarkers() as Array<Marker>;
  }
  /**
    * The audio data of this composition.
    */
  public audioBytes(): Uint8Array {
    return this.wasmIns._audioBytes() as Uint8Array;
  }
  /**
   * Remove the specified PAGLayer from current PAGComposition.
   */
  public removeLayerAt(index: number): PAGLayer {
    return this.wasmIns._removeLayerAt(index) as PAGLayer;
  }
  /**
   * Remove all PAGLayers from current PAGComposition.
   */
  public removeAllLayers(): void {
    this.wasmIns._removeAllLayers();
  }
}
