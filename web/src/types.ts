/* global EmscriptenModule */

import { PAGFile } from './pag-file';
import { PAGImage } from './pag-image';
import { PAGSurface } from './pag-surface';
import { WebAssemblyQueue } from './utils/queue';
import { VideoReader } from './core/video-reader';
import { ScalerContext } from './core/scaler-context';
import { PAGView } from './pag-view';
import { PAGFont } from './pag-font';
import { PAGPlayer } from './pag-player';
import { PAGLayer } from './pag-layer';
import { NativeImage } from './core/native-image';
import { WebMask } from './core/web-mask';

export interface PAG extends EmscriptenModule {
  _PAGFile: {
    Load: (bytes: number, length: number) => Promise<PAGFile>;
  };
  _PAGImage: {
    FromBytes: (bytes: number, length: number) => Promise<PAGImage>;
    FromNativeImage: (nativeImage: NativeImage) => Promise<PAGImage>;
  };
  _PAGPlayer: any;
  _PAGSurface: {
    FromCanvas: (canvasID: string) => Promise<PAGSurface>;
    FromTexture: (textureID: number, width: number, height: number, flipY: boolean) => Promise<PAGSurface>;
  };
  VectorString: any;
  webAssemblyQueue: WebAssemblyQueue;
  PAGPlayer: typeof PAGPlayer;
  PAGFile: typeof PAGFile;
  PAGView: typeof PAGView;
  PAGFont: typeof PAGFont;
  PAGImage: typeof PAGImage;
  PAGLayer: typeof PAGLayer;
  NativeImage: typeof NativeImage;
  WebMask: typeof WebMask;
  ScalerContext: typeof ScalerContext;
  VideoReader: typeof VideoReader;
  registerFontPath: (path: string) => void;
  setFallbackFontNames: (fontName: any) => void;
  traceImage: (info, pixels, tag: string) => void;
  GL: any;
  LayerType: typeof LayerType;
}

/**
 * Defines the rules on how to scale the content to fit the specified area.
 */
export enum PAGScaleMode {
  /**
   * The content is not scaled.
   */
  None = 0,
  /**
   * The content is stretched to fit.
   */
  Stretch = 1,
  /**
   * The content is scaled with respect to the original unscaled image's aspect ratio.
   * This is the default value.
   */
  LetterBox = 2,
  /**
   * The content is scaled to fit with respect to the original unscaled image's aspect ratio.
   * This results in cropping on one axis.
   */
  Zoom = 3,
}

export enum PAGViewListenerEvent {
  /**
   * Notifies the start of the animation.
   */
  onAnimationStart = 'onAnimationStart',
  /**
   * Notifies the end of the animation.
   */
  onAnimationEnd = 'onAnimationEnd',
  /**
   * Notifies the cancellation of the animation.
   */
  onAnimationCancel = 'onAnimationCancel',
  /**
   * Notifies the repetition of the animation.
   */
  onAnimationRepeat = 'onAnimationRepeat',
}

export const enum ParagraphJustification {
  LeftJustify = 0,
  CenterJustify = 1,
  RightJustify = 2,
  FullJustifyLastLineLeft = 3,
  FullJustifyLastLineRight = 4,
  FullJustifyLastLineCenter = 5,
  FullJustifyLastLineFull = 6,
}

export const enum TextDirection {
  Default = 0,
  Horizontal = 1,
  Vertical = 2,
}

export interface Point {
  x: number;
  y: number;
}

export declare class TextDocument {
  /**
   * When true, the text layer shows a fill.
   */
  public applyFill: boolean;
  /**
   * When true, the text layer shows a stroke.
   */
  public applyStroke: boolean;

  public baselineShift: number;
  /**
   * When true, the text layer is paragraph (bounded) text.
   */
  public boxText: boolean;

  public boxTextPos: Readonly<Point>;
  /**
   * For box text, the pixel dimensions for the text bounds.
   */
  public boxTextSize: Readonly<Point>;

  public firstBaseLine: number;

  public fauxBold: boolean;

  public fauxItalic: boolean;
  /**
   * The text layer’s fill color.
   */
  public fillColor: Readonly<Color>;
  /**
   * A string with the name of the font family.
   **/
  public fontFamily: string;
  /**
   * A string with the style information — e.g., “bold”, “italic”.
   **/
  public fontStyle: string;
  /**
   * The text layer’s font size in pixels.
   */
  public fontSize: number;
  /**
   * The text layer’s stroke color.
   */
  public strokeColor: Readonly<Color>;
  /**
   * Indicates the rendering order for the fill and stroke of a text layer.
   */
  public strokeOverFill: boolean;
  /**
   * The text layer’s stroke thickness.
   */
  public strokeWidth: number;
  /**
   * The text layer’s Source Text value.
   */
  public text: string;
  /**
   * The paragraph justification for the text layer.
   */
  public justification: ParagraphJustification;
  /**
   * The space between lines, 0 indicates 'auto', which is fontSize * 1.2
   */
  public leading: number;
  /**
   * The text layer’s spacing between characters.
   */
  public tracking: number;
  /**
   *  The text layer’s background color
   */
  public backgroundColor: Readonly<Color>;
  /**
   *  The text layer’s background alpha. 0 = 100% transparent, 255 = 100% opaque.
   */
  public backgroundAlpha: number;

  public direction: TextDirection;
  private constructor();
}

export interface Color {
  red: number;
  green: number;
  blue: number;
}
/**
 * Layers are always one of the following types.
 */
export enum LayerType {
  Unknown,
  Null,
  Solid,
  Text,
  Shape,
  Image,
  PreCompose,
}
