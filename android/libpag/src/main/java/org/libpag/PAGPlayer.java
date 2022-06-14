package org.libpag;

import android.graphics.Matrix;
import android.graphics.RectF;

import org.extra.tools.LibraryLoadUtils;
import org.ffavc.DecoderFactory;

public class PAGPlayer {
    private PAGSurface pagSurface = null;

    public PAGPlayer() {
        nativeSetup();
    }

    /**
     * Returns the current PAGComposition for PAGPlayer to render as content.
     */
    public native PAGComposition getComposition();

    /**
     * Sets a new PAGComposition for PAGPlayer to render as content.
     * Note: If the composition is already added to another PAGPlayer, it will be removed from the
     * previous PAGPlayer.
     */
    public native void setComposition(PAGComposition newComposition);

    /**
     * Returns the PAGSurface object for PAGPlayer to render onto.
     */
    public PAGSurface getSurface() {
        return pagSurface;
    }

    /**
     * Set the PAGSurface object for PAGPlayer to render onto.
     */
    public void setSurface(PAGSurface surface) {
        this.pagSurface = surface;
        if (surface == null) {
            nativeSetSurface(0);
        } else {
            nativeSetSurface(surface.nativeSurface);
        }
    }

    private native void nativeSetSurface(long surface);

    /**
     * If set to false, the player skips rendering for video composition.
     */
    public native boolean videoEnabled();

    /**
     * Set the value of videoEnabled property.
     */
    public native void setVideoEnabled(boolean value);

    /**
     * If set to true, PAGPlayer caches an internal bitmap representation of the static content for
     * each layer. This caching can increase performance for layers that contain complex vector
     * content. The execution speed can be significantly faster depending on the complexity of the
     * content, but it requires extra graphics memory. The default value is true.
     */
    public native boolean cacheEnabled();

    /**
     * Set the value of cacheEnabled property.
     */
    public native void setCacheEnabled(boolean value);

    /**
     * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
     * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of
     * graphics memory which leads to better performance. The default value is 1.0.
     */
    public native float cacheScale();

    /**
     * Set the value of cacheScale property.
     */
    public native void setCacheScale(float value);

    /**
     * The maximum frame rate for rendering, ranges from 1 to 60. If set to a value less than the
     * actual frame rate from composition, it drops frames but increases performance. Otherwise, it
     * has no effect. The default value is 60.
     */
    public native float maxFrameRate();

    /**
     * Set the maximum frame rate for rendering.
     */
    public native void setMaxFrameRate(float value);

    /**
     * Returns the current scale mode.
     */
    public native int scaleMode();

    /**
     * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
     * changes when this method is called.
     */
    public native void setScaleMode(int mode);

    /**
     * Returns a copy of current matrix.
     */
    public Matrix matrix() {
        float[] values = new float[9];
        nativeGetMatrix(values);
        Matrix matrix = new Matrix();
        matrix.setValues(values);
        return matrix;
    }

    private native void nativeGetMatrix(float[] values);

    /**
     * Sets the transformation which will be applied to the composition. The scaleMode property
     * will be set to PAGScaleMode::None when this method is called.
     */
    public void setMatrix(Matrix matrix) {
        float[] values = new float[9];
        matrix.getValues(values);
        nativeSetMatrix(values[0], values[3], values[1], values[4], values[2], values[5]);
    }

    private native void nativeSetMatrix(float a, float b, float c,
                                        float d, float tx, float ty);

    /**
     * The duration of current composition in microseconds.
     */
    public native long duration();

    /**
     * Returns the current progress of play position, the value is from 0.0 to 1.0.
     */
    public native double getProgress();

    /**
     * Sets the progress of play position, the value ranges from 0.0 to 1.0. It is applied only when
     * the composition is not null.
     */
    public native void setProgress(double value);

    /**
     * Prepares the player for the next flush() call. It collects all CPU tasks from the current
     * progress of the composition and runs them asynchronously in parallel. It is usually used for
     * speeding up the first frame rendering.
     */
    public native void prepare();

    /**
     * Apply all pending changes to the target surface immediately. Returns true if the content has
     * changed.
     */
    public boolean flush() {
        return flushAndFenceSync(null);
    }

    /**
     * Apply all pending changes to the target surface immediately. Returns true if the content has
     * changed.
     * If the sync array is not null, a new sync object will be created and set at the index 0.
     * After issuing all commands, the sync object will be signaled by the GPU.
     * The caller must delete the sync object returned by this method.
     * If the sync object is 0, the OpenGL did not create or add a sync object to signal on the GPU,
     * the caller should not instruct the GPU to wait on the sync object.
     */
    public native boolean flushAndFenceSync(long[] sync);

    /**
     * Inserts a sync object that the OpenGL API must wait on before executing any more commands
     * on the GPU for this surface. PAG will take ownership of the sync object and delete it once it
     * has been signaled and waited on. If this call returns false, then the GPU will not wait on
     * the passed sync object, and the client will still own the sync object.
     * Usually called before {@link #flush()} and @{@link #flushAndFenceSync(long[])}
     *
     * @returns true if GPU is waiting on sync object
     */
    public native boolean waitSync(long sync);

    /**
     * Returns a rectangle that defines the displaying area of the specified layer, which is in the
     * coordinate of the PAGSurface.
     */
    public native RectF getBounds(PAGLayer pagLayer);

    /**
     * Returns an array of layers that lie under the specified point. The point is in pixels and
     * from the surface's coordinates.
     */
    public native PAGLayer[] getLayersUnderPoint(float surfaceX, float surfaceY);

    /**
     * Evaluates the PAGLayer to see if it overlaps or intersects with the specified point. The point
     * is in the coordinate space of the PAGSurface, not the PAGComposition that contains the
     * PAGLayer. It always returns false if the PAGLayer or its parent (or parent's parent...) has not
     * been added to this PAGPlayer. The pixelHitTest parameter indicates whether or not to check
     * against the actual pixels of the object (true) or the bounding box (false). Returns true if the
     * PAGLayer overlaps or intersects with the specified point.
     */
    public native boolean hitTestPoint(PAGLayer pagLayer, float surfaceX,
                                       float surfaceY, boolean pixelHitTest);

    /**
     * Free up resources used by the PAGPlayer instance immediately instead of relying on the
     * garbage collector to do this for you at some point in the future.
     */
    public void release() {
        nativeRelease();
    }

    private native final void nativeRelease();

    protected void finalize() {
        nativeFinalize();
    }

    private native void nativeFinalize();

    private native final void nativeSetup();

    private static native final void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
        VideoDecoder.RegisterSoftwareDecoderFactory(DecoderFactory.GetHandle());
    }

    private long nativeContext = 0;
}
