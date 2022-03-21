package org.libpag;

/**
 * Describes how to interpret the alpha component of a pixel.
 */
public class PAGAlphaType {
    /**
     * uninitialized.
     */
    public static final int Unknown = 0;
    /**
     * pixel is opaque.
     */
    public static final int Opaque = 1;
    /**
     * pixel components are premultiplied by alpha.
     */
    public static final int Premultiplied = 2;
    /**
     * pixel components are independent of alpha.
     */
    public static final int Unpremultiplied = 3;
}
