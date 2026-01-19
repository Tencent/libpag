package org.libpag;

import org.extra.tools.LibraryLoadUtils;

public class PAGSolidLayer extends PAGLayer {
    public PAGSolidLayer(long nativeContext) {
        super(nativeContext);
    }

    /**
     * Returns the layer's solid color.
     */
    public native int solidColor();

    /**
     * Set the the layer's solid color.
     */
    public native void setSolidColor(int solidColor);

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
