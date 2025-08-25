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
  fontString: (fauxBold: boolean, fauxItalic: boolean) => string;
  getAdvance: (text: string) => number;
  getBounds: (text: string, fauxBold: boolean, fauxItalic: boolean) => Rect;
  getFontMetrics: () => FontMetrics;
  generateImage: (text: string, bounds: Rect) => TexImageSource | OffscreenCanvas;
}

export interface ScalerContextConstructor {
  isEmoji: (text: string) => boolean;
  new (fontName: string, fontStyle: string, size: number, fauxBold: boolean, fauxItalic: boolean): ScalerContext;
}
