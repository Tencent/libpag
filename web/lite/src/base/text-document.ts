import { ParagraphJustification } from '../constant';
import { Black, Color } from './color';
import { Point, ZERO_POINT } from './point';

export class TextDocument {
  /**
   * When true, the text layer shows a fill.
   */
  public applyFill = true;

  /**
   * When true, the text layer shows a stroke.
   */
  public applyStroke = false;

  public baselineShift = 0;

  /**
   * When true, the text layer is paragraph (bounded) text.
   */
  public boxText = false;

  public boxTextPos: Point = ZERO_POINT;

  /**
   * For box text, the pixel dimensions for the text bounds.
   */
  public boxTextSize: Point = ZERO_POINT;

  public firstBaseLine = 0;

  public fauxBold = false;

  public fauxItalic = false;
  /**
   * The text layer’s fill color.
   */
  public fillColor: Color = Black;

  /**
   * A string with the name of the font family.
   **/
  public fontFamily = '';

  /**
   * A string with the style information — e.g., “bold”, “italic”.
   **/
  public fontStyle = '';

  /**
   * The text layer’s font size in pixels.
   */
  public fontSize = 24;

  /**
   * The text layer’s stroke color.
   */
  public strokeColor: Color = Black;

  /**
   * Indicates the rendering order for the fill and stroke of a text layer.
   */
  public strokeOverFill = true;

  /**
   * The text layer’s stroke thickness.
   */
  public strokeWidth = 1;

  /**
   * The text layer’s Source Text value.
   */
  public text = '';

  /**
   * The paragraph justification for the text layer.
   */
  public justification: ParagraphJustification = ParagraphJustification.LeftJustify;

  /**
   * The space between lines, 0 indicates 'auto', which is fontSize * 1.2
   */
  public leading = 0;

  /**
   * The text layer’s spacing between characters.
   */
  public tracking = 0;
}
