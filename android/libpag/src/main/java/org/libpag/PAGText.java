package org.libpag;

import android.graphics.Color;
import android.graphics.RectF;

public class PAGText {
    public static class Justification {
        public static final int Left = 0;
        public static final int Center = 1;
        public static final int Right = 2;
        public static final int FullJustifyLastLineLeft = 3;
        public static final int FullJustifyLastLineRight = 4;
        public static final int FullJustifyLastLineCenter = 5;
        public static final int FullJustifyLastLineFull = 6;
    }


    /**
     * When true, the text layer shows a fill.
     */
    public boolean applyFill = true;

    /**
     * When true, the text layer shows a stroke.
     */
    public boolean applyStroke = false;

    /**
     * Readonly, external modifications are not valid.
     */
    public float baselineShift = 0;

    /**
     * When true, the text layer is paragraph (bounded) text.
     * Readonly, external modifications are not valid.
     */
    public boolean boxText = false;
    /**
     * For box text, the pixel boundary for the text bounds.
     * Readonly, external modifications are not valid.
     */
    public RectF boxTextRect = new RectF();

    /**
     * Readonly, external modifications are not valid.
     */
    public float firstBaseLine = 0;

    public boolean fauxBold = false;

    public boolean fauxItalic = false;

    /**
     * The text layer’s fill color.
     */
    public int fillColor = 0;

    /**
     * A string with the name of the font family.
     **/
    public String fontFamily = "";

    /**
     * A string with the style information — e.g., “bold”, “italic”.
     **/
    public String fontStyle = "";

    /**
     * The text layer’s font size in pixels.
     */
    public float fontSize = 24;

    /**
     * The text layer’s stroke color.
     */
    public int strokeColor = 0;

    /**
     * Indicates the rendering order for the fill and stroke of a text layer.
     * Readonly, external modifications are not valid.
     */
    public boolean strokeOverFill = true;

    /**
     * The text layer’s stroke thickness.
     */
    public float strokeWidth = 1;

    /**
     * The text layer’s Source Text value.
     */
    public String text = "";

    /**
     * The paragraph justification for the text layer. Such as : PAGJustificationLeftJustify, PAGJustificationCenterJustify...
     */
    public int justification = Justification.Left;

    /**
     * The space between lines, 0 indicates 'auto', which is fontSize * 1.2
     */
    public float leading = 0;

    /**
     * The text layer’s spacing between characters.
     */
    public float tracking = 0;

    /**
     * The text layer’s background color.
     */
    public int backgroundColor;

    /**
     * The text layer’s background alpha. 0 = 100% transparent, 255 = 100% opaque.
     */
    public int backgroundAlpha;

}
