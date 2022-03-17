package org.libpag;

import org.extra.tools.LibraryLoadUtils;

import java.nio.ByteBuffer;

public class PAGComposition extends PAGLayer {

    /**
     * Make a empty PAGComposition with specified size.
     */
    public static native PAGComposition Make(int width, int height);

    public PAGComposition(long nativeContext) {
        super(nativeContext);
    }

    /**
     * The width of Composition.
     */
    public native int width();

    /**
     * Returns the height of Composition.
     */
    public native int height();

    /**
     * Set the size of the Composition.
     */
    public native void setContentSize(int width, int height);

    /**
     * Returns the number of child layers of this composition.
     */
    public native int numChildren();

    /**
     * Returns the child layer that exists at the specified index.
     * @param index The index position of the child layer.
     * @return The child layer at the specified index position.
     */
    public native PAGLayer getLayerAt(int index);

    /**
     * Returns the index position of a child layer.
     * @param layer The layer instance to identify.
     * @return The index position of the child layer to identify.
     */
    public native int getLayerIndex(PAGLayer layer);

    /**
     * Changes the position of an existing child in the container. This affects the layering of child layers.
     * @param layer The child layer for which you want to change the index number.
     * @param index The resulting index number for the child layer.
     */
    public native void setLayerIndex(PAGLayer layer, int index);

    /**
     * Add PAGLayer to current PAGComposition at the top.
     */
    public native void addLayer(PAGLayer pagLayer);

    /**
     * Add PAGLayer to current PAGComposition at the specified index.
     */
    public native void addLayerAt(PAGLayer pagLayer, int index);

    /**
     * Check whether current PAGComposition contains the specified pagLayer.
     */
    public native boolean contains(PAGLayer pagLayer);

    /**
     * Remove the specified PAGLayer from current PAGComposition.
     */
    public native PAGLayer removeLayer(PAGLayer pagLayer);

    /**
     * Remove the PAGLayer at specified index from current PAGComposition.
     */
    public native PAGLayer removeLayerAt(int index);

    /**
     * Remove all PAGLayers from current PAGComposition.
     */
    public native void removeAllLayers();

    /**
     * Swap the layers.
     */
    public native void swapLayer(PAGLayer pagLayer1, PAGLayer pagLayer2);

    /**
     * Swap the layers at the specified index.
     */
    public native void swapLayerAt(int index1, int index2);

    /**
     * The audio data of this composition, which is an AAC audio in an MPEG-4 container.
     */
    public native ByteBuffer audioBytes();

    /**
     * Indicates when the first frame of the audio plays in the composition's timeline.
     */
    public native long audioStartTime();

    /**
     * Returns the audio markers of this composition.
     */
    public native PAGMarker[] audioMarkers();

    /**
     * Returns an array of layers that match the specified layer name.
     */
    public native PAGLayer[] getLayersByName(String layerName);

    /**
     * Returns an array of layers that lie under the specified point. The point is in pixels and from this
     * PAGComposition's local coordinates.
     */
    public native PAGLayer[] getLayersUnderPoint(float localX, float localY);

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
