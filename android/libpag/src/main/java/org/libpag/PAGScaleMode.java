package org.libpag;

/**
 * Defines the values used in the setScaleMode method of PAGPlayer.
 */
public class PAGScaleMode {
    /**
     * The content is not scaled.
     */
    public static final int None = 0;
    /**
     * The content is stretched to fit.
     */
    public static final int Stretch = 1;
    /**
     * The content is scaled with respect to the original unscaled image's aspect ratio.
     * This is the default value.
     */
    public static final int LetterBox = 2;
    /**
     * The content is scaled to fit with respect to the original unscaled image's aspect ratio.
     * This results in cropping on one axis.
     */
    public static final int Zoom = 3;
}
