package org.libpag;

import android.graphics.Matrix;
import android.graphics.RectF;

import org.extra.tools.LibraryLoadUtils;

public class PAGLayer {
    public static final int LayerTypeUnknown = 0;
    public static final int LayerTypeNull = 1;
    public static final int LayerTypeSolid = 2;
    public static final int LayerTypeText = 3;
    public static final int LayerTypeShape = 4;
    public static final int LayerTypeImage = 5;
    public static final int LayerTypePreCompose = 6;

    public PAGLayer(long nativeContext) {
        this.nativeContext = nativeContext;
    }

    /**
     * Returns the type of layer.
     */
    public native int layerType();

    /**
     * Returns the name of the layer.
     */
    public native String layerName();

    /**
     * A matrix object containing values that alter the scaling, rotation, and translation of the layer.
     * Altering it does not change the animation matrix, and it will be concatenated to current animation matrix for
     * displaying.
     */
    public Matrix matrix() {
        float[] data = new float[9];
        matrix(data);

        Matrix matrix = new Matrix();
        matrix.setValues(data);
        return matrix;
    }

    /**
     * A matrix object containing values that alter the scaling, rotation, and translation of the layer.
     * Altering it does not change the animation matrix, and it will be concatenated to current animation matrix for
     * displaying.
     */
    public void setMatrix(Matrix matrix) {
        if (matrix == null)
            return;

        float[] data = new float[9];
        matrix.getValues(data);
        setMatrix(data);
    }

    /**
     * Resets the matrix to its default value.
     */
    public native void resetMatrix();

    /**
     * Returns the layer's display matrix by combining its matrix) property with the current animation
     * matrix from the AE timeline. This does not include the parent layer's matrix.
     * To calculate the final bounds relative to the PAGSurface, use the PAGPlayer::getBounds(PAGLayer layer)
     * method directly.
     */
    public Matrix getTotalMatrix() {
        float[] data = new float[9];
        getTotalMatrix(data);

        Matrix matrix = new Matrix();
        matrix.setValues(data);
        return matrix;
    }

    /**
     * Whether or not the layer is visible.
     */
    public native boolean visible();

    public native void setVisible(boolean value);

    /**
     * Ranges from 0 to PAGFile.numTexts - 1 if the layer type is text, or from 0 to PAGFile.numImages -1 if the
     * layer type is image, otherwise returns -1.
     */
    public native int editableIndex();

    /**
     * Indicates the Container instance that contains this Node instance.
     */
    public native PAGComposition parent();

    /**
     * Returns the markers of this layer.
     */
    public native PAGMarker[] markers();

    /**
     * Converts the time from the PAGLayer's (local) timeline to the PAGSurface (global) timeline. The time is in
     * microseconds.
     */
    public native long localTimeToGlobal(long localTime);

    /**
     * Converts the time from the PAGSurface (global) to the PAGLayer's (local) timeline timeline. The time is in
     * microseconds.
     */
    public native long globalToLocalTime(long globalTime);

    /**
     * The duration of the layer in microseconds, indicates the length of the visible range.
     */
    public native long duration();

    /**
     * Returns the frame rate of this layer.
     */
    public native float frameRate();

    /**
     * The start time of the layer in microseconds, indicates the start position of the visible range. It could be a negative value.
     */
    public native long startTime();

    /**
     * Set the start time of the layer, in microseconds.
     */
    public native void setStartTime(long time);

    /**
     * The current time of the layer in microseconds, the layer is invisible if currentTime is not in the visible range
     * {@code (startTime <= currentTime < startTime + duration)}.
     */
    public native long currentTime();

    /**
     * Set the current time of the layer in microseconds.
     */
    public native void setCurrentTime(long time);

    /**
     * Returns the current progress of play position, the value is from 0.0 to 1.0.
     */
    public native double getProgress();

    /**
     * Set the progress of play position, the value ranges from 0.0 to 1.0. A value of 0.0 represents the
     * frame at startTime. A value of 1.0 represents the frame at the end of duration.
     */
    public native void setProgress(double value);

    /**
     * Returns trackMatte layer of this layer.
     */
    public native PAGLayer trackMatteLayer();

    /**
     * Returns a rectangle in pixels that defines the original area of the layer, which is not
     * transformed by the matrix.
     */
    public native RectF getBounds();

    /**
     * Indicate whether this layer is excluded from parent's timeline. If set to true, this layer's current time
     * will not change when parent's current time changes.
     */
    public native boolean excludedFromTimeline();

    /**
     * Set the excludedFromTimeline flag of this layer.
     */
    public native void setExcludedFromTimeline(boolean value);

    /**
     * Returns the current alpha of the layer if previously set.
     */
    public native float alpha();

    /**
     * Set the alpha of the layer, which will be concatenated to the current animation opacity for
     * displaying.
     */
    public native void setAlpha(float value);

    private native void nativeRelease();

    protected long nativeContext;

    private static native void nativeInit();

    private native void matrix(float[] value);

    private native void setMatrix(float[] value);

    private native void getTotalMatrix(float[] value);

    private native boolean nativeEquals(PAGLayer other);

    @Override
    protected void finalize() {
        nativeRelease();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof PAGLayer)) {
            return false;
        }
        return nativeEquals((PAGLayer) obj);
    }

    @Override
    public int hashCode() {
        return 31 * 17 + (int) (nativeContext ^ (nativeContext >>> 32));
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
