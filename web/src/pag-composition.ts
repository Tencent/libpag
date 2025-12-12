import {PAGModule} from './pag-module';
import {PAGLayer} from './pag-layer';
import {destroyVerify} from './utils/decorators';
import {layer2typeLayer, proxyVector} from './utils/type-utils';

import {type Marker, VecArray} from './types';

@destroyVerify
export class PAGComposition extends PAGLayer {
    /**
     * Make a empty PAGComposition with specified size.
     */
    public static make(width: number, height: number): PAGComposition {
        const wasmIns = PAGModule._PAGComposition._Make(width, height);
        if (!wasmIns) throw new Error('Make PAGComposition fail!');
        return new PAGComposition(wasmIns);
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
     * Set the width and height of the Composition.
     */
    public setContentSize(width: number, height: number): void {
        this.wasmIns._setContentSize(width, height);
    }

    /**
     * Returns the number of child layers of this composition.
     */
    public numChildren(): number {
        return this.wasmIns._numChildren() as number;
    }

    /**
     * Returns the child layer that exists at the specified index.
     * @param index The index position of the child layer.
     * @returns The child layer at the specified index position.
     */
    public getLayerAt(index: number) {
        const wasmIns = this.wasmIns._getLayerAt(index);
        if (!wasmIns) throw new Error(`Get layer at ${index} fail!`);
        return layer2typeLayer(wasmIns);
    }

    /**
     * Returns the index position of a child layer.
     * @param pagLayer The layer instance to identify.
     * @returns The index position of the child layer to identify.
     */
    public getLayerIndex(pagLayer: PAGLayer): number {
        return this.wasmIns._getLayerIndex(pagLayer.wasmIns) as number;
    }

    /**
     * Changes the position of an existing child layer in the composition. This affects the layering
     * of child layers.
     * @param pagLayer The child layer for which you want to change the index number.
     * @param index The resulting index number for the child layer.
     */
    public setLayerIndex(pagLayer: PAGLayer, index: number): number {
        return this.wasmIns._setLayerIndex(pagLayer.wasmIns, index) as number;
    }

    /**
     * Add a PAGLayer to current PAGComposition at the top. If you add a layer that already has a
     * different PAGComposition object as a parent, the layer is removed from the other PAGComposition
     * object.
     */
    public addLayer(pagLayer: PAGLayer): boolean {
        return this.wasmIns._addLayer(pagLayer.wasmIns) as boolean;
    }

    /**
     * Add a PAGLayer to current PAGComposition at the top. If you add a layer that already has a
     * different PAGComposition object as a parent, the layer is removed from the other PAGComposition
     * object.
     */
    public addLayerAt(pagLayer: PAGLayer, index: number): boolean {
        return this.wasmIns._addLayerAt(pagLayer.wasmIns, index) as boolean;
    }

    /**
     * Check whether current PAGComposition contains the specified pagLayer.
     */
    public contains(pagLayer: PAGLayer): boolean {
        return this.wasmIns._contains(pagLayer.wasmIns) as boolean;
    }

    /**
     * Remove the specified PAGLayer from current PAGComposition.
     */
    public removeLayer(pagLayer: PAGLayer) {
        const wasmIns = this.wasmIns._removeLayer(pagLayer.wasmIns);
        if (!wasmIns) throw new Error('Remove layer fail!');
        return layer2typeLayer(wasmIns);
    }

    /**
     * Remove the specified PAGLayer from current PAGComposition.
     */
    public removeLayerAt(index: number) {
        const wasmIns = this.wasmIns._removeLayerAt(index);
        if (!wasmIns) throw new Error(`Remove layer at ${index} fail!`);
        return layer2typeLayer(wasmIns);
    }

    /**
     * Remove all PAGLayers from current PAGComposition.
     */
    public removeAllLayers(): void {
        this.wasmIns._removeAllLayers();
    }

    /**
     * Swap the layers at the specified index.
     */
    public swapLayer(pagLayer1: PAGLayer, pagLayer2: PAGLayer): void {
        this.wasmIns._swapLayer(pagLayer1.wasmIns, pagLayer2.wasmIns);
    }

    /**
     * Swap the layers at the specified index.
     */
    public swapLayerAt(index1: number, index2: number): void {
        this.wasmIns._swapLayerAt(index1, index2);
    }

    /**
     * The audio data of this composition, which is an AAC audio in an MPEG-4 container.
     */
    public audioBytes(): Uint8Array | null {
        return this.wasmIns._audioBytes();
    }

    /**
     * Returns the audio markers of this composition.
     */
    public audioMarkers() {
        const wasmIns = this.wasmIns._audioMarkers();
        if (!wasmIns) throw new Error(`Get audioMarkers fail!`);
        return proxyVector(wasmIns, (wasmIns: any) => wasmIns as Marker);
    }

    /**
     * Indicates when the first frame of the audio plays in the composition's timeline.
     */
    public audioStartTime(): number {
        return this.wasmIns._audioStartTime() as number;
    }

    /**
     * Returns an array of layers that match the specified layer name.
     */
    public getLayersByName(layerName: string) {
        const wasmIns = this.wasmIns._getLayersByName(layerName);
        if (!wasmIns) throw new Error(`Get layers by ${layerName} fail!`);
        const layerArray = VecArray.create();
        for (const wasmIn of wasmIns) {
            layerArray.push(layer2typeLayer(wasmIn));
        }
        return layerArray;
    }

    /**
     * Returns an array of layers that lie under the specified point. The point is in pixels and from
     * this PAGComposition's local coordinates.
     */
    public getLayersUnderPoint(localX: number, localY: number) {
        const wasmIns = this.wasmIns._getLayersUnderPoint(localX, localY);
        if (!wasmIns) throw new Error(`Get layers under point ${localX},${localY} fail!`);
        const layerArray = VecArray.create();
        for (const wasmIn of wasmIns) {
            layerArray.push(layer2typeLayer(wasmIn));
        }
        return layerArray;
    }
}
