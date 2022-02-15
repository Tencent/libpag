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

    /**
     * If set to true, PAGVideoDecoder uses a software decoder first, but initializes a hardware on async
     * thread, and then switches to the hardware decoder when it is initialized.
     * The default is true, which will improve the performance of first frame rendering.
     */
    public static native void SetSoftwareToHardwareEnabled(boolean value);

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
