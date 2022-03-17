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
import { PAGTextLayer } from './pag-text-layer';

declare global {
  interface Window {
    WeixinJSBridge?: any;
  }
}

export interface PAG extends EmscriptenModule {
  _PAGFile: {
    _Load: (bytes: number, length: number) => any;
    _MaxSupportedTagLevel: () => number;
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
  _PAGComposition: {
    _Make: (width: number, height: number) => any;
  };
  _PAGTextLayer: {
    _Make(duration: number, text: string, fontSize: number, fontFamily: string, fontStyle: string): any;
    _Make(duration: number, textDocumentHandle: TextDocument): any;
  };
  _PAGImageLayer: {
    _Make(width: number, height: number, duration: number): any;
  };
  _PAGSolidLayer: {
    _Make(duration: number, width: number, height: number, solidColor: Color, opacity: number): any;
  };
  _PAGFont: {
    _create(fontFamily: string, fontStyle: string): any;
    _SetFallbackFontNames(fontName: any): void;
  };
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
  PAGTextLayer: typeof PAGTextLayer;
  NativeImage: typeof NativeImage;
  WebMask: typeof WebMask;
  ScalerContext: typeof ScalerContext;
  VideoReader: typeof VideoReader;
  traceImage: (info: { width: number; height: number }, pixels: Uint8Array, tag: string) => void;
  GL: EmscriptenGL;
}

export interface EmscriptenGL {
  currentContext: { GLctx: WebGLRenderingContext };
  textures: WebGLTexture[];
  registerContext: (gl: WebGLRenderingContext, options: { majorVersion: number; minorVersion: number }) => number;
  makeContextCurrent: (contextId: number) => void;
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
  /**
   * Notifies the play of the animation.
   */
  onAnimationPlay = 'onAnimationPlay',
  /**
   * Notifies the pause of the animation.
   */
  onAnimationPause = 'onAnimationPause',
  /**
   * Notifies the flushed of the animation.
   */
  onAnimationFlushed = 'onAnimationFlushed',
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

export const enum PathFillType {
  /**
   * Enclosed by a non-zero sum of contour directions.
   */
  Winding,
  /**
   * Enclosed by an odd number of contours.
   */
  EvenOdd,
  /**
   * Enclosed by a zero sum of contour directions.
   */
  InverseWinding,
  /**
   * Enclosed by an even number of contours.
   */
  InverseEvenOdd,
}

export const enum MatrixIndex {
  a,
  b,
  c,
  d,
  tx,
  ty,
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

export declare class Matrix {
  /**
   * The entry at position [1,1] in the matrix.
   */
  public a: number;
  /**
   * The entry at position [1,2] in the matrix.
   */
  public b: number;
  /**
   * The entry at position [2,1] in the matrix.
   */
  public c: number;
  /**
   * The entry at position [2,2] in the matrix.
   */
  public d: number;
  /**
   * The entry at position [3,1] in the matrix.
   */
  public tx: number;
  /**
   * The entry at position [3,2] in the matrix.
   */
  public ty: number;
  /**
   * Sets Matrix value.
   */
  public set: (index: number, value: number) => {};
  public setAffine: (a: number, b: number, c: number, d: number, tx: number, ty: number) => {};
  private constructor();
}

export declare class Rect {
  /**
   * smaller x-axis bounds.
   */
  public left: number;
  /**
   * smaller y-axis bounds.
   */
  public top: number;
  /**
   * larger x-axis bounds.
   */
  public right: number;
  /**
   * larger y-axis bounds.
   */
  public bottom: number;
  private constructor();
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

export declare class PAGVideoRange {
  /**
   * The start time of the source video, in microseconds.
   */
  public startTime(): number;
  /**
   * The end time of the source video (not included), in microseconds.
   */
  public endTime(): number;
  /**
   * The duration for playing after applying speed.
   */
  public playDuration(): number;
  /**
   * Indicates whether the video should play backward.
   */
  public reversed(): boolean;

  private constructor();
}

export declare class Vector<T> {
  public get: (index: number) => T;
  public set: (index: number, value: T) => boolean;
  public push_back: (value: T) => void;
  public size: () => number;
  public delete: () => void;
  private constructor();
}
