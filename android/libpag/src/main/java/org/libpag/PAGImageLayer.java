package org.libpag;

import org.extra.tools.LibraryLoadUtils;

import java.nio.ByteBuffer;

public class PAGImageLayer extends PAGLayer {
    /**
     * Make a PAGImageLayer with with, height and duration(in microseconds).
     */
    public static PAGImageLayer Make(int width, int height, long duration) {
        long context = nativeMake(width, height, duration);
        if (context == 0) {
            return null;
        }
        return new PAGImageLayer(context);
    }

    public PAGImageLayer(long nativeContext) {
        super(nativeContext);
    }

    /**
     * Returns the content duration in microseconds, which indicates the minimal length required for
     * replacement.
     */
    public native long contentDuration();

    /**
     * Returns the time ranges of the source video for replacement.
     */
    public native PAGVideoRange[] getVideoRanges();

    /**
     * [Deprecated]
     * Replace the original image content with the specified PAGImage object.
     * Passing in null for the image parameter resets the layer to its default image content.
     * The replaceImage() method modifies all associated PAGImageLayers that have the same
     * editableIndex to this layer.
     *
     * @param image The PAGImage object to replace with.
     */
    public void replaceImage(PAGImage image) {
        replaceImage(image == null ? 0 : image.nativeContext);
    }

    /**
     * Replace the original image content with the specified PAGImage object.
     * Passing in null for the image parameter resets the layer to its default image content.
     * The setImage() method only modifies the content of the calling PAGImageLayer.
     *
     * @param image The PAGImage object to replace with.
     */
    public void setImage(PAGImage image) {
        setImage(image == null ? 0 : image.nativeContext);
    }

    private native void replaceImage(long image);

    private native void setImage(long image);

    /**
     * The default image data of this layer, which is webp format.
     */
    public native ByteBuffer imageBytes();

    private static native long nativeMake(int width, int height, long duration);

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
