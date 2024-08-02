import { Rect } from '@ohos.arkui.node';

export enum PAGTextJustification {
  Left = 0,
  Center = 1,
  Right = 2,
  FullJustifyLastLineLeft = 3,
  FullJustifyLastLineRight = 4,
  FullJustifyLastLineCenter = 5,
  FullJustifyLastLineFull = 6
}

export class PAGText {
  /**
   * When true, the text layer shows a fill.
   */
  applyFill: boolean = true;
  /**
   * When true, the text layer shows a stroke.
   */
  applyStroke: boolean = false;
  /**
   * Readonly, external modifications are not valid.
   */
  baselineShift: number = 0;
  /**
   * When true, the text layer is paragraph (bounded) text.
   * Readonly, external modifications are not valid.
   */
  boxText: boolean = false;
  /**
   * For box text, the pixel boundary for the text bounds.
   * Readonly, external modifications are not valid.
   */
  boxTextRect: Rect = {
    left: 0,
    top: 0,
    right: 0,
    bottom: 0
  };
  /**
   * Readonly, external modifications are not valid.
   */
  firstBaseLine: number = 0;
  fauxBold: boolean = false;
  fauxItalic: boolean = false;
  /**
   * The text layer’s fill color.
   */
  fillColor: number = 0;
  /**
   * A string with the name of the font family.
   **/
  fontFamily: string = "";
  /**
   * A string with the style information — e.g., “bold”, “italic”.
   **/
  fontStyle: string = "";
  /**
   * The text layer’s font size in pixels.
   */
  fontSize: number = 24;
  /**
   * The text layer’s stroke color.
   */
  strokeColor: number = 0;
  /**
   * Indicates the rendering order for the fill and stroke of a text layer.
   * Readonly, external modifications are not valid.
   */
  strokeOverFill: boolean = true;
  /**
   * The text layer’s stroke thickness.
   */
  strokeWidth: number = 1;
  /**
   * The text layer’s Source Text value.
   */
  text: string = "";
  /**
   * The paragraph justification for the text layer. Such as : PAGJustificationLeftJustify, PAGJustificationCenterJustify...
   */
  justification: PAGTextJustification = PAGTextJustification.Left;
  /**
   * The space between lines, 0 indicates 'auto', which is fontSize * 1.2
   */
  leading: number = 0;
  /**
   * The text layer’s spacing between characters.
   */
  tracking: number = 0;
  /**
   * The text layer’s background color.
   */
  backgroundColor: number = 0;
  /**
   * The text layer’s background alpha. 0 = 100% transparent, 255 = 100% opaque.
   */
  backgroundAlpha: number = 255;
}
