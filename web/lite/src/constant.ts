export const ZERO_ID = 0;
export const ZERO_TIME = 0;
export const OPAQUE = 255;
export const TRANSPARENT = 0;

/**
 * Default WebGL ContextAttributes
 */
export const WEBGL_CONTEXT_ATTRIBUTES = {
  alpha: true,
  depth: false,
  stencil: false,
  antialias: false,
};

// 混合模式
export const enum BlendMode {
  Normal = 0,
  Multiply = 1,
  Screen = 2,
  Overlay = 3,
  Darken = 4,
  Lighten = 5,
  ColorDodge = 6,
  ColorBurn = 7,
  HardLight = 8,
  SoftLight = 9,
  Difference = 10,
  Exclusion = 11,
  Hue = 12,
  Saturation = 13,
  Color = 14,
  Luminosity = 15,

  // modes used only when rendering.
  DestinationIn = 21,
  DestinationOut = 22,
  DestinationATop = 23,
  SourceIn = 24,
  SourceOut = 25,
  Xor = 26,
}

// 路径动作
export const enum PathVerb {
  MoveTo,
  LineTo,
  CurveTo,
  Close,
}

// 关键帧的差值器类型
export const enum KeyframeInterpolationType {
  None = 0,
  Linear = 1,
  Bezier = 2,
  Hold = 3,
}

// 段落对齐
export const enum ParagraphJustification {
  LeftJustify = 0,
  CenterJustify = 1,
  RightJustify = 2,
  FullJustifyLastLineLeft = 3,
  FullJustifyLastLineRight = 4,
  FullJustifyLastLineCenter = 5,
  FullJustifyLastLineFull = 6,
}

export const IS_IOS = /(ios|ipad|iphone)/.test(navigator.userAgent.toLowerCase());
