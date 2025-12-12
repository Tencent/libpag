/* global EmscriptenModule */

import { PAGFile } from './pag-file';
import { PAGImage } from './pag-image';
import { PAGSurface } from './pag-surface';
import { WebAssemblyQueue } from './utils/queue';
import { PAGView } from './pag-view';
import { PAGFont } from './pag-font';
import { PAGPlayer } from './pag-player';
import { PAGLayer } from './pag-layer';
import { PAGComposition } from './pag-composition';
import { PAGTextLayer } from './pag-text-layer';
import { GlobalCanvas } from './core/global-canvas';
import { BackendContext } from './core/backend-context';
import { PAGImageLayer } from './pag-image-layer';
import { PAGSolidLayer } from './pag-solid-layer';
import { Matrix as ClassMatrix } from './core/matrix';
import { RenderCanvas } from './core/render-canvas';
import {VideoReader, VideoReaderManager} from "./interfaces";
import type { ScalerContextConstructor, VideoDecoderConstructor } from './interfaces';

import type { ArrayBufferImage } from '@tgfx/wechat/array-buffer-image';
import type { TGFXPathFillType, TGFXLineCap, TGFXLineJoin, EmscriptenGL, ctor, Point, Vector, Rect, } from '@tgfx/types';
export type { EmscriptenGL, ctor, Point, Vector, Rect };

declare global {
  interface Window {
    WeixinJSBridge?: any;
  }
}

export interface PAG extends EmscriptenModule {
  _PAGFile: {
    _Load: (buffer: Uint8Array) => any;
    _MaxSupportedTagLevel: () => number;
  };
  _PAGImage: {
    _FromNativeImage: (source: TexImageSource | ArrayBufferImage) => any;
    _FromPixels: (
      pixels: Uint8Array,
      width: number,
      height: number,
      rowBytes: number,
      colorType: ColorType,
      alphaType: AlphaType,
    ) => any;
    _FromTexture: (textureID: number, width: number, height: number, flipY: boolean) => any;
  };
  _PAGPlayer: any;
  _PAGSurface: {
    _FromCanvas: (canvasID: string) => any;
    _FromTexture: (textureID: number, width: number, height: number, flipY: boolean) => any;
    _FromRenderTarget: (framebufferID: number, width: number, height: number, flipY: boolean) => any;
  };
  _PAGComposition: {
    _Make: (width: number, height: number) => any;
  };
  _PAGTextLayer: {
    _Make: ((duration: number, text: string, fontSize: number, fontFamily: string, fontStyle: string) => any) &
      ((duration: number, textDocumentHandle: TextDocument) => any);
  };
  _PAGImageLayer: {
    _Make: (width: number, height: number, duration: number) => any;
  };
  _PAGSolidLayer: {
    _Make: (duration: number, width: number, height: number, solidColor: Color, opacity: number) => any;
  };
  _PAGFont: {
    _create: (fontFamily: string, fontStyle: string) => any;
    _SetFallbackFontNames: (fontName: any) => void;
  };
  _Matrix: {
    _MakeAll: (
      scaleX: number,
      skewX: number,
      transX: number,
      skewY: number,
      scaleY: number,
      transY: number,
      pers0: number,
      pers1: number,
      pers2: number,
    ) => any;
    _MakeScale: ((sx: number, sy: number) => any) & ((scale: number) => any);
    _MakeTrans: (dx: number, dy: number) => any;
  };
  _registerSoftwareDecoderFactory: (factory: SoftwareDecoderFactory | null) => void;
  VectorString: any;
  webAssemblyQueue: WebAssemblyQueue;
  GL: EmscriptenGL;
  TGFXPathFillType: TGFXPathFillType;
  TGFXLineCap: TGFXLineCap;
  TGFXLineJoin: TGFXLineJoin;
  globalCanvas: GlobalCanvas;
  module: PAG;
  PAGPlayer: typeof PAGPlayer;
  PAGFile: typeof PAGFile;
  PAGView: typeof PAGView;
  PAGFont: typeof PAGFont;
  PAGImage: typeof PAGImage;
  PAGLayer: typeof PAGLayer;
  PAGComposition: typeof PAGComposition;
  PAGSurface: typeof PAGSurface;
  PAGTextLayer: typeof PAGTextLayer;
  PAGImageLayer: typeof PAGImageLayer;
  PAGSolidLayer: typeof PAGSolidLayer;
  ScalerContext: ScalerContextConstructor;
  VideoReader: VideoDecoderConstructor;
  GlobalCanvas: typeof GlobalCanvas;
  BackendContext: typeof BackendContext;
  Matrix: typeof ClassMatrix;
  RenderCanvas: typeof RenderCanvas;
  traceImage: (info: { width: number; height: number }, pixels: Uint8Array, tag: string) => void;
  registerSoftwareDecoderFactory: (factory: SoftwareDecoderFactory | null) => void;
  SDKVersion: () => string;
  currentPlayer: PAGPlayer | null;
  videoReaderManager: VideoReaderManager | null;
  _videoInfoManager: {
    _Make: (pagFile:any) => any;
    _HasVideo:(pagFile:any) => any;
  };
  _useSoftwareDecoder: boolean | null;
  [key: string]: any;
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

// [Deprecated]
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
   * Notifies the update of the animation.
   */
  onAnimationUpdate = 'onAnimationUpdate',
  /**
   * Notifies the play of the animation.
   */
  onAnimationPlay = 'onAnimationPlay',
  /**
   * Notifies the pause of the animation.
   */
  onAnimationPause = 'onAnimationPause',
  /**
   * [Deprecated]
   * Notifies the flushed of the animation.
   */
  onAnimationFlushed = 'onAnimationFlushed',
}

export interface PAGViewEventMap {
  /**
   * Notifies the start of the animation.
   */
  onAnimationStart: PAGView;
  /**
   * Notifies the end of the animation.
   */
  onAnimationEnd: PAGView;
  /**
   * Notifies the cancellation of the animation.
   */
  onAnimationCancel: PAGView;
  /**
   * Notifies the repetition of the animation.
   */
  onAnimationRepeat: PAGView;
  /**
   * Notifies the update of the animation.
   */
  onAnimationUpdate: PAGView;
  /**
   * [Deprecated]
   * Notifies the play of the animation.
   */
  onAnimationPlay: PAGView;
  /**
   * Notifies the pause of the animation.
   */
  onAnimationPause: PAGView;
  /**
   * [Deprecated]
   * Notifies the flushed of the animation.
   */
  onAnimationFlushed: PAGView;
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

export const enum MatrixIndex {
  a,
  c,
  tx,
  b,
  d,
  ty,
}

export const enum DecoderResult {
  /**
   * The calling is successful.
   */
  Success = 0,
  /**
   * Output is not available in this state, need more input buffers.
   */
  TryAgainLater = -1,
  /**
   * The calling fails.
   */
  Error = -2,
}

/**
 * Describes how pixel bits encode color. These values match up with the enum in Bitmap.Config on
 * Android platform.
 */
export const enum ColorType {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * Each pixel is stored as a single translucency (alpha) channel. This is very useful to
   * efficiently store masks for instance. No color information is stored. With this configuration,
   * each pixel requires 1 byte of memory.
   */
  ALPHA_8,
  /**
   * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
   * bits of precision (256 possible values). The channel order is: red, green, blue, alpha.
   */
  RGBA_8888,
  /**
   * Each pixel is stored on 4 bytes. Each channel (RGB and alpha for translucency) is stored with 8
   * bits of precision (256 possible values). The channel order is: blue, green, red, alpha.
   */
  BGRA_8888,
}

/**
 * Describes how to interpret the alpha component of a pixel.
 */
export const enum AlphaType {
  /**
   * uninitialized.
   */
  Unknown,
  /**
   * pixel is opaque.
   */
  Opaque,
  /**
   * pixel components are premultiplied by alpha.
   */
  Premultiplied,
  /**
   * pixel components are independent of alpha.
   */
  Unpremultiplied,
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
export interface YUVBuffer {
  data: number[];
  lineSize: number[];
}

export interface DebugData {
  FPS?: number;
  flushTime?: number;
}

export interface PAGVideoRange {
  /**
   * The start time of the source video, in microseconds.
   */
  startTime: number;
  /**
   * The end time of the source video (not included), in microseconds.
   */
  endTime: number;
  /**
   * The duration for playing after applying speed.
   */
  playDuration: number;
  /**
   * Indicates whether the video should play backward.
   */
  reversed: boolean;
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

  public delete: () => void;

  private constructor();
}

export declare class SoftwareDecoder {
  public onConfigure(headers: Uint8Array[], mimeType: string, width: number, height: number): boolean;
  public onSendBytes(bytes: Uint8Array, timestamp: number): number; // 0:Success -1:TryAgainLater -2:Error
  public onDecodeFrame(): number; // 0:Success -1:TryAgainLater -2:Error
  public onEndOfStream(): number; // 0:Success -1:TryAgainLater -2:Error
  public onFlush(): void;
  public onRenderFrame(): YUVBuffer | null;
  public onRelease(): void;
}

export declare class SoftwareDecoderFactory {
  public createSoftwareDecoder(pag: PAG): SoftwareDecoder | null;
}


export class VecArray extends Array {
  private isDeleted = false;

  private constructor(...items: any[]) {
    super(...items);
    Object.setPrototypeOf(this, VecArray.prototype);
  }

  public static create(): VecArray {
    return new VecArray();
  }

  public get(index: number): any {
    this.ensureNotDeleted();
    if (index < 0 || index >= this.length) {
      throw new RangeError('Index out of bounds');
    }
    return this[index];
  }

  public push_back(value: any): void {
    this.ensureNotDeleted();
    this.push(value);
  }

  public size(): number {
    this.ensureNotDeleted();
    return this.length;
  }

  public delete(): void {
    this.ensureNotDeleted();
    this.length = 0;
    this.isDeleted = true;
  }

  private ensureNotDeleted(): void {
    if (this.isDeleted) {
      throw new Error('This VecArray instance has been deleted.');
    }
  }
}