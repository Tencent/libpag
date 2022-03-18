package org.libpag;

import org.extra.tools.LibraryLoadUtils;

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
     * Returns the preferred duration of a PAGMovie object which is used as a image replacement for this layer.
     */
    public native long contentDuration();

    /**
     * [Deprecated](Please use PAGMovie class instead) Returns the time ranges of the source video for replacement.
     */
    public native PAGVideoRange[] getVideoRanges();

    /**
     * Replace the original image content with the specified PAGImage object. Passing in null for the image parameter
     * resets the layer to its default image content.
     *
     * @param image     The PAGImage object to replace with.
     */
    public void replaceImage(PAGImage image) {
        replaceImage(image == null ? 0 : image.nativeContext);
    }

    private native void replaceImage(long image);

    private static native long nativeMake(int width, int height, long duration);

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
