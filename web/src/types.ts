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
import { PAGComposition } from './pag-composition';
import { NativeImage } from './core/native-image';
import { WebMask } from './core/web-mask';

declare global {
  interface Window {
    WeixinJSBridge?: any;
  }
}

export interface PAG extends EmscriptenModule {
  _PAGFile: {
    _Load: (bytes: number, length: number) => any;
  };
  _PAGImage: {
    _FromNativeImage: (nativeImage: NativeImage) => any;
  };
  _PAGPlayer: any;
  _PAGSurface: {
    _FromCanvas: (canvasID: string) => any;
    _FromTexture: (textureID: number, width: number, height: number, flipY: boolean) => any;
    _FromFrameBuffer: (framebufferID: number, width: number, height: number, flipY: boolean) => any;
  };
  _SetFallbackFontNames: (fontName: any) => void;
  VectorString: any;
  webAssemblyQueue: WebAssemblyQueue;
  PAGPlayer: typeof PAGPlayer;
  PAGFile: typeof PAGFile;
  PAGView: typeof PAGView;
  PAGFont: typeof PAGFont;
  PAGImage: typeof PAGImage;
  PAGLayer: typeof PAGLayer;
  PAGComposition: typeof PAGComposition;
  PAGSurface: typeof PAGSurface;
  NativeImage: typeof NativeImage;
  WebMask: typeof WebMask;
  ScalerContext: typeof ScalerContext;
  VideoReader: typeof VideoReader;
  traceImage: (info, pixels, tag: string) => void;
  GL: any;
  LayerType: typeof LayerType;
}

/**
 * Defines the rules on how to scale the content to fit the specified area.
 */
export const enum PAGScaleMode {
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

export const enum PAGViewListenerEvent {
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

/**
 * Layers are always one of the following types.
 */
export const enum LayerType {
  Unknown,
  Null,
  Solid,
  Text,
  Shape,
  Image,
  PreCompose,
}

/**
 * Defines the rules on how to stretch the timeline of content to fit the specified duration.
 */
export const enum PAGTimeStretchMode {
  /**
   * Keep the original playing speed, and display the last frame if the content's duration is less
   * than target duration.
   */
  None = 0,
  /*
   * Change the playing speed of the content to fit target duration.
   */
  Scale = 1,
  /**
   * Keep the original playing speed, but repeat the content if the content's duration is less than
   * target duration. This is the default mode.
   */
  Repeat = 2,
  /**
   * Keep the original playing speed, but repeat the content in reversed if the content's duration
   * is less than target duration.
   */
  RepeatInverted = 3,
}

export interface Point {
  x: number;
  y: number;
}

/**
 * Marker stores comments and other metadata and mark important times in a composition or layer.
 */
export interface Marker {
  startTime: number;
  duration: number;
  comment: string;
}

export interface Color {
  red: number;
  green: number;
  blue: number;
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

export declare class Vector<T> {
  public get: (index: number) => T;

  public size: () => number;

  private constructor();
}
