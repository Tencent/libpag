import type { WebFont } from '@tgfx/core/web-mask';
import type { ctor, Point, Rect, Vector } from './types';

export interface TimeRange {
  start: number;
  end: number;
}

export interface VideoReader {
  isSought: boolean;
  isPlaying: boolean;
  prepare: (targetFrame: number, playbackRate: number) => Promise<void>;
  getError: () => Promise<any>;
  onDestroy: () => void;
  play: () => Promise<void>;
  pause: () => void;
  stop: () => void;
}

export type VideoDecoderConstructor = new (
  mp4Data: Uint8Array,
  width: number,
  height: number,
  frameRate: number,
  staticTimeRanges: TimeRange[],
) => VideoReader;

export interface FontMetrics {
  ascent: number;
  descent: number;
  xHeight: number;
  capHeight: number;
}

export interface ScalerContext {
  fontString: () => string;
  getTextAdvance: (text: string) => number;
  getTextBounds: (text: string) => Rect;
  generateFontMetrics: () => FontMetrics;
  generateImage: (text: string, bounds: Rect) => TexImageSource | OffscreenCanvas;
}

export interface ScalerContextConstructor {
  isEmoji: (text: string) => boolean;
  new (fontName: string, fontStyle: string, size: number, fauxBold: boolean, fauxItalic: boolean): ScalerContext;
}

export interface WebMask {
  fillPath: (path: Path2D, fillType: ctor) => void;
  fillText: (webFont: WebFont, texts: Vector<string>, positions: Vector<Point>, matrixWasmIns: any) => void;
  strokeText: (
    webFont: WebFont,
    stroke: { width: number; cap: ctor; join: ctor; miterLimit: number },
    texts: Vector<string>,
    positions: Vector<Point>,
    matrixWasmIns: any,
  ) => void;
  clear: () => void;
}

export type WebMaskConstructor = new (canvas: HTMLCanvasElement | OffscreenCanvas) => WebMask;
