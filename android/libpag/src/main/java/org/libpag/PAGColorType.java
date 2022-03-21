package org.libpag;

/**
 * Describes how pixel bits encode color. These values match up with the enum in Bitmap.Config on
 * Android platform.
 */
public class PAGColorType {
    /**
     * uninitialized.
     */
    public static final int Unknown = 0;
    /**
     * Each pixel is stored as a single translucency (alpha) channel. This is very useful to
     * efficiently store masks for instance. No color information is stored. With this configuration,
     * each pixel requires 1 byte of memory.
     */
    public static final int ALPHA_8 = 1;
    /**
     * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
     * bits of precision (256 possible values). The channel order is: red, green, blue, alpha.
     */
    public static final int RGBA_8888 = 2;
    /**
     * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
     * bits of precision (256 possible values). The channel order is: blue, green, red, alpha.
     */
    public static final int BGRA_8888 = 3;
}
