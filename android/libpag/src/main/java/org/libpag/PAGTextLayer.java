package org.libpag;

import org.extra.tools.LibraryLoadUtils;

public class PAGTextLayer extends PAGLayer {
    public PAGTextLayer(long nativeContext) {
        super(nativeContext);
    }

    /**
     * Returns the text layer’s fill color.
     */
    public native int fillColor();

    /**
     * Set the text layer’s fill color.
     */
    public native void setFillColor(int color);

    /**
     * Returns the text layer's font.
     */
    public native PAGFont font();

    /**
     * Set the text layer's font.
     */
    public void setFont(PAGFont font) {
        setFont(font.fontFamily, font.fontStyle);
    }

    /**
     * Returns the text layer's font size.
     */
    public native float fontSize();

    /**
     * Set the text layer's font size.
     */
    public native void setFontSize(float fontSize);

    /**
     * Returns the text layer's stroke color.
     */
    public native int strokeColor();

    /**
     * Set the text layer's stroke color.
     */
    public native void setStrokeColor(int color);

    /**
     * Returns the text layer's text.
     */
    public native String text();

    /**
     * Set the text layer's text.
     */
    public native void setText(String text);

    /**
     * Reset the text layer to its default text data.
     */
    public native void reset();

    private native void setFont(String fontFamily, String fontStyle);

    private static native void nativeInit();

    static {
        LibraryLoadUtils.loadLibrary("pag");
        nativeInit();
    }
}
