/**
 * VideoDecode wait frame num.
 */
export const VIDEO_DECODE_WAIT_FRAME = 1;
/**
 * VideoDecode seek timeout frame num.
 */
export const VIDEO_DECODE_SEEK_TIMEOUT_FRAME = 12;
/**
 * NAL unit length. [ 0, 0, 0, 1 ]
 */
export const NALU_HEADER_LENGTH = 4;
/**
 * Web safe font.
 */
export const WEB_SAFE_FONT = 'Arial';
/**
 * Default render canvas size.
 */
export const DEFAULT_CANVAS_SIZE = 2560;
/**
 * Default WebGL ContextAttributes
 */
export const WEBGL_CONTEXT_ATTRIBUTES = {
  depth: false,
  stencil: false,
  antialias: false,
};

export const VIDEO_PLAYBACK_RATE_MIN = 0.125;
export const VIDEO_PLAYBACK_RATE_MAX = 4;

export const CANVAS_POOL_MAX_SIZE = 10;

/**
 * GPUTextureUsage.COPY_SRC bit, as defined by the WebGPU spec. Declared locally because
 * lib.dom of TypeScript ~5.0 does not ship WebGPU declarations.
 */
export const GPU_TEXTURE_USAGE_COPY_SRC = 0x01;
