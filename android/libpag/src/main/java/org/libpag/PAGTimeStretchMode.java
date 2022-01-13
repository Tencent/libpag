package org.libpag;

/**
 * Defines the values used in the setTimeStretchMode method of PAGFile.
 */
public class PAGTimeStretchMode {
    /**
     * Keep the original playing speed, and display the last frame if the content's duration is less than target duration.
     */
    public static final int None = 0;
    /*
     * Change the playing speed of the content to fit target duration.
     */
    public static final int Scale = 1;
    /**
     * Keep the original playing speed, but repeat the content if the content's duration is less than target duration.
     * This is the default mode.
     */
    public static final int Repeat = 2;
    /**
     * Keep the original playing speed, but repeat the content in reversed if the content's duration is less than
     * target duration.
     */
    public static final int RepeatInverted = 3;
}
