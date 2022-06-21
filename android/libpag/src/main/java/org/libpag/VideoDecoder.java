package org.libpag;

import org.extra.tools.LibraryLoadUtils;

public abstract class VideoDecoder {

    /**
     * Register a software decoder factory to implement the decoder fallback mechanism.
     * For further info please visit : https://pag.io/docs/plugin-decoder.html
     */
    public static native void RegisterSoftwareDecoderFactory(long factory);

    /**
     * Set the maximum number of hardware video decoders that can be create.
     */
    public static native void SetMaxHardwareDecoderCount(int maxCount);

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
