/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

let TGFXModule;
const setTGFXModule = (module) => {
  TGFXModule = module;
};

const CANVAS_POOL_MAX_SIZE = 10;

const isInstanceOf = (value, type) => typeof type !== "undefined" && value instanceof type;

const nav = navigator?.userAgent || "";
const IPHONE = /(iphone|ipad|ipod)/i.test(nav);
const SAFARI_OR_IOS_WEBVIEW = /^((?!chrome|android).)*safari/i.test(nav) || IPHONE;
const WORKER = typeof globalThis.importScripts === "function";

const canvasPool = new Array();
const isOffscreenCanvas = (element) => isInstanceOf(element, globalThis.OffscreenCanvas);
const isCanvas = (element) => isOffscreenCanvas(element) || isInstanceOf(element, globalThis.HTMLCanvasElement);
const getCanvas2D = (width, height) => {
  let canvas = canvasPool.pop() || createCanvas2D();
  if (canvas !== null) {
    canvas.width = width;
    canvas.height = height;
  }
  return canvas;
};
const releaseCanvas2D = (canvas) => {
  if (canvasPool.length < CANVAS_POOL_MAX_SIZE) {
    canvasPool.push(canvas);
  }
};
const createCanvas2D = () => {
  if (SAFARI_OR_IOS_WEBVIEW && !WORKER) {
    return document.createElement("canvas");
  }
  try {
    const offscreenCanvas = new OffscreenCanvas(0, 0);
    const context = offscreenCanvas.getContext("2d");
    if (typeof context.measureText === "function") return offscreenCanvas;
    return document.createElement("canvas");
  } catch (err) {
    return document.createElement("canvas");
  }
};

class BitmapImage {
  constructor(bitmap) {
    this.bitmap = bitmap;
  }
  setBitmap(bitmap) {
    if (this.bitmap) {
      this.bitmap.close();
    }
    this.bitmap = bitmap;
  }
}

var MatrixIndex = /* @__PURE__ */ ((MatrixIndex2) => {
  MatrixIndex2[MatrixIndex2["a"] = 0] = "a";
  MatrixIndex2[MatrixIndex2["c"] = 1] = "c";
  MatrixIndex2[MatrixIndex2["tx"] = 2] = "tx";
  MatrixIndex2[MatrixIndex2["b"] = 3] = "b";
  MatrixIndex2[MatrixIndex2["d"] = 4] = "d";
  MatrixIndex2[MatrixIndex2["ty"] = 5] = "ty";
  return MatrixIndex2;
})(MatrixIndex || {});
var WindowColorSpace = /* @__PURE__ */ ((WindowColorSpace2) => {
  WindowColorSpace2[WindowColorSpace2["None"] = 0] = "None";
  WindowColorSpace2[WindowColorSpace2["SRGB"] = 1] = "SRGB";
  WindowColorSpace2[WindowColorSpace2["DisplayP3"] = 2] = "DisplayP3";
  WindowColorSpace2[WindowColorSpace2["Others"] = 3] = "Others";
  return WindowColorSpace2;
})(WindowColorSpace || {});

const createImage = (source) => {
  return new Promise((resolve) => {
    const image = new Image();
    image.onload = function() {
      resolve(image);
    };
    image.onerror = function() {
      console.error("image create from bytes error.");
      resolve(null);
    };
    image.src = source;
  });
};
const createImageFromBytes = (bytes) => {
  const uint8Array = new Uint8Array(bytes);
  const blob = new Blob([uint8Array], { type: "image/*" });
  return createImage(URL.createObjectURL(blob));
};
const readImagePixels = (module, image, width, height) => {
  if (!image) {
    return null;
  }
  const canvas = getCanvas2D(width, height);
  const ctx = canvas.getContext("2d", { willReadFrequently: true });
  if (!ctx) {
    return null;
  }
  ctx.globalCompositeOperation = "copy";
  ctx.drawImage(image, 0, 0, width, height);
  ctx.globalCompositeOperation = "source-over";
  const { data } = ctx.getImageData(0, 0, width, height);
  releaseCanvas2D(canvas);
  if (data.length === 0) {
    return null;
  }
  return new Uint8Array(data);
};
const hasWebpSupport = () => {
  try {
    return document.createElement("canvas").toDataURL("image/webp", 0.5).indexOf("data:image/webp") === 0;
  } catch (err) {
    return false;
  }
};
const getSourceSize = (source) => {
  if (isInstanceOf(source, globalThis.HTMLVideoElement)) {
    return {
      width: source.videoWidth,
      height: source.videoHeight
    };
  }
  return { width: source.width, height: source.height };
};
let syncCanvas = null;
let syncCtx = null;
const syncVideoFrame = (video) => {
  if (!syncCanvas) {
    syncCanvas = document.createElement("canvas");
    syncCanvas.width = 1;
    syncCanvas.height = 1;
    syncCtx = syncCanvas.getContext("2d");
    if (!syncCtx) {
      syncCanvas = null;
      return;
    }
  }
  if (!syncCtx) {
    syncCanvas = null;
    return;
  }
  syncCtx.drawImage(video, 0, 0, 1, 1);
};
const uploadToTexture = (GL, source, textureID, offsetX, offsetY, alphaOnly) => {
  let renderSource = source instanceof BitmapImage ? source.bitmap : source;
  if (!renderSource) return;
  if (isInstanceOf(renderSource, globalThis.HTMLVideoElement)) {
    syncVideoFrame(renderSource);
  }
  const gl = GL.currentContext?.GLctx;
  gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
  if (alphaOnly) {
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, offsetX, offsetY, gl.RED, gl.UNSIGNED_BYTE, renderSource);
  } else {
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, offsetX, offsetY, gl.RGBA, gl.UNSIGNED_BYTE, renderSource);
  }
  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
};
const setColorSpace = (GL, colorSpace) => {
  if (colorSpace === WindowColorSpace.Others) {
    return false;
  }
  const gl = GL.currentContext?.GLctx;
  if ("drawingBufferColorSpace" in gl) {
    if (colorSpace === WindowColorSpace.None || colorSpace === WindowColorSpace.SRGB) {
      gl.drawingBufferColorSpace = "srgb";
    } else {
      gl.drawingBufferColorSpace = "display-p3";
    }
    return true;
  } else if (colorSpace === WindowColorSpace.DisplayP3) {
    return false;
  } else {
    return true;
  }
};
const isAndroidMiniprogram = () => {
  if (typeof wx !== "undefined" && wx.getSystemInfoSync) {
    return wx.getSystemInfoSync().platform === "android";
  }
};
const releaseNativeImage = (source) => {
  if (isInstanceOf(source, globalThis.ImageBitmap)) {
    source.close();
  } else if (isCanvas(source)) {
    releaseCanvas2D(source);
  }
};
const getBytesFromPath = async (module, path) => {
  const buffer = await fetch(path).then((res) => res.arrayBuffer());
  return new Uint8Array(buffer);
};
const uploadVideoToWebGPUTexture = (source, texturePtr, width, height) => {
  if (!source || !texturePtr) {
    return;
  }
  syncVideoFrame(source);
  const WebGPU = Module.WebGPU;
  if (!WebGPU) {
    return;
  }
  const gpuTexture = WebGPU.mgrTexture.get(texturePtr);
  if (!gpuTexture) {
    return;
  }
  const device = Module.preinitializedWebGPUDevice;
  if (!device || !device.queue) {
    return;
  }
  device.queue.copyExternalImageToTexture(
    { source },
    { texture: gpuTexture, premultipliedAlpha: true },
    [width, height]
  );
};

var tgfx = /*#__PURE__*/Object.freeze({
    __proto__: null,
    createCanvas2D: getCanvas2D,
    createImage: createImage,
    createImageFromBytes: createImageFromBytes,
    getBytesFromPath: getBytesFromPath,
    getSourceSize: getSourceSize,
    hasWebpSupport: hasWebpSupport,
    isAndroidMiniprogram: isAndroidMiniprogram,
    readImagePixels: readImagePixels,
    releaseNativeImage: releaseNativeImage,
    setColorSpace: setColorSpace,
    uploadToTexture: uploadToTexture,
    uploadVideoToWebGPUTexture: uploadVideoToWebGPUTexture
});

function destroyVerify$1(constructor) {
  let functions = Object.getOwnPropertyNames(constructor.prototype).filter(
    (name) => name !== "constructor" && typeof constructor.prototype[name] === "function"
  );
  const proxyFn = (target, methodName) => {
    const fn = target[methodName];
    target[methodName] = function(...args) {
      if (this["isDestroyed"]) {
        console.error(`Don't call ${methodName} of the ${constructor.name} that is destroyed.`);
        return;
      }
      return fn.call(this, ...args);
    };
  };
  functions.forEach((name) => proxyFn(constructor.prototype, name));
}

var __defProp$1 = Object.defineProperty;
var __getOwnPropDesc$1 = Object.getOwnPropertyDescriptor;
var __decorateClass$1 = (decorators, target, key, kind) => {
  var result = kind > 1 ? void 0 : kind ? __getOwnPropDesc$1(target, key) : target;
  for (var i = decorators.length - 1, decorator; i >= 0; i--)
    if (decorator = decorators[i])
      result = (kind ? decorator(target, key, result) : decorator(result)) || result;
  if (kind && result) __defProp$1(target, key, result);
  return result;
};
let Matrix = class {
  constructor(wasmIns) {
    this.isDestroyed = false;
    this.wasmIns = wasmIns;
  }
  /**
   * scaleX; horizontal scale factor to store
   */
  get a() {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.a) : 0;
  }
  set a(value) {
    this.wasmIns?._set(MatrixIndex.a, value);
  }
  /**
   * skewY; vertical skew factor to store
   */
  get b() {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.b) : 0;
  }
  set b(value) {
    this.wasmIns?._set(MatrixIndex.b, value);
  }
  /**
   * skewX; horizontal skew factor to store
   */
  get c() {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.c) : 0;
  }
  set c(value) {
    this.wasmIns?._set(MatrixIndex.c, value);
  }
  /**
   * scaleY; vertical scale factor to store
   */
  get d() {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.d) : 0;
  }
  set d(value) {
    this.wasmIns?._set(MatrixIndex.d, value);
  }
  /**
   * transX; horizontal translation to store
   */
  get tx() {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.tx) : 0;
  }
  set tx(value) {
    this.wasmIns?._set(MatrixIndex.tx, value);
  }
  /**
   * transY; vertical translation to store
   */
  get ty() {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.ty) : 0;
  }
  set ty(value) {
    this.wasmIns?._set(MatrixIndex.ty, value);
  }
  /**
   * Returns one matrix value.
   */
  get(index) {
    return this.wasmIns ? this.wasmIns._get(index) : 0;
  }
  /**
   * Sets Matrix value.
   */
  set(index, value) {
    this.wasmIns?._set(index, value);
  }
  destroy() {
    this.wasmIns.delete();
  }
};
Matrix = __decorateClass$1([
  destroyVerify$1
], Matrix);

const measureText = (imageData) => {
  const imageDataInt32Array = new Int32Array(imageData.data.buffer);
  let left = getLeftPixel(imageDataInt32Array, imageData.width, imageData.height);
  let top = getTopPixel(imageDataInt32Array, imageData.width, imageData.height);
  let right = getRightPixel(imageDataInt32Array, imageData.width, imageData.height);
  let bottom = getBottomPixel(imageDataInt32Array, imageData.width, imageData.height);
  if (left > right || top > bottom) {
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
  }
  return { left, top, right, bottom };
};
const getLeftPixel = (imageDataArray, width, height) => {
  const verticalCount = imageDataArray.length / width;
  const acrossCount = imageDataArray.length / height;
  for (let i = 0; i < acrossCount; i++) {
    for (let j = 0; j < verticalCount; j++) {
      if (imageDataArray[i + j * width] !== 0) return i;
    }
  }
  return acrossCount;
};
const getTopPixel = (imageDataArray, width, height) => {
  const verticalCount = imageDataArray.length / width;
  const acrossCount = imageDataArray.length / height;
  for (let i = 0; i < verticalCount; i++) {
    for (let j = 0; j < acrossCount; j++) {
      if (imageDataArray[i * width + j] !== 0) return i;
    }
  }
  return verticalCount;
};
const getRightPixel = (imageDataArray, width, height) => {
  const verticalCount = imageDataArray.length / width;
  const acrossCount = imageDataArray.length / height;
  for (let i = acrossCount - 1; i > 0; i--) {
    for (let j = verticalCount - 1; j > 0; j--) {
      if (imageDataArray[i + width * j] !== 0) return i;
    }
  }
  return 0;
};
const getBottomPixel = (imageDataArray, width, height) => {
  const verticalCount = imageDataArray.length / width;
  const acrossCount = imageDataArray.length / height;
  for (let i = verticalCount - 1; i > 0; i--) {
    for (let j = acrossCount - 1; j > 0; j--) {
      if (imageDataArray[i * width + j] !== 0) return i;
    }
  }
  return 0;
};

const fontNames = ["Arial", '"Courier New"', "Georgia", '"Times New Roman"', '"Trebuchet MS"', "Verdana"];
const defaultFontNames = (() => {
  return ["emoji"].concat(...fontNames);
})();
const getFontFamilies = (name, style = "") => {
  if (!name) return [];
  const nameChars = name.split(" ");
  let names = [];
  if (nameChars.length === 1) {
    names.push(name);
  } else {
    names.push(nameChars.join(""));
    names.push(nameChars.join(" "));
  }
  const fontFamilies = names.reduce((pre, cur) => {
    if (!style) {
      pre.push(`"${cur}"`);
    } else {
      pre.push(`"${cur} ${style}"`);
      pre.push(`"${cur}-${style}"`);
    }
    return pre;
  }, []);
  if (style !== "") {
    fontFamilies.push(`"${name}"`);
  }
  return fontFamilies;
};

let canvasProvider = {
  getCanvas2D: getCanvas2D,
  releaseCanvas2D: releaseCanvas2D
};
function getCanvasProvider() {
  return canvasProvider;
}

const _ScalerContext = class _ScalerContext {
  constructor(fontName, fontStyle, size) {
    // Cache of the font-specific bounding box probed lazily in the fallback
    // measureText path. Since fontName is readonly on each instance, this is
    // effectively a one-slot memoization and does not need a Map keyed by name.
    this.fontMeasureCache = void 0;
    this.fontName = fontName;
    this.fontStyle = fontStyle;
    this.size = size;
    this.backingSize = Math.min(size, _ScalerContext.MaxCanvasFontSize);
    this.textScale = this.backingSize > 0 ? size / this.backingSize : 1;
    this.loadCanvas();
  }
  static getLineCap(cap) {
    switch (cap) {
      case TGFXModule.TGFXLineCap.Round:
        return "round";
      case TGFXModule.TGFXLineCap.Square:
        return "square";
      default:
        return "butt";
    }
  }
  static getLineJoin(join) {
    switch (join) {
      case TGFXModule.TGFXLineJoin.Round:
        return "round";
      case TGFXModule.TGFXLineJoin.Bevel:
        return "bevel";
      default:
        return "miter";
    }
  }
  static setCanvas(canvas) {
    _ScalerContext.canvas = canvas;
  }
  static setContext(context) {
    _ScalerContext.context = context;
  }
  static isUnicodePropertyEscapeSupported() {
    try {
      new RegExp("\\p{L}", "u");
      return true;
    } catch {
      return false;
    }
  }
  static isEmoji(text) {
    let emojiRegExp;
    if (this.isUnicodePropertyEscapeSupported()) {
      emojiRegExp = /\p{Extended_Pictographic}|[#*0-9]\uFE0F?\u20E3|[\uD83C\uDDE6-\uD83C\uDDFF]/u;
    } else {
      emojiRegExp = /(\u00a9|\u00ae|[\u2000-\u3300]|\ud83c[\ud000-\udfff]|\ud83d[\ud000-\udfff]|\ud83e[\ud000-\udfff])/;
    }
    return emojiRegExp.test(text);
  }
  static measureDirectly(ctx) {
    if (_ScalerContext.hasMeasureBoundsAPI === void 0) {
      const metrics = ctx.measureText("x");
      _ScalerContext.hasMeasureBoundsAPI = metrics && metrics.actualBoundingBoxAscent > 0;
    }
    return _ScalerContext.hasMeasureBoundsAPI;
  }
  fontString(fauxBold, fauxItalic) {
    const attributes = [];
    if (fauxItalic) {
      attributes.push("italic");
    }
    if (fauxBold) {
      attributes.push("bold");
    }
    attributes.push(`${this.backingSize}px`);
    const fallbackFontNames = defaultFontNames.concat();
    fallbackFontNames.unshift(...getFontFamilies(this.fontName, this.fontStyle));
    attributes.push(`${fallbackFontNames.join(",")}`);
    return attributes.join(" ");
  }
  getFontMetrics() {
    if (this.fontMetrics) {
      return this.fontMetrics;
    }
    const { context } = _ScalerContext;
    context.font = this.fontString(false, false);
    const metrics = this.measureText(context, "H");
    const capHeight = metrics.actualBoundingBoxAscent * this.textScale;
    const xMetrics = this.measureText(context, "x");
    const xHeight = xMetrics.actualBoundingBoxAscent * this.textScale;
    this.fontMetrics = {
      ascent: -metrics.fontBoundingBoxAscent * this.textScale,
      descent: metrics.fontBoundingBoxDescent * this.textScale,
      xHeight,
      capHeight
    };
    return this.fontMetrics;
  }
  getBounds(text, fauxBold, fauxItalic) {
    const { context } = _ScalerContext;
    context.font = this.fontString(fauxBold, fauxItalic);
    const metrics = this.measureText(context, text);
    const bounds = {
      left: Math.floor(-metrics.actualBoundingBoxLeft),
      top: Math.floor(-metrics.actualBoundingBoxAscent),
      right: Math.ceil(metrics.actualBoundingBoxRight),
      bottom: Math.ceil(metrics.actualBoundingBoxDescent)
    };
    if (bounds.left >= bounds.right || bounds.top >= bounds.bottom) {
      bounds.left = 0;
      bounds.top = 0;
      bounds.right = 0;
      bounds.bottom = 0;
    }
    const scale = this.textScale;
    if (scale !== 1) {
      bounds.left = Math.floor(bounds.left * scale);
      bounds.top = Math.floor(bounds.top * scale);
      bounds.right = Math.ceil(bounds.right * scale);
      bounds.bottom = Math.ceil(bounds.bottom * scale);
    }
    return bounds;
  }
  getAdvance(text) {
    const { context } = _ScalerContext;
    context.font = this.fontString(false, false);
    return context.measureText(text).width * this.textScale;
  }
  readPixels(text, bounds, fauxBold, stroke) {
    const width = bounds.right - bounds.left;
    const height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) {
      return null;
    }
    const provider = getCanvasProvider();
    const canvas = provider.getCanvas2D(width, height);
    const context = canvas.getContext("2d", { willReadFrequently: true });
    if (!context) {
      provider.releaseCanvas2D(canvas);
      return null;
    }
    context.setTransform(1, 0, 0, 1, 0, 0);
    context.clearRect(0, 0, width, height);
    context.font = this.fontString(fauxBold, false);
    context.setTransform(this.textScale, 0, 0, this.textScale, -bounds.left, -bounds.top);
    if (stroke) {
      context.lineJoin = _ScalerContext.getLineJoin(stroke.join);
      context.miterLimit = stroke.miterLimit;
      context.lineCap = _ScalerContext.getLineCap(stroke.cap);
      context.lineWidth = stroke.width / this.textScale;
      context.strokeText(text, 0, 0);
    } else {
      context.fillText(text, 0, 0);
    }
    context.setTransform(1, 0, 0, 1, 0, 0);
    const { data } = context.getImageData(0, 0, width, height);
    provider.releaseCanvas2D(canvas);
    return new Uint8Array(data);
  }
  getGlyphCanvas(text, bounds, fauxBold, stroke, padding = 0) {
    const glyphWidth = bounds.right - bounds.left;
    const glyphHeight = bounds.bottom - bounds.top;
    const width = glyphWidth + 2 * padding;
    const height = glyphHeight + 2 * padding;
    if (width <= 0 || height <= 0) {
      return null;
    }
    const provider = getCanvasProvider();
    const canvas = provider.getCanvas2D(width, height);
    const context = canvas.getContext("2d");
    if (!context) {
      provider.releaseCanvas2D(canvas);
      return null;
    }
    context.setTransform(1, 0, 0, 1, 0, 0);
    context.clearRect(0, 0, width, height);
    context.font = this.fontString(fauxBold, false);
    context.setTransform(
      this.textScale,
      0,
      0,
      this.textScale,
      padding - bounds.left,
      padding - bounds.top
    );
    if (stroke) {
      context.strokeStyle = "#FFFFFF";
      context.lineJoin = _ScalerContext.getLineJoin(stroke.join);
      context.miterLimit = stroke.miterLimit;
      context.lineCap = _ScalerContext.getLineCap(stroke.cap);
      context.lineWidth = stroke.width / this.textScale;
      context.strokeText(text, 0, 0);
    } else {
      context.fillStyle = "#FFFFFF";
      context.fillText(text, 0, 0);
    }
    context.setTransform(1, 0, 0, 1, 0, 0);
    return canvas;
  }
  /**
   * Lazily initializes the shared canvas and 2D context used for font
   * metrics probing. Called from the constructor, so the first
   * `new ScalerContext(...)` in an environment without a working 2D context
   * will throw and propagate to the WASM bridge. Callers that embed
   * ScalerContext in a host without JS exception handling should ensure the
   * canvas provider is correctly injected before any instance is created.
   */
  loadCanvas() {
    if (!_ScalerContext.canvas) {
      const canvas = getCanvasProvider().getCanvas2D(10, 10);
      const context = canvas.getContext("2d", { willReadFrequently: true });
      if (!context) {
        getCanvasProvider().releaseCanvas2D(canvas);
        throw new Error("[tgfx] Failed to acquire a 2D context for the shared ScalerContext canvas.");
      }
      _ScalerContext.setCanvas(canvas);
      _ScalerContext.setContext(context);
    }
  }
  measureText(ctx, text) {
    if (_ScalerContext.measureDirectly(ctx)) {
      return ctx.measureText(text);
    }
    const side = Math.ceil(this.backingSize * 1.5);
    const provider = getCanvasProvider();
    const tmpCanvas = provider.getCanvas2D(side, side);
    const tmpCtx = tmpCanvas.getContext("2d", { willReadFrequently: true });
    if (!tmpCtx) {
      provider.releaseCanvas2D(tmpCanvas);
      return {
        actualBoundingBoxAscent: 0,
        actualBoundingBoxRight: 0,
        actualBoundingBoxDescent: 0,
        actualBoundingBoxLeft: 0,
        fontBoundingBoxAscent: 0,
        fontBoundingBoxDescent: 0,
        width: 0
      };
    }
    tmpCtx.font = ctx.font;
    const baselineY = this.backingSize;
    tmpCtx.clearRect(0, 0, side, side);
    tmpCtx.fillText(text, 0, baselineY);
    const imageData = tmpCtx.getImageData(0, 0, side, side);
    const { left, top, right, bottom } = measureText(imageData);
    let fontMeasure = this.fontMeasureCache;
    if (!fontMeasure) {
      tmpCtx.clearRect(0, 0, side, side);
      tmpCtx.fillText("\u6D4B", 0, baselineY);
      const fontImageData = tmpCtx.getImageData(0, 0, side, side);
      fontMeasure = measureText(fontImageData);
      this.fontMeasureCache = fontMeasure;
    }
    provider.releaseCanvas2D(tmpCanvas);
    return {
      actualBoundingBoxAscent: baselineY - top,
      actualBoundingBoxRight: right,
      actualBoundingBoxDescent: bottom - baselineY,
      actualBoundingBoxLeft: -left,
      fontBoundingBoxAscent: fontMeasure.bottom - fontMeasure.top,
      fontBoundingBoxDescent: 0,
      width: fontMeasure.right - fontMeasure.left
    };
  }
};
_ScalerContext.hasMeasureBoundsAPI = void 0;
// Browsers may silently clamp very large Canvas font sizes, which makes measureText return
// bounds for the clamped size instead of the requested size. Keep Canvas measurements at a
// conservative backing size and scale all metrics back to the requested size, matching the
// "clamped backing size + extra scale" model used by native font backends.
_ScalerContext.MaxCanvasFontSize = 8192;
let ScalerContext = _ScalerContext;

class PathRasterizer {
  static readPixels(width, height, path, fillType) {
    if (width <= 0 || height <= 0) {
      return null;
    }
    const provider = getCanvasProvider();
    const canvas = provider.getCanvas2D(width, height);
    const context = canvas.getContext("2d", { willReadFrequently: true });
    if (!context) {
      provider.releaseCanvas2D(canvas);
      return null;
    }
    context.setTransform(1, 0, 0, 1, 0, 0);
    if (fillType === TGFXModule.TGFXPathFillType.InverseWinding || fillType === TGFXModule.TGFXPathFillType.InverseEvenOdd) {
      context.clip(path, fillType === TGFXModule.TGFXPathFillType.InverseEvenOdd ? "evenodd" : "nonzero");
      context.fillRect(0, 0, width, height);
    } else {
      context.fill(path, fillType === TGFXModule.TGFXPathFillType.EvenOdd ? "evenodd" : "nonzero");
    }
    const { data } = context.getImageData(0, 0, width, height);
    provider.releaseCanvas2D(canvas);
    return new Uint8Array(data);
  }
}

const TGFXBind = (module) => {
  setTGFXModule(module);
  module.module = module;
  module.ScalerContext = ScalerContext;
  module.PathRasterizer = PathRasterizer;
  module.Matrix = Matrix;
  module.tgfx = { ...tgfx };
};

let pagxModule = null;
function setPAGXModule(module) {
  if (pagxModule !== null && pagxModule !== module) {
    console.warn(
      "setPAGXModule: overwriting an existing PAGX module. Existing PAGXView instances bound to the previous module may behave unexpectedly."
    );
  }
  pagxModule = module;
}
function getPAGXModule() {
  return pagxModule;
}

var PAGXUpdateStrategyRaw = /* @__PURE__ */ ((PAGXUpdateStrategyRaw2) => {
  PAGXUpdateStrategyRaw2[PAGXUpdateStrategyRaw2["NoChange"] = 0] = "NoChange";
  PAGXUpdateStrategyRaw2[PAGXUpdateStrategyRaw2["FullReplace"] = 1] = "FullReplace";
  PAGXUpdateStrategyRaw2[PAGXUpdateStrategyRaw2["Incremental"] = 2] = "Incremental";
  PAGXUpdateStrategyRaw2[PAGXUpdateStrategyRaw2["Error"] = -1] = "Error";
  return PAGXUpdateStrategyRaw2;
})(PAGXUpdateStrategyRaw || {});

function destroyVerify(constructor) {
  const prototype = constructor.prototype;
  const methodNames = Object.getOwnPropertyNames(prototype).filter((name) => {
    if (name === "constructor") return false;
    const descriptor = Object.getOwnPropertyDescriptor(prototype, name);
    return !!descriptor && typeof descriptor.value === "function";
  });
  const proxyFn = (target, methodName) => {
    const fn = target[methodName];
    target[methodName] = function(...args) {
      if (this.isDestroyed) {
        throw new Error(
          `Cannot call ${methodName} of the ${constructor.name} that has been destroyed.`
        );
      }
      return fn.call(this, ...args);
    };
  };
  methodNames.forEach((name) => proxyFn(prototype, name));
}

var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __decorateClass = (decorators, target, key, kind) => {
  var result = kind > 1 ? void 0 : kind ? __getOwnPropDesc(target, key) : target;
  for (var i = decorators.length - 1, decorator; i >= 0; i--)
    if (decorator = decorators[i])
      result = (kind ? decorator(target, key, result) : decorator(result)) || result;
  if (kind && result) __defProp(target, key, result);
  return result;
};
function toPAGXUpdateStrategy(raw) {
  switch (raw) {
    case PAGXUpdateStrategyRaw.NoChange:
      return "noChange";
    case PAGXUpdateStrategyRaw.FullReplace:
      return "fullReplace";
    case PAGXUpdateStrategyRaw.Incremental:
      return "incremental";
    default:
      return "error";
  }
}
let PAGXView = class {
  constructor(nativeView) {
    this._isRunning = false;
    this._isDestroyed = false;
    this.animationFrameId = null;
    this.nativeView = nativeView;
  }
  /**
   * Creates a PAGXView from a canvas element or a CSS selector.
   * @param canvas Either a CSS selector string (e.g., `'#my-canvas'`) or an `HTMLCanvasElement`.
   *               When an element is passed it must have a non-empty `id` attribute; the id is
   *               used to build the CSS selector required by the underlying WebGL binding.
   * @returns PAGXView instance or null if creation failed
   */
  static init(canvas) {
    const module = getPAGXModule();
    if (!module) {
      console.error("PAGXView: Module not initialized. Call PAGXInit() first.");
      return null;
    }
    let canvasID;
    if (typeof canvas === "string") {
      if (!canvas) {
        console.error("PAGXView: canvas selector must be a non-empty string.");
        return null;
      }
      canvasID = canvas;
    } else {
      if (!canvas.id) {
        console.error("PAGXView: Canvas element must have a non-empty id attribute.");
        return null;
      }
      canvasID = `#${canvas.id}`;
    }
    const nativeView = module._PAGXView._MakeFrom(canvasID);
    if (!nativeView) {
      console.error(`PAGXView: Failed to create PAGXView from canvas "${canvasID}".`);
      return null;
    }
    return new PAGXView(nativeView);
  }
  /**
   * Whether the render loop is running.
   */
  get isRunning() {
    return this._isRunning;
  }
  /**
   * Whether the view has been destroyed.
   */
  get isDestroyed() {
    return this._isDestroyed;
  }
  /**
   * The original width of the PAGX content.
   */
  get contentWidth() {
    return this.nativeView._contentWidth();
  }
  /**
   * The original height of the PAGX content.
   */
  get contentHeight() {
    return this.nativeView._contentHeight();
  }
  /**
   * Registers fallback fonts for text rendering.
   * @param fontData Primary font data (e.g., NotoSansSC)
   * @param emojiFontData Emoji font data (e.g., NotoColorEmoji)
   */
  registerFonts(fontData, emojiFontData) {
    this.nativeView._registerFonts(fontData, emojiFontData);
  }
  /**
   * Loads a PAGX document (no external files).
   * @param data PAGX file binary data
   */
  loadPAGX(data) {
    this.nativeView._loadPAGX(data);
  }
  /**
   * Clears the currently loaded PAGX content and releases associated resources. After this call
   * the view will render as empty until a new document is loaded.
   */
  clear() {
    this.nativeView._loadPAGX(new Uint8Array(0));
  }
  /**
   * Parses PAGX data without building layers.
   * Call getExternalFilePaths() and loadFileData() after this to load external resources,
   * then call buildLayers() to complete the loading.
   * @param data PAGX file binary data
   */
  parsePAGX(data) {
    this.nativeView._parsePAGX(data);
  }
  /**
   * Applies new PAGX bytes to the view through the V0 incremental-update pipeline. See
   * PAGXView::updatePAGX in C++ for the full contract. Summary of the returned `strategy`:
   *
   * - `noChange`: bytes are byte-identical to the previously accepted document. Nothing was
   *   touched; the caller can skip re-resolving external resources and re-drawing.
   * - `fullReplace`: the live document was replaced. The caller must then call
   *   {@link getExternalFilePaths} + {@link loadFileData} for any external references and
   *   finally {@link buildLayers} + {@link draw}, mirroring the sequence used after
   *   {@link parsePAGX}. V0 always takes this branch on the non-short-circuited path.
   * - `incremental`: reserved for V1+; V0 never returns this.
   * - `error`: candidate parsing failed. The live document was left untouched, so the previous
   *   frame keeps rendering; `errorMessage` carries a short reason.
   *
   * @param data PAGX file binary data.
   * @param forceFullReplace forwarded to native; ignored in V0 (kept for JS contract stability).
   */
  updatePAGX(data, forceFullReplace = false) {
    const raw = this.nativeView._updatePAGX(data, forceFullReplace);
    return {
      strategy: toPAGXUpdateStrategy(raw.strategy),
      dirtyCount: raw.dirtyCount,
      totalCount: raw.totalCount,
      errorMessage: raw.errorMessage
    };
  }
  /**
   * Applies a batch of channel edits to the live document in a single transaction. The scene and
   * animation state are preserved — no buildLayers / PAGScene::Make rebuild occurs. See
   * PAGXView::applyPatch in C++ for the full contract.
   *
   * Requires a loaded document and built scene; callers should have called loadPAGX or
   * updatePAGX first.
   */
  applyPatch(ops) {
    const raw = this.nativeView._applyPatch(ops);
    return {
      success: raw.success,
      applied: raw.applied,
      failed: raw.failed,
      errorMessage: raw.errorMessage
    };
  }
  /**
   * Returns the list of external file paths referenced by the PAGX document.
   */
  getExternalFilePaths() {
    const vector = this.nativeView._getExternalFilePaths();
    const paths = [];
    try {
      const count = vector.size();
      for (let i = 0; i < count; i++) {
        paths.push(vector.get(i));
      }
    } finally {
      vector.delete();
    }
    return paths;
  }
  /**
   * Loads external file data referenced by the PAGX document.
   * @param path The file path as returned by getExternalFilePaths()
   * @param data The file binary data
   * @returns true if successful
   */
  loadFileData(path, data) {
    return this.nativeView._loadFileData(path, data);
  }
  /**
   * Builds the layer tree after parsing and loading external files.
   * Must be called after parsePAGX() and any loadFileData() calls.
   */
  buildLayers() {
    this.nativeView._buildLayers();
  }
  /**
   * Updates the view size to match the canvas dimensions.
   * Call this after canvas resize.
   */
  updateSize() {
    this.nativeView._updateSize();
  }
  /**
   * Updates the zoom scale and content offset.
   * @param zoom Zoom scale (1.0 = 100%)
   * @param offsetX Horizontal offset in pixels
   * @param offsetY Vertical offset in pixels
   */
  updateZoomScaleAndOffset(zoom, offsetX, offsetY) {
    this.nativeView._updateZoomScaleAndOffset(zoom, offsetX, offsetY);
  }
  /**
   * Sets a solid background color. When set, the solid color will be used instead of the default
   * checkerboard pattern.
   * @param color Hex color string (#RGB, #RRGGBB, #RGBA, or #RRGGBBAA)
   * @param alpha Optional alpha override (0.0 - 1.0). If provided, overrides any alpha in the color string.
   *
   * @example
   * ```typescript
   * view.setBackgroundColor('#ff0000');       // Opaque red
   * view.setBackgroundColor('#f00');          // Opaque red (shorthand)
   * view.setBackgroundColor('#ff000080');     // 50% transparent red
   * view.setBackgroundColor('#ffffff', 0.5);  // 50% transparent white
   * view.setBackgroundColor('#fff', 0.8);     // 80% opaque white
   * ```
   */
  setBackgroundColor(color, alpha) {
    let parsed = PAGXView.parseColor(color);
    if (!parsed) {
      console.warn(`PAGXView: Invalid color format "${color}". Use #RGB, #RGBA, #RRGGBB, or #RRGGBBAA. Falling back to opaque white.`);
      parsed = { r: 1, g: 1, b: 1, a: 1 };
    }
    const finalAlpha = alpha !== void 0 ? alpha : parsed.a;
    this.nativeView._setBackgroundColor(parsed.r, parsed.g, parsed.b, finalAlpha);
  }
  /**
   * Clears the custom background color and reverts to the default checkerboard pattern.
   */
  clearBackgroundColor() {
    this.nativeView._clearBackgroundColor();
  }
  /**
   * Parses a hex color string into RGBA components (0.0 - 1.0).
   * Supports #RGB, #RGBA, #RRGGBB, #RRGGBBAA formats.
   */
  static parseColor(color) {
    if (!color.startsWith("#")) {
      return null;
    }
    const hex = color.slice(1);
    let r, g, b, a;
    if (hex.length === 3) {
      r = parseInt(hex[0] + hex[0], 16) / 255;
      g = parseInt(hex[1] + hex[1], 16) / 255;
      b = parseInt(hex[2] + hex[2], 16) / 255;
      a = 1;
    } else if (hex.length === 4) {
      r = parseInt(hex[0] + hex[0], 16) / 255;
      g = parseInt(hex[1] + hex[1], 16) / 255;
      b = parseInt(hex[2] + hex[2], 16) / 255;
      a = parseInt(hex[3] + hex[3], 16) / 255;
    } else if (hex.length === 6) {
      r = parseInt(hex.slice(0, 2), 16) / 255;
      g = parseInt(hex.slice(2, 4), 16) / 255;
      b = parseInt(hex.slice(4, 6), 16) / 255;
      a = 1;
    } else if (hex.length === 8) {
      r = parseInt(hex.slice(0, 2), 16) / 255;
      g = parseInt(hex.slice(2, 4), 16) / 255;
      b = parseInt(hex.slice(4, 6), 16) / 255;
      a = parseInt(hex.slice(6, 8), 16) / 255;
    } else {
      return null;
    }
    if (isNaN(r) || isNaN(g) || isNaN(b) || isNaN(a)) {
      return null;
    }
    return { r, g, b, a };
  }
  /**
   * Draws the current frame immediately.
   */
  draw() {
    this.nativeView._draw();
  }
  // Playback control methods
  /**
   * Starts or resumes playback of the default timeline.
   */
  play() {
    this.nativeView._play();
  }
  /**
   * Pauses playback of the default timeline.
   */
  pause() {
    this.nativeView._pause();
  }
  /**
   * Returns whether the default timeline is currently playing.
   */
  isPlaying() {
    return this.nativeView._isPlaying();
  }
  /**
   * Returns the current playback time in microseconds.
   */
  currentTimeMicros() {
    return this.nativeView._currentTimeMicros();
  }
  /**
   * Returns the total duration in microseconds. Returns 0 if no content is loaded.
   */
  durationMicros() {
    return this.nativeView._durationMicros();
  }
  /**
   * Returns the frame rate of the animation. Returns 0 if no content is loaded.
   */
  frameRate() {
    return this.nativeView._frameRate();
  }
  /**
   * Sets the current playback time in microseconds.
   * @param micros Time position in microseconds
   */
  setCurrentTimeMicros(micros) {
    this.nativeView._setCurrentTimeMicros(micros);
    if (!this._isRunning) {
      this.nativeView._draw();
    }
  }
  /**
   * Sets whether playback loops, overriding the loop mode stored in the file. When enabled, each
   * cycle repeats (preserving PingPong mirroring); when disabled, playback rewinds to the first
   * frame and stops after a single pass.
   * @param loop true to loop, false to play once
   */
  setLoop(loop) {
    this.nativeView._setLoop(loop);
  }
  /**
   * Returns whether playback is set to loop.
   */
  isLoop() {
    return this.nativeView._isLoop();
  }
  /**
   * Starts the render loop.
   */
  start() {
    if (this._isRunning) return;
    this._isRunning = true;
    this.renderLoop();
  }
  /**
   * Stops the render loop.
   */
  stop() {
    if (!this._isRunning) return;
    this._isRunning = false;
    if (this.animationFrameId !== null) {
      cancelAnimationFrame(this.animationFrameId);
      this.animationFrameId = null;
    }
  }
  /**
   * Destroys the view and releases resources.
   */
  destroy() {
    this.stop();
    this.nativeView.delete();
    this._isDestroyed = true;
  }
  renderLoop() {
    if (!this._isRunning) return;
    this.nativeView._draw();
    this.animationFrameId = requestAnimationFrame(() => this.renderLoop());
  }
};
PAGXView = __decorateClass([
  destroyVerify
], PAGXView);

function PAGXBind(module) {
  TGFXBind(module);
  setPAGXModule(module);
  module.PAGXView = PAGXView;
}

var PAGXViewerWasm = (() => {
  var _scriptName = import.meta.url;
  return (function(moduleArg = {}) {
    var moduleRtn;
    var Module = Object.assign({}, moduleArg);
    var readyPromiseResolve, readyPromiseReject;
    var readyPromise = new Promise((resolve, reject) => {
      readyPromiseResolve = resolve;
      readyPromiseReject = reject;
    });
    ["_memory", "___indirect_function_table", "onRuntimeInitialized"].forEach((prop) => {
      if (!Object.getOwnPropertyDescriptor(readyPromise, prop)) {
        Object.defineProperty(readyPromise, prop, {
          get: () => abort("You are getting " + prop + " on the Promise object, instead of the instance. Use .then() to get called back with the instance, see the MODULARIZE docs in src/settings.js"),
          set: () => abort("You are setting " + prop + " on the Promise object, instead of the instance. Use .then() to get called back with the instance, see the MODULARIZE docs in src/settings.js")
        });
      }
    });
    var ENVIRONMENT_IS_WEB = typeof window == "object";
    var ENVIRONMENT_IS_WORKER = typeof importScripts == "function";
    var ENVIRONMENT_IS_NODE = typeof process == "object" && typeof process.versions == "object" && typeof process.versions.node == "string";
    var ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;
    if (Module["ENVIRONMENT"]) {
      throw new Error("Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)");
    }
    var moduleOverrides = Object.assign({}, Module);
    var thisProgram = "./this.program";
    var quit_ = (status, toThrow) => {
      throw toThrow;
    };
    var scriptDirectory = "";
    function locateFile(path) {
      if (Module["locateFile"]) {
        return Module["locateFile"](path, scriptDirectory);
      }
      return scriptDirectory + path;
    }
    var read_, readAsync, readBinary;
    if (ENVIRONMENT_IS_SHELL) {
      if (typeof process == "object" && typeof require === "function" || typeof window == "object" || typeof importScripts == "function") throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    } else if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
      if (ENVIRONMENT_IS_WORKER) {
        scriptDirectory = self.location.href;
      } else if (typeof document != "undefined" && document.currentScript) {
        scriptDirectory = document.currentScript.src;
      }
      if (_scriptName) {
        scriptDirectory = _scriptName;
      }
      if (scriptDirectory.startsWith("blob:")) {
        scriptDirectory = "";
      } else {
        scriptDirectory = scriptDirectory.substr(0, scriptDirectory.replace(/[?#].*/, "").lastIndexOf("/") + 1);
      }
      if (!(typeof window == "object" || typeof importScripts == "function")) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
      {
        read_ = (url) => {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", url, false);
          xhr.send(null);
          return xhr.responseText;
        };
        if (ENVIRONMENT_IS_WORKER) {
          readBinary = (url) => {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", url, false);
            xhr.responseType = "arraybuffer";
            xhr.send(null);
            return new Uint8Array(
              /** @type{!ArrayBuffer} */
              xhr.response
            );
          };
        }
        readAsync = (url, onload, onerror) => {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", url, true);
          xhr.responseType = "arraybuffer";
          xhr.onload = () => {
            if (xhr.status == 200 || xhr.status == 0 && xhr.response) {
              onload(xhr.response);
              return;
            }
            onerror();
          };
          xhr.onerror = onerror;
          xhr.send(null);
        };
      }
    } else {
      throw new Error("environment detection error");
    }
    var out = Module["print"] || console.log.bind(console);
    var err = Module["printErr"] || console.error.bind(console);
    Object.assign(Module, moduleOverrides);
    moduleOverrides = null;
    checkIncomingModuleAPI();
    if (Module["arguments"]) Module["arguments"];
    legacyModuleProp("arguments", "arguments_");
    if (Module["thisProgram"]) thisProgram = Module["thisProgram"];
    legacyModuleProp("thisProgram", "thisProgram");
    if (Module["quit"]) quit_ = Module["quit"];
    legacyModuleProp("quit", "quit_");
    assert(typeof Module["memoryInitializerPrefixURL"] == "undefined", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead");
    assert(typeof Module["pthreadMainPrefixURL"] == "undefined", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead");
    assert(typeof Module["cdInitializerPrefixURL"] == "undefined", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead");
    assert(typeof Module["filePackagePrefixURL"] == "undefined", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead");
    assert(typeof Module["read"] == "undefined", "Module.read option was removed (modify read_ in JS)");
    assert(typeof Module["readAsync"] == "undefined", "Module.readAsync option was removed (modify readAsync in JS)");
    assert(typeof Module["readBinary"] == "undefined", "Module.readBinary option was removed (modify readBinary in JS)");
    assert(typeof Module["setWindowTitle"] == "undefined", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)");
    assert(typeof Module["TOTAL_MEMORY"] == "undefined", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY");
    legacyModuleProp("asm", "wasmExports");
    legacyModuleProp("read", "read_");
    legacyModuleProp("readAsync", "readAsync");
    legacyModuleProp("readBinary", "readBinary");
    legacyModuleProp("setWindowTitle", "setWindowTitle");
    assert(!ENVIRONMENT_IS_NODE, "node environment detected but not enabled at build time.  Add `node` to `-sENVIRONMENT` to enable.");
    assert(!ENVIRONMENT_IS_SHELL, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
    var wasmBinary;
    if (Module["wasmBinary"]) wasmBinary = Module["wasmBinary"];
    legacyModuleProp("wasmBinary", "wasmBinary");
    if (typeof WebAssembly != "object") {
      err("no native wasm support detected");
    }
    function getSafeHeapType(bytes, isFloat) {
      switch (bytes) {
        case 1:
          return "i8";
        case 2:
          return "i16";
        case 4:
          return isFloat ? "float" : "i32";
        case 8:
          return isFloat ? "double" : "i64";
        default:
          abort(`getSafeHeapType() invalid bytes=${bytes}`);
      }
    }
    function SAFE_HEAP_STORE(dest, value, bytes, isFloat) {
      if (dest <= 0) abort(`segmentation fault storing ${bytes} bytes to address ${dest}`);
      if (dest % bytes !== 0) abort(`alignment error storing to address ${dest}, which was expected to be aligned to a multiple of ${bytes}`);
      if (runtimeInitialized) {
        var brk = _sbrk(0);
        if (dest + bytes > brk) abort(`segmentation fault, exceeded the top of the available dynamic heap when storing ${bytes} bytes to address ${dest}. DYNAMICTOP=${brk}`);
        if (brk < _emscripten_stack_get_base()) abort(`brk >= _emscripten_stack_get_base() (brk=${brk}, _emscripten_stack_get_base()=${_emscripten_stack_get_base()})`);
        if (brk > wasmMemory.buffer.byteLength) abort(`brk <= wasmMemory.buffer.byteLength (brk=${brk}, wasmMemory.buffer.byteLength=${wasmMemory.buffer.byteLength})`);
      }
      setValue_safe(dest, value, getSafeHeapType(bytes, isFloat));
      return value;
    }
    function SAFE_HEAP_STORE_D(dest, value, bytes) {
      return SAFE_HEAP_STORE(dest, value, bytes, true);
    }
    function SAFE_HEAP_LOAD(dest, bytes, unsigned, isFloat) {
      if (dest <= 0) abort(`segmentation fault loading ${bytes} bytes from address ${dest}`);
      if (dest % bytes !== 0) abort(`alignment error loading from address ${dest}, which was expected to be aligned to a multiple of ${bytes}`);
      if (runtimeInitialized) {
        var brk = _sbrk(0);
        if (dest + bytes > brk) abort(`segmentation fault, exceeded the top of the available dynamic heap when loading ${bytes} bytes from address ${dest}. DYNAMICTOP=${brk}`);
        if (brk < _emscripten_stack_get_base()) abort(`brk >= _emscripten_stack_get_base() (brk=${brk}, _emscripten_stack_get_base()=${_emscripten_stack_get_base()})`);
        if (brk > wasmMemory.buffer.byteLength) abort(`brk <= wasmMemory.buffer.byteLength (brk=${brk}, wasmMemory.buffer.byteLength=${wasmMemory.buffer.byteLength})`);
      }
      var type = getSafeHeapType(bytes, isFloat);
      var ret = getValue_safe(dest, type);
      if (unsigned) ret = unSign(ret, parseInt(type.substr(1), 10));
      return ret;
    }
    function SAFE_HEAP_LOAD_D(dest, bytes, unsigned) {
      return SAFE_HEAP_LOAD(dest, bytes, unsigned, true);
    }
    function segfault() {
      abort("segmentation fault");
    }
    function alignfault() {
      abort("alignment fault");
    }
    var wasmMemory;
    var ABORT = false;
    function assert(condition, text) {
      if (!condition) {
        abort("Assertion failed" + (text ? ": " + text : ""));
      }
    }
    var HEAP8, HEAPU8, HEAP16, HEAPU16, HEAP32, HEAPU32, HEAPF32, HEAPF64;
    function updateMemoryViews() {
      var b = wasmMemory.buffer;
      Module["HEAP8"] = HEAP8 = new Int8Array(b);
      Module["HEAP16"] = HEAP16 = new Int16Array(b);
      Module["HEAPU8"] = HEAPU8 = new Uint8Array(b);
      Module["HEAPU16"] = HEAPU16 = new Uint16Array(b);
      Module["HEAP32"] = HEAP32 = new Int32Array(b);
      Module["HEAPU32"] = HEAPU32 = new Uint32Array(b);
      Module["HEAPF32"] = HEAPF32 = new Float32Array(b);
      Module["HEAPF64"] = HEAPF64 = new Float64Array(b);
    }
    assert(!Module["STACK_SIZE"], "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time");
    assert(typeof Int32Array != "undefined" && typeof Float64Array !== "undefined" && Int32Array.prototype.subarray != void 0 && Int32Array.prototype.set != void 0, "JS engine does not provide full typed array support");
    assert(!Module["wasmMemory"], "Use of `wasmMemory` detected.  Use -sIMPORTED_MEMORY to define wasmMemory externally");
    assert(!Module["INITIAL_MEMORY"], "Detected runtime INITIAL_MEMORY setting.  Use -sIMPORTED_MEMORY to define wasmMemory dynamically");
    function writeStackCookie() {
      var max = _emscripten_stack_get_end();
      assert((max & 3) == 0);
      if (max == 0) {
        max += 4;
      }
      SAFE_HEAP_STORE((max >> 2) * 4, 34821223, 4);
      SAFE_HEAP_STORE((max + 4 >> 2) * 4, 2310721022, 4);
    }
    function checkStackCookie() {
      if (ABORT) return;
      var max = _emscripten_stack_get_end();
      if (max == 0) {
        max += 4;
      }
      var cookie1 = SAFE_HEAP_LOAD((max >> 2) * 4, 4, 1);
      var cookie2 = SAFE_HEAP_LOAD((max + 4 >> 2) * 4, 4, 1);
      if (cookie1 != 34821223 || cookie2 != 2310721022) {
        abort(`Stack overflow! Stack cookie has been overwritten at ${ptrToString(max)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${ptrToString(cookie2)} ${ptrToString(cookie1)}`);
      }
    }
    (function() {
      var h16 = new Int16Array(1);
      var h8 = new Int8Array(h16.buffer);
      h16[0] = 25459;
      if (h8[0] !== 115 || h8[1] !== 99) throw "Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)";
    })();
    var __ATPRERUN__ = [];
    var __ATINIT__ = [];
    var __ATPOSTRUN__ = [];
    var runtimeInitialized = false;
    function preRun() {
      if (Module["preRun"]) {
        if (typeof Module["preRun"] == "function") Module["preRun"] = [Module["preRun"]];
        while (Module["preRun"].length) {
          addOnPreRun(Module["preRun"].shift());
        }
      }
      callRuntimeCallbacks(__ATPRERUN__);
    }
    function initRuntime() {
      assert(!runtimeInitialized);
      runtimeInitialized = true;
      checkStackCookie();
      if (!Module["noFSInit"] && !FS.init.initialized) FS.init();
      FS.ignorePermissions = false;
      callRuntimeCallbacks(__ATINIT__);
    }
    function postRun() {
      checkStackCookie();
      if (Module["postRun"]) {
        if (typeof Module["postRun"] == "function") Module["postRun"] = [Module["postRun"]];
        while (Module["postRun"].length) {
          addOnPostRun(Module["postRun"].shift());
        }
      }
      callRuntimeCallbacks(__ATPOSTRUN__);
    }
    function addOnPreRun(cb) {
      __ATPRERUN__.unshift(cb);
    }
    function addOnInit(cb) {
      __ATINIT__.unshift(cb);
    }
    function addOnPostRun(cb) {
      __ATPOSTRUN__.unshift(cb);
    }
    assert(Math.imul, "This browser does not support Math.imul(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill");
    assert(Math.fround, "This browser does not support Math.fround(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill");
    assert(Math.clz32, "This browser does not support Math.clz32(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill");
    assert(Math.trunc, "This browser does not support Math.trunc(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill");
    var runDependencies = 0;
    var runDependencyWatcher = null;
    var dependenciesFulfilled = null;
    var runDependencyTracking = {};
    function getUniqueRunDependency(id) {
      var orig = id;
      while (1) {
        if (!runDependencyTracking[id]) return id;
        id = orig + Math.random();
      }
    }
    function addRunDependency(id) {
      runDependencies++;
      Module["monitorRunDependencies"]?.(runDependencies);
      if (id) {
        assert(!runDependencyTracking[id]);
        runDependencyTracking[id] = 1;
        if (runDependencyWatcher === null && typeof setInterval != "undefined") {
          runDependencyWatcher = setInterval(() => {
            if (ABORT) {
              clearInterval(runDependencyWatcher);
              runDependencyWatcher = null;
              return;
            }
            var shown = false;
            for (var dep in runDependencyTracking) {
              if (!shown) {
                shown = true;
                err("still waiting on run dependencies:");
              }
              err(`dependency: ${dep}`);
            }
            if (shown) {
              err("(end of list)");
            }
          }, 1e4);
        }
      } else {
        err("warning: run dependency added without ID");
      }
    }
    function removeRunDependency(id) {
      runDependencies--;
      Module["monitorRunDependencies"]?.(runDependencies);
      if (id) {
        assert(runDependencyTracking[id]);
        delete runDependencyTracking[id];
      } else {
        err("warning: run dependency removed without ID");
      }
      if (runDependencies == 0) {
        if (runDependencyWatcher !== null) {
          clearInterval(runDependencyWatcher);
          runDependencyWatcher = null;
        }
        if (dependenciesFulfilled) {
          var callback = dependenciesFulfilled;
          dependenciesFulfilled = null;
          callback();
        }
      }
    }
    function abort(what) {
      Module["onAbort"]?.(what);
      what = "Aborted(" + what + ")";
      err(what);
      ABORT = true;
      var e = new WebAssembly.RuntimeError(what);
      readyPromiseReject(e);
      throw e;
    }
    var dataURIPrefix = "data:application/octet-stream;base64,";
    var isDataURI = (filename) => filename.startsWith(dataURIPrefix);
    var isFileURI = (filename) => filename.startsWith("file://");
    function createExportWrapper(name, nargs) {
      return (...args) => {
        assert(runtimeInitialized, `native function \`${name}\` called before runtime initialization`);
        var f = wasmExports[name];
        assert(f, `exported native function \`${name}\` not found`);
        assert(args.length <= nargs, `native function \`${name}\` called with ${args.length} args but expects ${nargs}`);
        return f(...args);
      };
    }
    var wasmBinaryFile;
    if (Module["locateFile"]) {
      wasmBinaryFile = "pagx-viewer.st.wasm";
      if (!isDataURI(wasmBinaryFile)) {
        wasmBinaryFile = locateFile(wasmBinaryFile);
      }
    } else {
      wasmBinaryFile = new URL("pagx-viewer.st.wasm", import.meta.url).href;
    }
    function getBinarySync(file) {
      if (file == wasmBinaryFile && wasmBinary) {
        return new Uint8Array(wasmBinary);
      }
      if (readBinary) {
        return readBinary(file);
      }
      throw "both async and sync fetching of the wasm failed";
    }
    function getBinaryPromise(binaryFile) {
      if (!wasmBinary && (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER)) {
        if (typeof fetch == "function") {
          return fetch(binaryFile, {
            credentials: "same-origin"
          }).then((response) => {
            if (!response["ok"]) {
              throw `failed to load wasm binary file at '${binaryFile}'`;
            }
            return response["arrayBuffer"]();
          }).catch(() => getBinarySync(binaryFile));
        }
      }
      return Promise.resolve().then(() => getBinarySync(binaryFile));
    }
    function instantiateArrayBuffer(binaryFile, imports, receiver) {
      return getBinaryPromise(binaryFile).then((binary) => WebAssembly.instantiate(binary, imports)).then(receiver, (reason) => {
        err(`failed to asynchronously prepare wasm: ${reason}`);
        if (isFileURI(wasmBinaryFile)) {
          err(`warning: Loading from a file URI (${wasmBinaryFile}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`);
        }
        abort(reason);
      });
    }
    function instantiateAsync(binary, binaryFile, imports, callback) {
      if (!binary && typeof WebAssembly.instantiateStreaming == "function" && !isDataURI(binaryFile) && typeof fetch == "function") {
        return fetch(binaryFile, {
          credentials: "same-origin"
        }).then((response) => {
          var result = WebAssembly.instantiateStreaming(response, imports);
          return result.then(callback, function(reason) {
            err(`wasm streaming compile failed: ${reason}`);
            err("falling back to ArrayBuffer instantiation");
            return instantiateArrayBuffer(binaryFile, imports, callback);
          });
        });
      }
      return instantiateArrayBuffer(binaryFile, imports, callback);
    }
    function getWasmImports() {
      return {
        "env": wasmImports,
        "wasi_snapshot_preview1": wasmImports
      };
    }
    function createWasm() {
      var info = getWasmImports();
      function receiveInstance(instance, module) {
        wasmExports = instance.exports;
        wasmMemory = wasmExports["memory"];
        assert(wasmMemory, "memory not found in wasm exports");
        updateMemoryViews();
        wasmTable = wasmExports["__indirect_function_table"];
        assert(wasmTable, "table not found in wasm exports");
        addOnInit(wasmExports["__wasm_call_ctors"]);
        removeRunDependency("wasm-instantiate");
        return wasmExports;
      }
      addRunDependency("wasm-instantiate");
      var trueModule = Module;
      function receiveInstantiationResult(result) {
        assert(Module === trueModule, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?");
        trueModule = null;
        receiveInstance(result["instance"]);
      }
      if (Module["instantiateWasm"]) {
        try {
          return Module["instantiateWasm"](info, receiveInstance);
        } catch (e) {
          err(`Module.instantiateWasm callback failed with error: ${e}`);
          readyPromiseReject(e);
        }
      }
      instantiateAsync(wasmBinary, wasmBinaryFile, info, receiveInstantiationResult).catch(readyPromiseReject);
      return {};
    }
    var tempDouble;
    var tempI64;
    function legacyModuleProp(prop, newName, incoming = true) {
      if (!Object.getOwnPropertyDescriptor(Module, prop)) {
        Object.defineProperty(Module, prop, {
          configurable: true,
          get() {
            let extra = incoming ? " (the initial value can be provided on Module, but after startup the value is only looked for on a local variable of that name)" : "";
            abort(`\`Module.${prop}\` has been replaced by \`${newName}\`` + extra);
          }
        });
      }
    }
    function ignoredModuleProp(prop) {
      if (Object.getOwnPropertyDescriptor(Module, prop)) {
        abort(`\`Module.${prop}\` was supplied but \`${prop}\` not included in INCOMING_MODULE_JS_API`);
      }
    }
    function isExportedByForceFilesystem(name) {
      return name === "FS_createPath" || name === "FS_createDataFile" || name === "FS_createPreloadedFile" || name === "FS_unlink" || name === "addRunDependency" || name === "FS_createLazyFile" || name === "FS_createDevice" || name === "removeRunDependency";
    }
    function missingGlobal(sym, msg) {
      if (typeof globalThis != "undefined") {
        Object.defineProperty(globalThis, sym, {
          configurable: true,
          get() {
            warnOnce(`\`${sym}\` is not longer defined by emscripten. ${msg}`);
            return void 0;
          }
        });
      }
    }
    missingGlobal("buffer", "Please use HEAP8.buffer or wasmMemory.buffer");
    missingGlobal("asm", "Please use wasmExports instead");
    function missingLibrarySymbol(sym) {
      if (typeof globalThis != "undefined" && !Object.getOwnPropertyDescriptor(globalThis, sym)) {
        Object.defineProperty(globalThis, sym, {
          configurable: true,
          get() {
            var msg = `\`${sym}\` is a library symbol and not included by default; add it to your library.js __deps or to DEFAULT_LIBRARY_FUNCS_TO_INCLUDE on the command line`;
            var librarySymbol = sym;
            if (!librarySymbol.startsWith("_")) {
              librarySymbol = "$" + sym;
            }
            msg += ` (e.g. -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='${librarySymbol}')`;
            if (isExportedByForceFilesystem(sym)) {
              msg += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you";
            }
            warnOnce(msg);
            return void 0;
          }
        });
      }
      unexportedRuntimeSymbol(sym);
    }
    function unexportedRuntimeSymbol(sym) {
      if (!Object.getOwnPropertyDescriptor(Module, sym)) {
        Object.defineProperty(Module, sym, {
          configurable: true,
          get() {
            var msg = `'${sym}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
            if (isExportedByForceFilesystem(sym)) {
              msg += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you";
            }
            abort(msg);
          }
        });
      }
    }
    function ExitStatus(status) {
      this.name = "ExitStatus";
      this.message = `Program terminated with exit(${status})`;
      this.status = status;
    }
    var callRuntimeCallbacks = (callbacks) => {
      while (callbacks.length > 0) {
        callbacks.shift()(Module);
      }
    };
    function getValue_safe(ptr, type = "i8") {
      if (type.endsWith("*")) type = "*";
      switch (type) {
        case "i1":
          return HEAP8[ptr];
        case "i8":
          return HEAP8[ptr];
        case "i16":
          return HEAP16[ptr >> 1];
        case "i32":
          return HEAP32[ptr >> 2];
        case "i64":
          abort("to do getValue(i64) use WASM_BIGINT");
        case "float":
          return HEAPF32[ptr >> 2];
        case "double":
          return HEAPF64[ptr >> 3];
        case "*":
          return HEAPU32[ptr >> 2];
        default:
          abort(`invalid type for getValue: ${type}`);
      }
    }
    var noExitRuntime = Module["noExitRuntime"] || true;
    var ptrToString = (ptr) => {
      assert(typeof ptr === "number");
      ptr >>>= 0;
      return "0x" + ptr.toString(16).padStart(8, "0");
    };
    function setValue_safe(ptr, value, type = "i8") {
      if (type.endsWith("*")) type = "*";
      switch (type) {
        case "i1":
          HEAP8[ptr] = value;
          break;
        case "i8":
          HEAP8[ptr] = value;
          break;
        case "i16":
          HEAP16[ptr >> 1] = value;
          break;
        case "i32":
          HEAP32[ptr >> 2] = value;
          break;
        case "i64":
          abort("to do setValue(i64) use WASM_BIGINT");
        case "float":
          HEAPF32[ptr >> 2] = value;
          break;
        case "double":
          HEAPF64[ptr >> 3] = value;
          break;
        case "*":
          HEAPU32[ptr >> 2] = value;
          break;
        default:
          abort(`invalid type for setValue: ${type}`);
      }
    }
    var stackRestore = (val) => __emscripten_stack_restore(val);
    var stackSave = () => _emscripten_stack_get_current();
    var unSign = (value, bits) => {
      if (value >= 0) {
        return value;
      }
      return bits <= 32 ? 2 * Math.abs(1 << bits - 1) + value : Math.pow(2, bits) + value;
    };
    var warnOnce = (text) => {
      warnOnce.shown || (warnOnce.shown = {});
      if (!warnOnce.shown[text]) {
        warnOnce.shown[text] = 1;
        err(text);
      }
    };
    var UTF8Decoder = typeof TextDecoder != "undefined" ? new TextDecoder("utf8") : void 0;
    var UTF8ArrayToString = (heapOrArray, idx, maxBytesToRead) => {
      var endIdx = idx + maxBytesToRead;
      var endPtr = idx;
      while (heapOrArray[endPtr] && !(endPtr >= endIdx)) ++endPtr;
      if (endPtr - idx > 16 && heapOrArray.buffer && UTF8Decoder) {
        return UTF8Decoder.decode(heapOrArray.subarray(idx, endPtr));
      }
      var str = "";
      while (idx < endPtr) {
        var u0 = heapOrArray[idx++];
        if (!(u0 & 128)) {
          str += String.fromCharCode(u0);
          continue;
        }
        var u1 = heapOrArray[idx++] & 63;
        if ((u0 & 224) == 192) {
          str += String.fromCharCode((u0 & 31) << 6 | u1);
          continue;
        }
        var u2 = heapOrArray[idx++] & 63;
        if ((u0 & 240) == 224) {
          u0 = (u0 & 15) << 12 | u1 << 6 | u2;
        } else {
          if ((u0 & 248) != 240) warnOnce("Invalid UTF-8 leading byte " + ptrToString(u0) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!");
          u0 = (u0 & 7) << 18 | u1 << 12 | u2 << 6 | heapOrArray[idx++] & 63;
        }
        if (u0 < 65536) {
          str += String.fromCharCode(u0);
        } else {
          var ch = u0 - 65536;
          str += String.fromCharCode(55296 | ch >> 10, 56320 | ch & 1023);
        }
      }
      return str;
    };
    var UTF8ToString = (ptr, maxBytesToRead) => {
      assert(typeof ptr == "number", `UTF8ToString expects a number (got ${typeof ptr})`);
      return ptr ? UTF8ArrayToString(HEAPU8, ptr, maxBytesToRead) : "";
    };
    var ___assert_fail = (condition, filename, line, func) => {
      abort(`Assertion failed: ${UTF8ToString(condition)}, at: ` + [filename ? UTF8ToString(filename) : "unknown filename", line, func ? UTF8ToString(func) : "unknown function"]);
    };
    class ExceptionInfo {
      constructor(excPtr) {
        this.excPtr = excPtr;
        this.ptr = excPtr - 24;
      }
      set_type(type) {
        SAFE_HEAP_STORE((this.ptr + 4 >> 2) * 4, type, 4);
      }
      get_type() {
        return SAFE_HEAP_LOAD((this.ptr + 4 >> 2) * 4, 4, 1);
      }
      set_destructor(destructor) {
        SAFE_HEAP_STORE((this.ptr + 8 >> 2) * 4, destructor, 4);
      }
      get_destructor() {
        return SAFE_HEAP_LOAD((this.ptr + 8 >> 2) * 4, 4, 1);
      }
      set_caught(caught) {
        caught = caught ? 1 : 0;
        SAFE_HEAP_STORE(this.ptr + 12, caught, 1);
      }
      get_caught() {
        return SAFE_HEAP_LOAD(this.ptr + 12, 1, 0) != 0;
      }
      set_rethrown(rethrown) {
        rethrown = rethrown ? 1 : 0;
        SAFE_HEAP_STORE(this.ptr + 13, rethrown, 1);
      }
      get_rethrown() {
        return SAFE_HEAP_LOAD(this.ptr + 13, 1, 0) != 0;
      }
      init(type, destructor) {
        this.set_adjusted_ptr(0);
        this.set_type(type);
        this.set_destructor(destructor);
      }
      set_adjusted_ptr(adjustedPtr) {
        SAFE_HEAP_STORE((this.ptr + 16 >> 2) * 4, adjustedPtr, 4);
      }
      get_adjusted_ptr() {
        return SAFE_HEAP_LOAD((this.ptr + 16 >> 2) * 4, 4, 1);
      }
      get_exception_ptr() {
        var isPointer = ___cxa_is_pointer_type(this.get_type());
        if (isPointer) {
          return SAFE_HEAP_LOAD((this.excPtr >> 2) * 4, 4, 1);
        }
        var adjusted = this.get_adjusted_ptr();
        if (adjusted !== 0) return adjusted;
        return this.excPtr;
      }
    }
    var ___cxa_throw = (ptr, type, destructor) => {
      var info = new ExceptionInfo(ptr);
      info.init(type, destructor);
      assert(false, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
    };
    function syscallGetVarargI() {
      assert(SYSCALLS.varargs != void 0);
      var ret = SAFE_HEAP_LOAD((+SYSCALLS.varargs >> 2) * 4, 4, 0);
      SYSCALLS.varargs += 4;
      return ret;
    }
    var syscallGetVarargP = syscallGetVarargI;
    var PATH = {
      isAbs: (path) => path.charAt(0) === "/",
      splitPath: (filename) => {
        var splitPathRe = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
        return splitPathRe.exec(filename).slice(1);
      },
      normalizeArray: (parts, allowAboveRoot) => {
        var up = 0;
        for (var i = parts.length - 1; i >= 0; i--) {
          var last = parts[i];
          if (last === ".") {
            parts.splice(i, 1);
          } else if (last === "..") {
            parts.splice(i, 1);
            up++;
          } else if (up) {
            parts.splice(i, 1);
            up--;
          }
        }
        if (allowAboveRoot) {
          for (; up; up--) {
            parts.unshift("..");
          }
        }
        return parts;
      },
      normalize: (path) => {
        var isAbsolute = PATH.isAbs(path), trailingSlash = path.substr(-1) === "/";
        path = PATH.normalizeArray(path.split("/").filter((p) => !!p), !isAbsolute).join("/");
        if (!path && !isAbsolute) {
          path = ".";
        }
        if (path && trailingSlash) {
          path += "/";
        }
        return (isAbsolute ? "/" : "") + path;
      },
      dirname: (path) => {
        var result = PATH.splitPath(path), root = result[0], dir = result[1];
        if (!root && !dir) {
          return ".";
        }
        if (dir) {
          dir = dir.substr(0, dir.length - 1);
        }
        return root + dir;
      },
      basename: (path) => {
        if (path === "/") return "/";
        path = PATH.normalize(path);
        path = path.replace(/\/$/, "");
        var lastSlash = path.lastIndexOf("/");
        if (lastSlash === -1) return path;
        return path.substr(lastSlash + 1);
      },
      join: (...paths) => PATH.normalize(paths.join("/")),
      join2: (l, r) => PATH.normalize(l + "/" + r)
    };
    var initRandomFill = () => {
      if (typeof crypto == "object" && typeof crypto["getRandomValues"] == "function") {
        return (view) => crypto.getRandomValues(view);
      } else abort("no cryptographic support found for randomDevice. consider polyfilling it if you want to use something insecure like Math.random(), e.g. put this in a --pre-js: var crypto = { getRandomValues: (array) => { for (var i = 0; i < array.length; i++) array[i] = (Math.random()*256)|0 } };");
    };
    var randomFill = (view) => (randomFill = initRandomFill())(view);
    var PATH_FS = {
      resolve: (...args) => {
        var resolvedPath = "", resolvedAbsolute = false;
        for (var i = args.length - 1; i >= -1 && !resolvedAbsolute; i--) {
          var path = i >= 0 ? args[i] : FS.cwd();
          if (typeof path != "string") {
            throw new TypeError("Arguments to path.resolve must be strings");
          } else if (!path) {
            return "";
          }
          resolvedPath = path + "/" + resolvedPath;
          resolvedAbsolute = PATH.isAbs(path);
        }
        resolvedPath = PATH.normalizeArray(resolvedPath.split("/").filter((p) => !!p), !resolvedAbsolute).join("/");
        return (resolvedAbsolute ? "/" : "") + resolvedPath || ".";
      },
      relative: (from, to) => {
        from = PATH_FS.resolve(from).substr(1);
        to = PATH_FS.resolve(to).substr(1);
        function trim(arr) {
          var start = 0;
          for (; start < arr.length; start++) {
            if (arr[start] !== "") break;
          }
          var end = arr.length - 1;
          for (; end >= 0; end--) {
            if (arr[end] !== "") break;
          }
          if (start > end) return [];
          return arr.slice(start, end - start + 1);
        }
        var fromParts = trim(from.split("/"));
        var toParts = trim(to.split("/"));
        var length = Math.min(fromParts.length, toParts.length);
        var samePartsLength = length;
        for (var i = 0; i < length; i++) {
          if (fromParts[i] !== toParts[i]) {
            samePartsLength = i;
            break;
          }
        }
        var outputParts = [];
        for (var i = samePartsLength; i < fromParts.length; i++) {
          outputParts.push("..");
        }
        outputParts = outputParts.concat(toParts.slice(samePartsLength));
        return outputParts.join("/");
      }
    };
    var FS_stdin_getChar_buffer = [];
    var lengthBytesUTF8 = (str) => {
      var len = 0;
      for (var i = 0; i < str.length; ++i) {
        var c = str.charCodeAt(i);
        if (c <= 127) {
          len++;
        } else if (c <= 2047) {
          len += 2;
        } else if (c >= 55296 && c <= 57343) {
          len += 4;
          ++i;
        } else {
          len += 3;
        }
      }
      return len;
    };
    var stringToUTF8Array = (str, heap, outIdx, maxBytesToWrite) => {
      assert(typeof str === "string", `stringToUTF8Array expects a string (got ${typeof str})`);
      if (!(maxBytesToWrite > 0)) return 0;
      var startIdx = outIdx;
      var endIdx = outIdx + maxBytesToWrite - 1;
      for (var i = 0; i < str.length; ++i) {
        var u = str.charCodeAt(i);
        if (u >= 55296 && u <= 57343) {
          var u1 = str.charCodeAt(++i);
          u = 65536 + ((u & 1023) << 10) | u1 & 1023;
        }
        if (u <= 127) {
          if (outIdx >= endIdx) break;
          heap[outIdx++] = u;
        } else if (u <= 2047) {
          if (outIdx + 1 >= endIdx) break;
          heap[outIdx++] = 192 | u >> 6;
          heap[outIdx++] = 128 | u & 63;
        } else if (u <= 65535) {
          if (outIdx + 2 >= endIdx) break;
          heap[outIdx++] = 224 | u >> 12;
          heap[outIdx++] = 128 | u >> 6 & 63;
          heap[outIdx++] = 128 | u & 63;
        } else {
          if (outIdx + 3 >= endIdx) break;
          if (u > 1114111) warnOnce("Invalid Unicode code point " + ptrToString(u) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF).");
          heap[outIdx++] = 240 | u >> 18;
          heap[outIdx++] = 128 | u >> 12 & 63;
          heap[outIdx++] = 128 | u >> 6 & 63;
          heap[outIdx++] = 128 | u & 63;
        }
      }
      heap[outIdx] = 0;
      return outIdx - startIdx;
    };
    function intArrayFromString(stringy, dontAddNull, length) {
      var len = lengthBytesUTF8(stringy) + 1;
      var u8array = new Array(len);
      var numBytesWritten = stringToUTF8Array(stringy, u8array, 0, u8array.length);
      if (dontAddNull) u8array.length = numBytesWritten;
      return u8array;
    }
    var FS_stdin_getChar = () => {
      if (!FS_stdin_getChar_buffer.length) {
        var result = null;
        if (typeof window != "undefined" && typeof window.prompt == "function") {
          result = window.prompt("Input: ");
          if (result !== null) {
            result += "\n";
          }
        } else if (typeof readline == "function") {
          result = readline();
          if (result !== null) {
            result += "\n";
          }
        }
        if (!result) {
          return null;
        }
        FS_stdin_getChar_buffer = intArrayFromString(result, true);
      }
      return FS_stdin_getChar_buffer.shift();
    };
    var TTY = {
      ttys: [],
      init() {
      },
      shutdown() {
      },
      register(dev, ops) {
        TTY.ttys[dev] = {
          input: [],
          output: [],
          ops
        };
        FS.registerDevice(dev, TTY.stream_ops);
      },
      stream_ops: {
        open(stream) {
          var tty = TTY.ttys[stream.node.rdev];
          if (!tty) {
            throw new FS.ErrnoError(43);
          }
          stream.tty = tty;
          stream.seekable = false;
        },
        close(stream) {
          stream.tty.ops.fsync(stream.tty);
        },
        fsync(stream) {
          stream.tty.ops.fsync(stream.tty);
        },
        read(stream, buffer, offset, length, pos) {
          if (!stream.tty || !stream.tty.ops.get_char) {
            throw new FS.ErrnoError(60);
          }
          var bytesRead = 0;
          for (var i = 0; i < length; i++) {
            var result;
            try {
              result = stream.tty.ops.get_char(stream.tty);
            } catch (e) {
              throw new FS.ErrnoError(29);
            }
            if (result === void 0 && bytesRead === 0) {
              throw new FS.ErrnoError(6);
            }
            if (result === null || result === void 0) break;
            bytesRead++;
            buffer[offset + i] = result;
          }
          if (bytesRead) {
            stream.node.timestamp = Date.now();
          }
          return bytesRead;
        },
        write(stream, buffer, offset, length, pos) {
          if (!stream.tty || !stream.tty.ops.put_char) {
            throw new FS.ErrnoError(60);
          }
          try {
            for (var i = 0; i < length; i++) {
              stream.tty.ops.put_char(stream.tty, buffer[offset + i]);
            }
          } catch (e) {
            throw new FS.ErrnoError(29);
          }
          if (length) {
            stream.node.timestamp = Date.now();
          }
          return i;
        }
      },
      default_tty_ops: {
        get_char(tty) {
          return FS_stdin_getChar();
        },
        put_char(tty, val) {
          if (val === null || val === 10) {
            out(UTF8ArrayToString(tty.output, 0));
            tty.output = [];
          } else {
            if (val != 0) tty.output.push(val);
          }
        },
        fsync(tty) {
          if (tty.output && tty.output.length > 0) {
            out(UTF8ArrayToString(tty.output, 0));
            tty.output = [];
          }
        },
        ioctl_tcgets(tty) {
          return {
            c_iflag: 25856,
            c_oflag: 5,
            c_cflag: 191,
            c_lflag: 35387,
            c_cc: [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
          };
        },
        ioctl_tcsets(tty, optional_actions, data) {
          return 0;
        },
        ioctl_tiocgwinsz(tty) {
          return [24, 80];
        }
      },
      default_tty1_ops: {
        put_char(tty, val) {
          if (val === null || val === 10) {
            err(UTF8ArrayToString(tty.output, 0));
            tty.output = [];
          } else {
            if (val != 0) tty.output.push(val);
          }
        },
        fsync(tty) {
          if (tty.output && tty.output.length > 0) {
            err(UTF8ArrayToString(tty.output, 0));
            tty.output = [];
          }
        }
      }
    };
    var zeroMemory = (address, size) => {
      HEAPU8.fill(0, address, address + size);
      return address;
    };
    var alignMemory = (size, alignment) => {
      assert(alignment, "alignment argument is required");
      return Math.ceil(size / alignment) * alignment;
    };
    var mmapAlloc = (size) => {
      size = alignMemory(size, 65536);
      var ptr = _emscripten_builtin_memalign(65536, size);
      if (!ptr) return 0;
      return zeroMemory(ptr, size);
    };
    var MEMFS = {
      ops_table: null,
      mount(mount) {
        return MEMFS.createNode(
          null,
          "/",
          16384 | 511,
          /* 0777 */
          0
        );
      },
      createNode(parent, name, mode, dev) {
        if (FS.isBlkdev(mode) || FS.isFIFO(mode)) {
          throw new FS.ErrnoError(63);
        }
        MEMFS.ops_table || (MEMFS.ops_table = {
          dir: {
            node: {
              getattr: MEMFS.node_ops.getattr,
              setattr: MEMFS.node_ops.setattr,
              lookup: MEMFS.node_ops.lookup,
              mknod: MEMFS.node_ops.mknod,
              rename: MEMFS.node_ops.rename,
              unlink: MEMFS.node_ops.unlink,
              rmdir: MEMFS.node_ops.rmdir,
              readdir: MEMFS.node_ops.readdir,
              symlink: MEMFS.node_ops.symlink
            },
            stream: {
              llseek: MEMFS.stream_ops.llseek
            }
          },
          file: {
            node: {
              getattr: MEMFS.node_ops.getattr,
              setattr: MEMFS.node_ops.setattr
            },
            stream: {
              llseek: MEMFS.stream_ops.llseek,
              read: MEMFS.stream_ops.read,
              write: MEMFS.stream_ops.write,
              allocate: MEMFS.stream_ops.allocate,
              mmap: MEMFS.stream_ops.mmap,
              msync: MEMFS.stream_ops.msync
            }
          },
          link: {
            node: {
              getattr: MEMFS.node_ops.getattr,
              setattr: MEMFS.node_ops.setattr,
              readlink: MEMFS.node_ops.readlink
            },
            stream: {}
          },
          chrdev: {
            node: {
              getattr: MEMFS.node_ops.getattr,
              setattr: MEMFS.node_ops.setattr
            },
            stream: FS.chrdev_stream_ops
          }
        });
        var node = FS.createNode(parent, name, mode, dev);
        if (FS.isDir(node.mode)) {
          node.node_ops = MEMFS.ops_table.dir.node;
          node.stream_ops = MEMFS.ops_table.dir.stream;
          node.contents = {};
        } else if (FS.isFile(node.mode)) {
          node.node_ops = MEMFS.ops_table.file.node;
          node.stream_ops = MEMFS.ops_table.file.stream;
          node.usedBytes = 0;
          node.contents = null;
        } else if (FS.isLink(node.mode)) {
          node.node_ops = MEMFS.ops_table.link.node;
          node.stream_ops = MEMFS.ops_table.link.stream;
        } else if (FS.isChrdev(node.mode)) {
          node.node_ops = MEMFS.ops_table.chrdev.node;
          node.stream_ops = MEMFS.ops_table.chrdev.stream;
        }
        node.timestamp = Date.now();
        if (parent) {
          parent.contents[name] = node;
          parent.timestamp = node.timestamp;
        }
        return node;
      },
      getFileDataAsTypedArray(node) {
        if (!node.contents) return new Uint8Array(0);
        if (node.contents.subarray) return node.contents.subarray(0, node.usedBytes);
        return new Uint8Array(node.contents);
      },
      expandFileStorage(node, newCapacity) {
        var prevCapacity = node.contents ? node.contents.length : 0;
        if (prevCapacity >= newCapacity) return;
        var CAPACITY_DOUBLING_MAX = 1024 * 1024;
        newCapacity = Math.max(newCapacity, prevCapacity * (prevCapacity < CAPACITY_DOUBLING_MAX ? 2 : 1.125) >>> 0);
        if (prevCapacity != 0) newCapacity = Math.max(newCapacity, 256);
        var oldContents = node.contents;
        node.contents = new Uint8Array(newCapacity);
        if (node.usedBytes > 0) node.contents.set(oldContents.subarray(0, node.usedBytes), 0);
      },
      resizeFileStorage(node, newSize) {
        if (node.usedBytes == newSize) return;
        if (newSize == 0) {
          node.contents = null;
          node.usedBytes = 0;
        } else {
          var oldContents = node.contents;
          node.contents = new Uint8Array(newSize);
          if (oldContents) {
            node.contents.set(oldContents.subarray(0, Math.min(newSize, node.usedBytes)));
          }
          node.usedBytes = newSize;
        }
      },
      node_ops: {
        getattr(node) {
          var attr = {};
          attr.dev = FS.isChrdev(node.mode) ? node.id : 1;
          attr.ino = node.id;
          attr.mode = node.mode;
          attr.nlink = 1;
          attr.uid = 0;
          attr.gid = 0;
          attr.rdev = node.rdev;
          if (FS.isDir(node.mode)) {
            attr.size = 4096;
          } else if (FS.isFile(node.mode)) {
            attr.size = node.usedBytes;
          } else if (FS.isLink(node.mode)) {
            attr.size = node.link.length;
          } else {
            attr.size = 0;
          }
          attr.atime = new Date(node.timestamp);
          attr.mtime = new Date(node.timestamp);
          attr.ctime = new Date(node.timestamp);
          attr.blksize = 4096;
          attr.blocks = Math.ceil(attr.size / attr.blksize);
          return attr;
        },
        setattr(node, attr) {
          if (attr.mode !== void 0) {
            node.mode = attr.mode;
          }
          if (attr.timestamp !== void 0) {
            node.timestamp = attr.timestamp;
          }
          if (attr.size !== void 0) {
            MEMFS.resizeFileStorage(node, attr.size);
          }
        },
        lookup(parent, name) {
          throw FS.genericErrors[44];
        },
        mknod(parent, name, mode, dev) {
          return MEMFS.createNode(parent, name, mode, dev);
        },
        rename(old_node, new_dir, new_name) {
          if (FS.isDir(old_node.mode)) {
            var new_node;
            try {
              new_node = FS.lookupNode(new_dir, new_name);
            } catch (e) {
            }
            if (new_node) {
              for (var i in new_node.contents) {
                throw new FS.ErrnoError(55);
              }
            }
          }
          delete old_node.parent.contents[old_node.name];
          old_node.parent.timestamp = Date.now();
          old_node.name = new_name;
          new_dir.contents[new_name] = old_node;
          new_dir.timestamp = old_node.parent.timestamp;
          old_node.parent = new_dir;
        },
        unlink(parent, name) {
          delete parent.contents[name];
          parent.timestamp = Date.now();
        },
        rmdir(parent, name) {
          var node = FS.lookupNode(parent, name);
          for (var i in node.contents) {
            throw new FS.ErrnoError(55);
          }
          delete parent.contents[name];
          parent.timestamp = Date.now();
        },
        readdir(node) {
          var entries = [".", ".."];
          for (var key of Object.keys(node.contents)) {
            entries.push(key);
          }
          return entries;
        },
        symlink(parent, newname, oldpath) {
          var node = MEMFS.createNode(parent, newname, 511 | /* 0777 */
          40960, 0);
          node.link = oldpath;
          return node;
        },
        readlink(node) {
          if (!FS.isLink(node.mode)) {
            throw new FS.ErrnoError(28);
          }
          return node.link;
        }
      },
      stream_ops: {
        read(stream, buffer, offset, length, position) {
          var contents = stream.node.contents;
          if (position >= stream.node.usedBytes) return 0;
          var size = Math.min(stream.node.usedBytes - position, length);
          assert(size >= 0);
          if (size > 8 && contents.subarray) {
            buffer.set(contents.subarray(position, position + size), offset);
          } else {
            for (var i = 0; i < size; i++) buffer[offset + i] = contents[position + i];
          }
          return size;
        },
        write(stream, buffer, offset, length, position, canOwn) {
          assert(!(buffer instanceof ArrayBuffer));
          if (buffer.buffer === HEAP8.buffer) {
            canOwn = false;
          }
          if (!length) return 0;
          var node = stream.node;
          node.timestamp = Date.now();
          if (buffer.subarray && (!node.contents || node.contents.subarray)) {
            if (canOwn) {
              assert(position === 0, "canOwn must imply no weird position inside the file");
              node.contents = buffer.subarray(offset, offset + length);
              node.usedBytes = length;
              return length;
            } else if (node.usedBytes === 0 && position === 0) {
              node.contents = buffer.slice(offset, offset + length);
              node.usedBytes = length;
              return length;
            } else if (position + length <= node.usedBytes) {
              node.contents.set(buffer.subarray(offset, offset + length), position);
              return length;
            }
          }
          MEMFS.expandFileStorage(node, position + length);
          if (node.contents.subarray && buffer.subarray) {
            node.contents.set(buffer.subarray(offset, offset + length), position);
          } else {
            for (var i = 0; i < length; i++) {
              node.contents[position + i] = buffer[offset + i];
            }
          }
          node.usedBytes = Math.max(node.usedBytes, position + length);
          return length;
        },
        llseek(stream, offset, whence) {
          var position = offset;
          if (whence === 1) {
            position += stream.position;
          } else if (whence === 2) {
            if (FS.isFile(stream.node.mode)) {
              position += stream.node.usedBytes;
            }
          }
          if (position < 0) {
            throw new FS.ErrnoError(28);
          }
          return position;
        },
        allocate(stream, offset, length) {
          MEMFS.expandFileStorage(stream.node, offset + length);
          stream.node.usedBytes = Math.max(stream.node.usedBytes, offset + length);
        },
        mmap(stream, length, position, prot, flags) {
          if (!FS.isFile(stream.node.mode)) {
            throw new FS.ErrnoError(43);
          }
          var ptr;
          var allocated;
          var contents = stream.node.contents;
          if (!(flags & 2) && contents.buffer === HEAP8.buffer) {
            allocated = false;
            ptr = contents.byteOffset;
          } else {
            if (position > 0 || position + length < contents.length) {
              if (contents.subarray) {
                contents = contents.subarray(position, position + length);
              } else {
                contents = Array.prototype.slice.call(contents, position, position + length);
              }
            }
            allocated = true;
            ptr = mmapAlloc(length);
            if (!ptr) {
              throw new FS.ErrnoError(48);
            }
            HEAP8.set(contents, ptr);
          }
          return {
            ptr,
            allocated
          };
        },
        msync(stream, buffer, offset, length, mmapFlags) {
          MEMFS.stream_ops.write(stream, buffer, 0, length, offset, false);
          return 0;
        }
      }
    };
    var asyncLoad = (url, onload, onerror, noRunDep) => {
      var dep = getUniqueRunDependency(`al ${url}`) ;
      readAsync(url, (arrayBuffer) => {
        assert(arrayBuffer, `Loading data file "${url}" failed (no arrayBuffer).`);
        onload(new Uint8Array(arrayBuffer));
        if (dep) removeRunDependency(dep);
      }, (event) => {
        if (onerror) {
          onerror();
        } else {
          throw `Loading data file "${url}" failed.`;
        }
      });
      if (dep) addRunDependency(dep);
    };
    var FS_createDataFile = (parent, name, fileData, canRead, canWrite, canOwn) => {
      FS.createDataFile(parent, name, fileData, canRead, canWrite, canOwn);
    };
    var preloadPlugins = Module["preloadPlugins"] || [];
    var FS_handledByPreloadPlugin = (byteArray, fullname, finish, onerror) => {
      if (typeof Browser != "undefined") Browser.init();
      var handled = false;
      preloadPlugins.forEach((plugin) => {
        if (handled) return;
        if (plugin["canHandle"](fullname)) {
          plugin["handle"](byteArray, fullname, finish, onerror);
          handled = true;
        }
      });
      return handled;
    };
    var FS_createPreloadedFile = (parent, name, url, canRead, canWrite, onload, onerror, dontCreateFile, canOwn, preFinish) => {
      var fullname = name ? PATH_FS.resolve(PATH.join2(parent, name)) : parent;
      var dep = getUniqueRunDependency(`cp ${fullname}`);
      function processData(byteArray) {
        function finish(byteArray2) {
          preFinish?.();
          if (!dontCreateFile) {
            FS_createDataFile(parent, name, byteArray2, canRead, canWrite, canOwn);
          }
          onload?.();
          removeRunDependency(dep);
        }
        if (FS_handledByPreloadPlugin(byteArray, fullname, finish, () => {
          onerror?.();
          removeRunDependency(dep);
        })) {
          return;
        }
        finish(byteArray);
      }
      addRunDependency(dep);
      if (typeof url == "string") {
        asyncLoad(url, processData, onerror);
      } else {
        processData(url);
      }
    };
    var FS_modeStringToFlags = (str) => {
      var flagModes = {
        "r": 0,
        "r+": 2,
        "w": 512 | 64 | 1,
        "w+": 512 | 64 | 2,
        "a": 1024 | 64 | 1,
        "a+": 1024 | 64 | 2
      };
      var flags = flagModes[str];
      if (typeof flags == "undefined") {
        throw new Error(`Unknown file open mode: ${str}`);
      }
      return flags;
    };
    var FS_getMode = (canRead, canWrite) => {
      var mode = 0;
      if (canRead) mode |= 292 | 73;
      if (canWrite) mode |= 146;
      return mode;
    };
    var ERRNO_MESSAGES = {
      0: "Success",
      1: "Arg list too long",
      2: "Permission denied",
      3: "Address already in use",
      4: "Address not available",
      5: "Address family not supported by protocol family",
      6: "No more processes",
      7: "Socket already connected",
      8: "Bad file number",
      9: "Trying to read unreadable message",
      10: "Mount device busy",
      11: "Operation canceled",
      12: "No children",
      13: "Connection aborted",
      14: "Connection refused",
      15: "Connection reset by peer",
      16: "File locking deadlock error",
      17: "Destination address required",
      18: "Math arg out of domain of func",
      19: "Quota exceeded",
      20: "File exists",
      21: "Bad address",
      22: "File too large",
      23: "Host is unreachable",
      24: "Identifier removed",
      25: "Illegal byte sequence",
      26: "Connection already in progress",
      27: "Interrupted system call",
      28: "Invalid argument",
      29: "I/O error",
      30: "Socket is already connected",
      31: "Is a directory",
      32: "Too many symbolic links",
      33: "Too many open files",
      34: "Too many links",
      35: "Message too long",
      36: "Multihop attempted",
      37: "File or path name too long",
      38: "Network interface is not configured",
      39: "Connection reset by network",
      40: "Network is unreachable",
      41: "Too many open files in system",
      42: "No buffer space available",
      43: "No such device",
      44: "No such file or directory",
      45: "Exec format error",
      46: "No record locks available",
      47: "The link has been severed",
      48: "Not enough core",
      49: "No message of desired type",
      50: "Protocol not available",
      51: "No space left on device",
      52: "Function not implemented",
      53: "Socket is not connected",
      54: "Not a directory",
      55: "Directory not empty",
      56: "State not recoverable",
      57: "Socket operation on non-socket",
      59: "Not a typewriter",
      60: "No such device or address",
      61: "Value too large for defined data type",
      62: "Previous owner died",
      63: "Not super-user",
      64: "Broken pipe",
      65: "Protocol error",
      66: "Unknown protocol",
      67: "Protocol wrong type for socket",
      68: "Math result not representable",
      69: "Read only file system",
      70: "Illegal seek",
      71: "No such process",
      72: "Stale file handle",
      73: "Connection timed out",
      74: "Text file busy",
      75: "Cross-device link",
      100: "Device not a stream",
      101: "Bad font file fmt",
      102: "Invalid slot",
      103: "Invalid request code",
      104: "No anode",
      105: "Block device required",
      106: "Channel number out of range",
      107: "Level 3 halted",
      108: "Level 3 reset",
      109: "Link number out of range",
      110: "Protocol driver not attached",
      111: "No CSI structure available",
      112: "Level 2 halted",
      113: "Invalid exchange",
      114: "Invalid request descriptor",
      115: "Exchange full",
      116: "No data (for no delay io)",
      117: "Timer expired",
      118: "Out of streams resources",
      119: "Machine is not on the network",
      120: "Package not installed",
      121: "The object is remote",
      122: "Advertise error",
      123: "Srmount error",
      124: "Communication error on send",
      125: "Cross mount point (not really error)",
      126: "Given log. name not unique",
      127: "f.d. invalid for this operation",
      128: "Remote address changed",
      129: "Can   access a needed shared lib",
      130: "Accessing a corrupted shared lib",
      131: ".lib section in a.out corrupted",
      132: "Attempting to link in too many libs",
      133: "Attempting to exec a shared library",
      135: "Streams pipe error",
      136: "Too many users",
      137: "Socket type not supported",
      138: "Not supported",
      139: "Protocol family not supported",
      140: "Can't send after socket shutdown",
      141: "Too many references",
      142: "Host is down",
      148: "No medium (in tape drive)",
      156: "Level 2 not synchronized"
    };
    var ERRNO_CODES = {
      "EPERM": 63,
      "ENOENT": 44,
      "ESRCH": 71,
      "EINTR": 27,
      "EIO": 29,
      "ENXIO": 60,
      "E2BIG": 1,
      "ENOEXEC": 45,
      "EBADF": 8,
      "ECHILD": 12,
      "EAGAIN": 6,
      "EWOULDBLOCK": 6,
      "ENOMEM": 48,
      "EACCES": 2,
      "EFAULT": 21,
      "ENOTBLK": 105,
      "EBUSY": 10,
      "EEXIST": 20,
      "EXDEV": 75,
      "ENODEV": 43,
      "ENOTDIR": 54,
      "EISDIR": 31,
      "EINVAL": 28,
      "ENFILE": 41,
      "EMFILE": 33,
      "ENOTTY": 59,
      "ETXTBSY": 74,
      "EFBIG": 22,
      "ENOSPC": 51,
      "ESPIPE": 70,
      "EROFS": 69,
      "EMLINK": 34,
      "EPIPE": 64,
      "EDOM": 18,
      "ERANGE": 68,
      "ENOMSG": 49,
      "EIDRM": 24,
      "ECHRNG": 106,
      "EL2NSYNC": 156,
      "EL3HLT": 107,
      "EL3RST": 108,
      "ELNRNG": 109,
      "EUNATCH": 110,
      "ENOCSI": 111,
      "EL2HLT": 112,
      "EDEADLK": 16,
      "ENOLCK": 46,
      "EBADE": 113,
      "EBADR": 114,
      "EXFULL": 115,
      "ENOANO": 104,
      "EBADRQC": 103,
      "EBADSLT": 102,
      "EDEADLOCK": 16,
      "EBFONT": 101,
      "ENOSTR": 100,
      "ENODATA": 116,
      "ETIME": 117,
      "ENOSR": 118,
      "ENONET": 119,
      "ENOPKG": 120,
      "EREMOTE": 121,
      "ENOLINK": 47,
      "EADV": 122,
      "ESRMNT": 123,
      "ECOMM": 124,
      "EPROTO": 65,
      "EMULTIHOP": 36,
      "EDOTDOT": 125,
      "EBADMSG": 9,
      "ENOTUNIQ": 126,
      "EBADFD": 127,
      "EREMCHG": 128,
      "ELIBACC": 129,
      "ELIBBAD": 130,
      "ELIBSCN": 131,
      "ELIBMAX": 132,
      "ELIBEXEC": 133,
      "ENOSYS": 52,
      "ENOTEMPTY": 55,
      "ENAMETOOLONG": 37,
      "ELOOP": 32,
      "EOPNOTSUPP": 138,
      "EPFNOSUPPORT": 139,
      "ECONNRESET": 15,
      "ENOBUFS": 42,
      "EAFNOSUPPORT": 5,
      "EPROTOTYPE": 67,
      "ENOTSOCK": 57,
      "ENOPROTOOPT": 50,
      "ESHUTDOWN": 140,
      "ECONNREFUSED": 14,
      "EADDRINUSE": 3,
      "ECONNABORTED": 13,
      "ENETUNREACH": 40,
      "ENETDOWN": 38,
      "ETIMEDOUT": 73,
      "EHOSTDOWN": 142,
      "EHOSTUNREACH": 23,
      "EINPROGRESS": 26,
      "EALREADY": 7,
      "EDESTADDRREQ": 17,
      "EMSGSIZE": 35,
      "EPROTONOSUPPORT": 66,
      "ESOCKTNOSUPPORT": 137,
      "EADDRNOTAVAIL": 4,
      "ENETRESET": 39,
      "EISCONN": 30,
      "ENOTCONN": 53,
      "ETOOMANYREFS": 141,
      "EUSERS": 136,
      "EDQUOT": 19,
      "ESTALE": 72,
      "ENOTSUP": 138,
      "ENOMEDIUM": 148,
      "EILSEQ": 25,
      "EOVERFLOW": 61,
      "ECANCELED": 11,
      "ENOTRECOVERABLE": 56,
      "EOWNERDEAD": 62,
      "ESTRPIPE": 135
    };
    var FS = {
      root: null,
      mounts: [],
      devices: {},
      streams: [],
      nextInode: 1,
      nameTable: null,
      currentPath: "/",
      initialized: false,
      ignorePermissions: true,
      ErrnoError: class extends Error {
        constructor(errno) {
          super(ERRNO_MESSAGES[errno]);
          this.name = "ErrnoError";
          this.errno = errno;
          for (var key in ERRNO_CODES) {
            if (ERRNO_CODES[key] === errno) {
              this.code = key;
              break;
            }
          }
        }
      },
      genericErrors: {},
      filesystems: null,
      syncFSRequests: 0,
      FSStream: class {
        constructor() {
          this.shared = {};
        }
        get object() {
          return this.node;
        }
        set object(val) {
          this.node = val;
        }
        get isRead() {
          return (this.flags & 2097155) !== 1;
        }
        get isWrite() {
          return (this.flags & 2097155) !== 0;
        }
        get isAppend() {
          return this.flags & 1024;
        }
        get flags() {
          return this.shared.flags;
        }
        set flags(val) {
          this.shared.flags = val;
        }
        get position() {
          return this.shared.position;
        }
        set position(val) {
          this.shared.position = val;
        }
      },
      FSNode: class {
        constructor(parent, name, mode, rdev) {
          if (!parent) {
            parent = this;
          }
          this.parent = parent;
          this.mount = parent.mount;
          this.mounted = null;
          this.id = FS.nextInode++;
          this.name = name;
          this.mode = mode;
          this.node_ops = {};
          this.stream_ops = {};
          this.rdev = rdev;
          this.readMode = 292 | /*292*/
          73;
          this.writeMode = 146;
        }
        /*146*/
        get read() {
          return (this.mode & this.readMode) === this.readMode;
        }
        set read(val) {
          val ? this.mode |= this.readMode : this.mode &= ~this.readMode;
        }
        get write() {
          return (this.mode & this.writeMode) === this.writeMode;
        }
        set write(val) {
          val ? this.mode |= this.writeMode : this.mode &= ~this.writeMode;
        }
        get isFolder() {
          return FS.isDir(this.mode);
        }
        get isDevice() {
          return FS.isChrdev(this.mode);
        }
      },
      lookupPath(path, opts = {}) {
        path = PATH_FS.resolve(path);
        if (!path) return {
          path: "",
          node: null
        };
        var defaults = {
          follow_mount: true,
          recurse_count: 0
        };
        opts = Object.assign(defaults, opts);
        if (opts.recurse_count > 8) {
          throw new FS.ErrnoError(32);
        }
        var parts = path.split("/").filter((p) => !!p);
        var current = FS.root;
        var current_path = "/";
        for (var i = 0; i < parts.length; i++) {
          var islast = i === parts.length - 1;
          if (islast && opts.parent) {
            break;
          }
          current = FS.lookupNode(current, parts[i]);
          current_path = PATH.join2(current_path, parts[i]);
          if (FS.isMountpoint(current)) {
            if (!islast || islast && opts.follow_mount) {
              current = current.mounted.root;
            }
          }
          if (!islast || opts.follow) {
            var count = 0;
            while (FS.isLink(current.mode)) {
              var link = FS.readlink(current_path);
              current_path = PATH_FS.resolve(PATH.dirname(current_path), link);
              var lookup = FS.lookupPath(current_path, {
                recurse_count: opts.recurse_count + 1
              });
              current = lookup.node;
              if (count++ > 40) {
                throw new FS.ErrnoError(32);
              }
            }
          }
        }
        return {
          path: current_path,
          node: current
        };
      },
      getPath(node) {
        var path;
        while (true) {
          if (FS.isRoot(node)) {
            var mount = node.mount.mountpoint;
            if (!path) return mount;
            return mount[mount.length - 1] !== "/" ? `${mount}/${path}` : mount + path;
          }
          path = path ? `${node.name}/${path}` : node.name;
          node = node.parent;
        }
      },
      hashName(parentid, name) {
        var hash = 0;
        for (var i = 0; i < name.length; i++) {
          hash = (hash << 5) - hash + name.charCodeAt(i) | 0;
        }
        return (parentid + hash >>> 0) % FS.nameTable.length;
      },
      hashAddNode(node) {
        var hash = FS.hashName(node.parent.id, node.name);
        node.name_next = FS.nameTable[hash];
        FS.nameTable[hash] = node;
      },
      hashRemoveNode(node) {
        var hash = FS.hashName(node.parent.id, node.name);
        if (FS.nameTable[hash] === node) {
          FS.nameTable[hash] = node.name_next;
        } else {
          var current = FS.nameTable[hash];
          while (current) {
            if (current.name_next === node) {
              current.name_next = node.name_next;
              break;
            }
            current = current.name_next;
          }
        }
      },
      lookupNode(parent, name) {
        var errCode = FS.mayLookup(parent);
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        var hash = FS.hashName(parent.id, name);
        for (var node = FS.nameTable[hash]; node; node = node.name_next) {
          var nodeName = node.name;
          if (node.parent.id === parent.id && nodeName === name) {
            return node;
          }
        }
        return FS.lookup(parent, name);
      },
      createNode(parent, name, mode, rdev) {
        assert(typeof parent == "object");
        var node = new FS.FSNode(parent, name, mode, rdev);
        FS.hashAddNode(node);
        return node;
      },
      destroyNode(node) {
        FS.hashRemoveNode(node);
      },
      isRoot(node) {
        return node === node.parent;
      },
      isMountpoint(node) {
        return !!node.mounted;
      },
      isFile(mode) {
        return (mode & 61440) === 32768;
      },
      isDir(mode) {
        return (mode & 61440) === 16384;
      },
      isLink(mode) {
        return (mode & 61440) === 40960;
      },
      isChrdev(mode) {
        return (mode & 61440) === 8192;
      },
      isBlkdev(mode) {
        return (mode & 61440) === 24576;
      },
      isFIFO(mode) {
        return (mode & 61440) === 4096;
      },
      isSocket(mode) {
        return (mode & 49152) === 49152;
      },
      flagsToPermissionString(flag) {
        var perms = ["r", "w", "rw"][flag & 3];
        if (flag & 512) {
          perms += "w";
        }
        return perms;
      },
      nodePermissions(node, perms) {
        if (FS.ignorePermissions) {
          return 0;
        }
        if (perms.includes("r") && !(node.mode & 292)) {
          return 2;
        } else if (perms.includes("w") && !(node.mode & 146)) {
          return 2;
        } else if (perms.includes("x") && !(node.mode & 73)) {
          return 2;
        }
        return 0;
      },
      mayLookup(dir) {
        if (!FS.isDir(dir.mode)) return 54;
        var errCode = FS.nodePermissions(dir, "x");
        if (errCode) return errCode;
        if (!dir.node_ops.lookup) return 2;
        return 0;
      },
      mayCreate(dir, name) {
        try {
          var node = FS.lookupNode(dir, name);
          return 20;
        } catch (e) {
        }
        return FS.nodePermissions(dir, "wx");
      },
      mayDelete(dir, name, isdir) {
        var node;
        try {
          node = FS.lookupNode(dir, name);
        } catch (e) {
          return e.errno;
        }
        var errCode = FS.nodePermissions(dir, "wx");
        if (errCode) {
          return errCode;
        }
        if (isdir) {
          if (!FS.isDir(node.mode)) {
            return 54;
          }
          if (FS.isRoot(node) || FS.getPath(node) === FS.cwd()) {
            return 10;
          }
        } else {
          if (FS.isDir(node.mode)) {
            return 31;
          }
        }
        return 0;
      },
      mayOpen(node, flags) {
        if (!node) {
          return 44;
        }
        if (FS.isLink(node.mode)) {
          return 32;
        } else if (FS.isDir(node.mode)) {
          if (FS.flagsToPermissionString(flags) !== "r" || flags & 512) {
            return 31;
          }
        }
        return FS.nodePermissions(node, FS.flagsToPermissionString(flags));
      },
      MAX_OPEN_FDS: 4096,
      nextfd() {
        for (var fd = 0; fd <= FS.MAX_OPEN_FDS; fd++) {
          if (!FS.streams[fd]) {
            return fd;
          }
        }
        throw new FS.ErrnoError(33);
      },
      getStreamChecked(fd) {
        var stream = FS.getStream(fd);
        if (!stream) {
          throw new FS.ErrnoError(8);
        }
        return stream;
      },
      getStream: (fd) => FS.streams[fd],
      createStream(stream, fd = -1) {
        stream = Object.assign(new FS.FSStream(), stream);
        if (fd == -1) {
          fd = FS.nextfd();
        }
        stream.fd = fd;
        FS.streams[fd] = stream;
        return stream;
      },
      closeStream(fd) {
        FS.streams[fd] = null;
      },
      dupStream(origStream, fd = -1) {
        var stream = FS.createStream(origStream, fd);
        stream.stream_ops?.dup?.(stream);
        return stream;
      },
      chrdev_stream_ops: {
        open(stream) {
          var device = FS.getDevice(stream.node.rdev);
          stream.stream_ops = device.stream_ops;
          stream.stream_ops.open?.(stream);
        },
        llseek() {
          throw new FS.ErrnoError(70);
        }
      },
      major: (dev) => dev >> 8,
      minor: (dev) => dev & 255,
      makedev: (ma, mi) => ma << 8 | mi,
      registerDevice(dev, ops) {
        FS.devices[dev] = {
          stream_ops: ops
        };
      },
      getDevice: (dev) => FS.devices[dev],
      getMounts(mount) {
        var mounts = [];
        var check = [mount];
        while (check.length) {
          var m = check.pop();
          mounts.push(m);
          check.push(...m.mounts);
        }
        return mounts;
      },
      syncfs(populate, callback) {
        if (typeof populate == "function") {
          callback = populate;
          populate = false;
        }
        FS.syncFSRequests++;
        if (FS.syncFSRequests > 1) {
          err(`warning: ${FS.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
        }
        var mounts = FS.getMounts(FS.root.mount);
        var completed = 0;
        function doCallback(errCode) {
          assert(FS.syncFSRequests > 0);
          FS.syncFSRequests--;
          return callback(errCode);
        }
        function done(errCode) {
          if (errCode) {
            if (!done.errored) {
              done.errored = true;
              return doCallback(errCode);
            }
            return;
          }
          if (++completed >= mounts.length) {
            doCallback(null);
          }
        }
        mounts.forEach((mount) => {
          if (!mount.type.syncfs) {
            return done(null);
          }
          mount.type.syncfs(mount, populate, done);
        });
      },
      mount(type, opts, mountpoint) {
        if (typeof type == "string") {
          throw type;
        }
        var root = mountpoint === "/";
        var pseudo = !mountpoint;
        var node;
        if (root && FS.root) {
          throw new FS.ErrnoError(10);
        } else if (!root && !pseudo) {
          var lookup = FS.lookupPath(mountpoint, {
            follow_mount: false
          });
          mountpoint = lookup.path;
          node = lookup.node;
          if (FS.isMountpoint(node)) {
            throw new FS.ErrnoError(10);
          }
          if (!FS.isDir(node.mode)) {
            throw new FS.ErrnoError(54);
          }
        }
        var mount = {
          type,
          opts,
          mountpoint,
          mounts: []
        };
        var mountRoot = type.mount(mount);
        mountRoot.mount = mount;
        mount.root = mountRoot;
        if (root) {
          FS.root = mountRoot;
        } else if (node) {
          node.mounted = mount;
          if (node.mount) {
            node.mount.mounts.push(mount);
          }
        }
        return mountRoot;
      },
      unmount(mountpoint) {
        var lookup = FS.lookupPath(mountpoint, {
          follow_mount: false
        });
        if (!FS.isMountpoint(lookup.node)) {
          throw new FS.ErrnoError(28);
        }
        var node = lookup.node;
        var mount = node.mounted;
        var mounts = FS.getMounts(mount);
        Object.keys(FS.nameTable).forEach((hash) => {
          var current = FS.nameTable[hash];
          while (current) {
            var next = current.name_next;
            if (mounts.includes(current.mount)) {
              FS.destroyNode(current);
            }
            current = next;
          }
        });
        node.mounted = null;
        var idx = node.mount.mounts.indexOf(mount);
        assert(idx !== -1);
        node.mount.mounts.splice(idx, 1);
      },
      lookup(parent, name) {
        return parent.node_ops.lookup(parent, name);
      },
      mknod(path, mode, dev) {
        var lookup = FS.lookupPath(path, {
          parent: true
        });
        var parent = lookup.node;
        var name = PATH.basename(path);
        if (!name || name === "." || name === "..") {
          throw new FS.ErrnoError(28);
        }
        var errCode = FS.mayCreate(parent, name);
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        if (!parent.node_ops.mknod) {
          throw new FS.ErrnoError(63);
        }
        return parent.node_ops.mknod(parent, name, mode, dev);
      },
      create(path, mode) {
        mode = mode !== void 0 ? mode : 438;
        mode &= 4095;
        mode |= 32768;
        return FS.mknod(path, mode, 0);
      },
      mkdir(path, mode) {
        mode = mode !== void 0 ? mode : 511;
        mode &= 511 | 512;
        mode |= 16384;
        return FS.mknod(path, mode, 0);
      },
      mkdirTree(path, mode) {
        var dirs = path.split("/");
        var d = "";
        for (var i = 0; i < dirs.length; ++i) {
          if (!dirs[i]) continue;
          d += "/" + dirs[i];
          try {
            FS.mkdir(d, mode);
          } catch (e) {
            if (e.errno != 20) throw e;
          }
        }
      },
      mkdev(path, mode, dev) {
        if (typeof dev == "undefined") {
          dev = mode;
          mode = 438;
        }
        mode |= 8192;
        return FS.mknod(path, mode, dev);
      },
      symlink(oldpath, newpath) {
        if (!PATH_FS.resolve(oldpath)) {
          throw new FS.ErrnoError(44);
        }
        var lookup = FS.lookupPath(newpath, {
          parent: true
        });
        var parent = lookup.node;
        if (!parent) {
          throw new FS.ErrnoError(44);
        }
        var newname = PATH.basename(newpath);
        var errCode = FS.mayCreate(parent, newname);
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        if (!parent.node_ops.symlink) {
          throw new FS.ErrnoError(63);
        }
        return parent.node_ops.symlink(parent, newname, oldpath);
      },
      rename(old_path, new_path) {
        var old_dirname = PATH.dirname(old_path);
        var new_dirname = PATH.dirname(new_path);
        var old_name = PATH.basename(old_path);
        var new_name = PATH.basename(new_path);
        var lookup, old_dir, new_dir;
        lookup = FS.lookupPath(old_path, {
          parent: true
        });
        old_dir = lookup.node;
        lookup = FS.lookupPath(new_path, {
          parent: true
        });
        new_dir = lookup.node;
        if (!old_dir || !new_dir) throw new FS.ErrnoError(44);
        if (old_dir.mount !== new_dir.mount) {
          throw new FS.ErrnoError(75);
        }
        var old_node = FS.lookupNode(old_dir, old_name);
        var relative = PATH_FS.relative(old_path, new_dirname);
        if (relative.charAt(0) !== ".") {
          throw new FS.ErrnoError(28);
        }
        relative = PATH_FS.relative(new_path, old_dirname);
        if (relative.charAt(0) !== ".") {
          throw new FS.ErrnoError(55);
        }
        var new_node;
        try {
          new_node = FS.lookupNode(new_dir, new_name);
        } catch (e) {
        }
        if (old_node === new_node) {
          return;
        }
        var isdir = FS.isDir(old_node.mode);
        var errCode = FS.mayDelete(old_dir, old_name, isdir);
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        errCode = new_node ? FS.mayDelete(new_dir, new_name, isdir) : FS.mayCreate(new_dir, new_name);
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        if (!old_dir.node_ops.rename) {
          throw new FS.ErrnoError(63);
        }
        if (FS.isMountpoint(old_node) || new_node && FS.isMountpoint(new_node)) {
          throw new FS.ErrnoError(10);
        }
        if (new_dir !== old_dir) {
          errCode = FS.nodePermissions(old_dir, "w");
          if (errCode) {
            throw new FS.ErrnoError(errCode);
          }
        }
        FS.hashRemoveNode(old_node);
        try {
          old_dir.node_ops.rename(old_node, new_dir, new_name);
        } catch (e) {
          throw e;
        } finally {
          FS.hashAddNode(old_node);
        }
      },
      rmdir(path) {
        var lookup = FS.lookupPath(path, {
          parent: true
        });
        var parent = lookup.node;
        var name = PATH.basename(path);
        var node = FS.lookupNode(parent, name);
        var errCode = FS.mayDelete(parent, name, true);
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        if (!parent.node_ops.rmdir) {
          throw new FS.ErrnoError(63);
        }
        if (FS.isMountpoint(node)) {
          throw new FS.ErrnoError(10);
        }
        parent.node_ops.rmdir(parent, name);
        FS.destroyNode(node);
      },
      readdir(path) {
        var lookup = FS.lookupPath(path, {
          follow: true
        });
        var node = lookup.node;
        if (!node.node_ops.readdir) {
          throw new FS.ErrnoError(54);
        }
        return node.node_ops.readdir(node);
      },
      unlink(path) {
        var lookup = FS.lookupPath(path, {
          parent: true
        });
        var parent = lookup.node;
        if (!parent) {
          throw new FS.ErrnoError(44);
        }
        var name = PATH.basename(path);
        var node = FS.lookupNode(parent, name);
        var errCode = FS.mayDelete(parent, name, false);
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        if (!parent.node_ops.unlink) {
          throw new FS.ErrnoError(63);
        }
        if (FS.isMountpoint(node)) {
          throw new FS.ErrnoError(10);
        }
        parent.node_ops.unlink(parent, name);
        FS.destroyNode(node);
      },
      readlink(path) {
        var lookup = FS.lookupPath(path);
        var link = lookup.node;
        if (!link) {
          throw new FS.ErrnoError(44);
        }
        if (!link.node_ops.readlink) {
          throw new FS.ErrnoError(28);
        }
        return PATH_FS.resolve(FS.getPath(link.parent), link.node_ops.readlink(link));
      },
      stat(path, dontFollow) {
        var lookup = FS.lookupPath(path, {
          follow: !dontFollow
        });
        var node = lookup.node;
        if (!node) {
          throw new FS.ErrnoError(44);
        }
        if (!node.node_ops.getattr) {
          throw new FS.ErrnoError(63);
        }
        return node.node_ops.getattr(node);
      },
      lstat(path) {
        return FS.stat(path, true);
      },
      chmod(path, mode, dontFollow) {
        var node;
        if (typeof path == "string") {
          var lookup = FS.lookupPath(path, {
            follow: !dontFollow
          });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(63);
        }
        node.node_ops.setattr(node, {
          mode: mode & 4095 | node.mode & ~4095,
          timestamp: Date.now()
        });
      },
      lchmod(path, mode) {
        FS.chmod(path, mode, true);
      },
      fchmod(fd, mode) {
        var stream = FS.getStreamChecked(fd);
        FS.chmod(stream.node, mode);
      },
      chown(path, uid, gid, dontFollow) {
        var node;
        if (typeof path == "string") {
          var lookup = FS.lookupPath(path, {
            follow: !dontFollow
          });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(63);
        }
        node.node_ops.setattr(node, {
          timestamp: Date.now()
        });
      },
      lchown(path, uid, gid) {
        FS.chown(path, uid, gid, true);
      },
      fchown(fd, uid, gid) {
        var stream = FS.getStreamChecked(fd);
        FS.chown(stream.node, uid, gid);
      },
      truncate(path, len) {
        if (len < 0) {
          throw new FS.ErrnoError(28);
        }
        var node;
        if (typeof path == "string") {
          var lookup = FS.lookupPath(path, {
            follow: true
          });
          node = lookup.node;
        } else {
          node = path;
        }
        if (!node.node_ops.setattr) {
          throw new FS.ErrnoError(63);
        }
        if (FS.isDir(node.mode)) {
          throw new FS.ErrnoError(31);
        }
        if (!FS.isFile(node.mode)) {
          throw new FS.ErrnoError(28);
        }
        var errCode = FS.nodePermissions(node, "w");
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        node.node_ops.setattr(node, {
          size: len,
          timestamp: Date.now()
        });
      },
      ftruncate(fd, len) {
        var stream = FS.getStreamChecked(fd);
        if ((stream.flags & 2097155) === 0) {
          throw new FS.ErrnoError(28);
        }
        FS.truncate(stream.node, len);
      },
      utime(path, atime, mtime) {
        var lookup = FS.lookupPath(path, {
          follow: true
        });
        var node = lookup.node;
        node.node_ops.setattr(node, {
          timestamp: Math.max(atime, mtime)
        });
      },
      open(path, flags, mode) {
        if (path === "") {
          throw new FS.ErrnoError(44);
        }
        flags = typeof flags == "string" ? FS_modeStringToFlags(flags) : flags;
        mode = typeof mode == "undefined" ? 438 : (
          /* 0666 */
          mode
        );
        if (flags & 64) {
          mode = mode & 4095 | 32768;
        } else {
          mode = 0;
        }
        var node;
        if (typeof path == "object") {
          node = path;
        } else {
          path = PATH.normalize(path);
          try {
            var lookup = FS.lookupPath(path, {
              follow: !(flags & 131072)
            });
            node = lookup.node;
          } catch (e) {
          }
        }
        var created = false;
        if (flags & 64) {
          if (node) {
            if (flags & 128) {
              throw new FS.ErrnoError(20);
            }
          } else {
            node = FS.mknod(path, mode, 0);
            created = true;
          }
        }
        if (!node) {
          throw new FS.ErrnoError(44);
        }
        if (FS.isChrdev(node.mode)) {
          flags &= ~512;
        }
        if (flags & 65536 && !FS.isDir(node.mode)) {
          throw new FS.ErrnoError(54);
        }
        if (!created) {
          var errCode = FS.mayOpen(node, flags);
          if (errCode) {
            throw new FS.ErrnoError(errCode);
          }
        }
        if (flags & 512 && !created) {
          FS.truncate(node, 0);
        }
        flags &= ~(128 | 512 | 131072);
        var stream = FS.createStream({
          node,
          path: FS.getPath(node),
          flags,
          seekable: true,
          position: 0,
          stream_ops: node.stream_ops,
          ungotten: [],
          error: false
        });
        if (stream.stream_ops.open) {
          stream.stream_ops.open(stream);
        }
        if (Module["logReadFiles"] && !(flags & 1)) {
          if (!FS.readFiles) FS.readFiles = {};
          if (!(path in FS.readFiles)) {
            FS.readFiles[path] = 1;
          }
        }
        return stream;
      },
      close(stream) {
        if (FS.isClosed(stream)) {
          throw new FS.ErrnoError(8);
        }
        if (stream.getdents) stream.getdents = null;
        try {
          if (stream.stream_ops.close) {
            stream.stream_ops.close(stream);
          }
        } catch (e) {
          throw e;
        } finally {
          FS.closeStream(stream.fd);
        }
        stream.fd = null;
      },
      isClosed(stream) {
        return stream.fd === null;
      },
      llseek(stream, offset, whence) {
        if (FS.isClosed(stream)) {
          throw new FS.ErrnoError(8);
        }
        if (!stream.seekable || !stream.stream_ops.llseek) {
          throw new FS.ErrnoError(70);
        }
        if (whence != 0 && whence != 1 && whence != 2) {
          throw new FS.ErrnoError(28);
        }
        stream.position = stream.stream_ops.llseek(stream, offset, whence);
        stream.ungotten = [];
        return stream.position;
      },
      read(stream, buffer, offset, length, position) {
        assert(offset >= 0);
        if (length < 0 || position < 0) {
          throw new FS.ErrnoError(28);
        }
        if (FS.isClosed(stream)) {
          throw new FS.ErrnoError(8);
        }
        if ((stream.flags & 2097155) === 1) {
          throw new FS.ErrnoError(8);
        }
        if (FS.isDir(stream.node.mode)) {
          throw new FS.ErrnoError(31);
        }
        if (!stream.stream_ops.read) {
          throw new FS.ErrnoError(28);
        }
        var seeking = typeof position != "undefined";
        if (!seeking) {
          position = stream.position;
        } else if (!stream.seekable) {
          throw new FS.ErrnoError(70);
        }
        var bytesRead = stream.stream_ops.read(stream, buffer, offset, length, position);
        if (!seeking) stream.position += bytesRead;
        return bytesRead;
      },
      write(stream, buffer, offset, length, position, canOwn) {
        assert(offset >= 0);
        if (length < 0 || position < 0) {
          throw new FS.ErrnoError(28);
        }
        if (FS.isClosed(stream)) {
          throw new FS.ErrnoError(8);
        }
        if ((stream.flags & 2097155) === 0) {
          throw new FS.ErrnoError(8);
        }
        if (FS.isDir(stream.node.mode)) {
          throw new FS.ErrnoError(31);
        }
        if (!stream.stream_ops.write) {
          throw new FS.ErrnoError(28);
        }
        if (stream.seekable && stream.flags & 1024) {
          FS.llseek(stream, 0, 2);
        }
        var seeking = typeof position != "undefined";
        if (!seeking) {
          position = stream.position;
        } else if (!stream.seekable) {
          throw new FS.ErrnoError(70);
        }
        var bytesWritten = stream.stream_ops.write(stream, buffer, offset, length, position, canOwn);
        if (!seeking) stream.position += bytesWritten;
        return bytesWritten;
      },
      allocate(stream, offset, length) {
        if (FS.isClosed(stream)) {
          throw new FS.ErrnoError(8);
        }
        if (offset < 0 || length <= 0) {
          throw new FS.ErrnoError(28);
        }
        if ((stream.flags & 2097155) === 0) {
          throw new FS.ErrnoError(8);
        }
        if (!FS.isFile(stream.node.mode) && !FS.isDir(stream.node.mode)) {
          throw new FS.ErrnoError(43);
        }
        if (!stream.stream_ops.allocate) {
          throw new FS.ErrnoError(138);
        }
        stream.stream_ops.allocate(stream, offset, length);
      },
      mmap(stream, length, position, prot, flags) {
        if ((prot & 2) !== 0 && (flags & 2) === 0 && (stream.flags & 2097155) !== 2) {
          throw new FS.ErrnoError(2);
        }
        if ((stream.flags & 2097155) === 1) {
          throw new FS.ErrnoError(2);
        }
        if (!stream.stream_ops.mmap) {
          throw new FS.ErrnoError(43);
        }
        return stream.stream_ops.mmap(stream, length, position, prot, flags);
      },
      msync(stream, buffer, offset, length, mmapFlags) {
        assert(offset >= 0);
        if (!stream.stream_ops.msync) {
          return 0;
        }
        return stream.stream_ops.msync(stream, buffer, offset, length, mmapFlags);
      },
      ioctl(stream, cmd, arg) {
        if (!stream.stream_ops.ioctl) {
          throw new FS.ErrnoError(59);
        }
        return stream.stream_ops.ioctl(stream, cmd, arg);
      },
      readFile(path, opts = {}) {
        opts.flags = opts.flags || 0;
        opts.encoding = opts.encoding || "binary";
        if (opts.encoding !== "utf8" && opts.encoding !== "binary") {
          throw new Error(`Invalid encoding type "${opts.encoding}"`);
        }
        var ret;
        var stream = FS.open(path, opts.flags);
        var stat = FS.stat(path);
        var length = stat.size;
        var buf = new Uint8Array(length);
        FS.read(stream, buf, 0, length, 0);
        if (opts.encoding === "utf8") {
          ret = UTF8ArrayToString(buf, 0);
        } else if (opts.encoding === "binary") {
          ret = buf;
        }
        FS.close(stream);
        return ret;
      },
      writeFile(path, data, opts = {}) {
        opts.flags = opts.flags || 577;
        var stream = FS.open(path, opts.flags, opts.mode);
        if (typeof data == "string") {
          var buf = new Uint8Array(lengthBytesUTF8(data) + 1);
          var actualNumBytes = stringToUTF8Array(data, buf, 0, buf.length);
          FS.write(stream, buf, 0, actualNumBytes, void 0, opts.canOwn);
        } else if (ArrayBuffer.isView(data)) {
          FS.write(stream, data, 0, data.byteLength, void 0, opts.canOwn);
        } else {
          throw new Error("Unsupported data type");
        }
        FS.close(stream);
      },
      cwd: () => FS.currentPath,
      chdir(path) {
        var lookup = FS.lookupPath(path, {
          follow: true
        });
        if (lookup.node === null) {
          throw new FS.ErrnoError(44);
        }
        if (!FS.isDir(lookup.node.mode)) {
          throw new FS.ErrnoError(54);
        }
        var errCode = FS.nodePermissions(lookup.node, "x");
        if (errCode) {
          throw new FS.ErrnoError(errCode);
        }
        FS.currentPath = lookup.path;
      },
      createDefaultDirectories() {
        FS.mkdir("/tmp");
        FS.mkdir("/home");
        FS.mkdir("/home/web_user");
      },
      createDefaultDevices() {
        FS.mkdir("/dev");
        FS.registerDevice(FS.makedev(1, 3), {
          read: () => 0,
          write: (stream, buffer, offset, length, pos) => length
        });
        FS.mkdev("/dev/null", FS.makedev(1, 3));
        TTY.register(FS.makedev(5, 0), TTY.default_tty_ops);
        TTY.register(FS.makedev(6, 0), TTY.default_tty1_ops);
        FS.mkdev("/dev/tty", FS.makedev(5, 0));
        FS.mkdev("/dev/tty1", FS.makedev(6, 0));
        var randomBuffer = new Uint8Array(1024), randomLeft = 0;
        var randomByte = () => {
          if (randomLeft === 0) {
            randomLeft = randomFill(randomBuffer).byteLength;
          }
          return randomBuffer[--randomLeft];
        };
        FS.createDevice("/dev", "random", randomByte);
        FS.createDevice("/dev", "urandom", randomByte);
        FS.mkdir("/dev/shm");
        FS.mkdir("/dev/shm/tmp");
      },
      createSpecialDirectories() {
        FS.mkdir("/proc");
        var proc_self = FS.mkdir("/proc/self");
        FS.mkdir("/proc/self/fd");
        FS.mount({
          mount() {
            var node = FS.createNode(
              proc_self,
              "fd",
              16384 | 511,
              /* 0777 */
              73
            );
            node.node_ops = {
              lookup(parent, name) {
                var fd = +name;
                var stream = FS.getStreamChecked(fd);
                var ret = {
                  parent: null,
                  mount: {
                    mountpoint: "fake"
                  },
                  node_ops: {
                    readlink: () => stream.path
                  }
                };
                ret.parent = ret;
                return ret;
              }
            };
            return node;
          }
        }, {}, "/proc/self/fd");
      },
      createStandardStreams() {
        if (Module["stdin"]) {
          FS.createDevice("/dev", "stdin", Module["stdin"]);
        } else {
          FS.symlink("/dev/tty", "/dev/stdin");
        }
        if (Module["stdout"]) {
          FS.createDevice("/dev", "stdout", null, Module["stdout"]);
        } else {
          FS.symlink("/dev/tty", "/dev/stdout");
        }
        if (Module["stderr"]) {
          FS.createDevice("/dev", "stderr", null, Module["stderr"]);
        } else {
          FS.symlink("/dev/tty1", "/dev/stderr");
        }
        var stdin = FS.open("/dev/stdin", 0);
        var stdout = FS.open("/dev/stdout", 1);
        var stderr = FS.open("/dev/stderr", 1);
        assert(stdin.fd === 0, `invalid handle for stdin (${stdin.fd})`);
        assert(stdout.fd === 1, `invalid handle for stdout (${stdout.fd})`);
        assert(stderr.fd === 2, `invalid handle for stderr (${stderr.fd})`);
      },
      staticInit() {
        [44].forEach((code) => {
          FS.genericErrors[code] = new FS.ErrnoError(code);
          FS.genericErrors[code].stack = "<generic error, no stack>";
        });
        FS.nameTable = new Array(4096);
        FS.mount(MEMFS, {}, "/");
        FS.createDefaultDirectories();
        FS.createDefaultDevices();
        FS.createSpecialDirectories();
        FS.filesystems = {
          "MEMFS": MEMFS
        };
      },
      init(input, output, error) {
        assert(!FS.init.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)");
        FS.init.initialized = true;
        Module["stdin"] = input || Module["stdin"];
        Module["stdout"] = output || Module["stdout"];
        Module["stderr"] = error || Module["stderr"];
        FS.createStandardStreams();
      },
      quit() {
        FS.init.initialized = false;
        _fflush(0);
        for (var i = 0; i < FS.streams.length; i++) {
          var stream = FS.streams[i];
          if (!stream) {
            continue;
          }
          FS.close(stream);
        }
      },
      findObject(path, dontResolveLastLink) {
        var ret = FS.analyzePath(path, dontResolveLastLink);
        if (!ret.exists) {
          return null;
        }
        return ret.object;
      },
      analyzePath(path, dontResolveLastLink) {
        try {
          var lookup = FS.lookupPath(path, {
            follow: !dontResolveLastLink
          });
          path = lookup.path;
        } catch (e) {
        }
        var ret = {
          isRoot: false,
          exists: false,
          error: 0,
          name: null,
          path: null,
          object: null,
          parentExists: false,
          parentPath: null,
          parentObject: null
        };
        try {
          var lookup = FS.lookupPath(path, {
            parent: true
          });
          ret.parentExists = true;
          ret.parentPath = lookup.path;
          ret.parentObject = lookup.node;
          ret.name = PATH.basename(path);
          lookup = FS.lookupPath(path, {
            follow: !dontResolveLastLink
          });
          ret.exists = true;
          ret.path = lookup.path;
          ret.object = lookup.node;
          ret.name = lookup.node.name;
          ret.isRoot = lookup.path === "/";
        } catch (e) {
          ret.error = e.errno;
        }
        return ret;
      },
      createPath(parent, path, canRead, canWrite) {
        parent = typeof parent == "string" ? parent : FS.getPath(parent);
        var parts = path.split("/").reverse();
        while (parts.length) {
          var part = parts.pop();
          if (!part) continue;
          var current = PATH.join2(parent, part);
          try {
            FS.mkdir(current);
          } catch (e) {
          }
          parent = current;
        }
        return current;
      },
      createFile(parent, name, properties, canRead, canWrite) {
        var path = PATH.join2(typeof parent == "string" ? parent : FS.getPath(parent), name);
        var mode = FS_getMode(canRead, canWrite);
        return FS.create(path, mode);
      },
      createDataFile(parent, name, data, canRead, canWrite, canOwn) {
        var path = name;
        if (parent) {
          parent = typeof parent == "string" ? parent : FS.getPath(parent);
          path = name ? PATH.join2(parent, name) : parent;
        }
        var mode = FS_getMode(canRead, canWrite);
        var node = FS.create(path, mode);
        if (data) {
          if (typeof data == "string") {
            var arr = new Array(data.length);
            for (var i = 0, len = data.length; i < len; ++i) arr[i] = data.charCodeAt(i);
            data = arr;
          }
          FS.chmod(node, mode | 146);
          var stream = FS.open(node, 577);
          FS.write(stream, data, 0, data.length, 0, canOwn);
          FS.close(stream);
          FS.chmod(node, mode);
        }
      },
      createDevice(parent, name, input, output) {
        var path = PATH.join2(typeof parent == "string" ? parent : FS.getPath(parent), name);
        var mode = FS_getMode(!!input, !!output);
        if (!FS.createDevice.major) FS.createDevice.major = 64;
        var dev = FS.makedev(FS.createDevice.major++, 0);
        FS.registerDevice(dev, {
          open(stream) {
            stream.seekable = false;
          },
          close(stream) {
            if (output?.buffer?.length) {
              output(10);
            }
          },
          read(stream, buffer, offset, length, pos) {
            var bytesRead = 0;
            for (var i = 0; i < length; i++) {
              var result;
              try {
                result = input();
              } catch (e) {
                throw new FS.ErrnoError(29);
              }
              if (result === void 0 && bytesRead === 0) {
                throw new FS.ErrnoError(6);
              }
              if (result === null || result === void 0) break;
              bytesRead++;
              buffer[offset + i] = result;
            }
            if (bytesRead) {
              stream.node.timestamp = Date.now();
            }
            return bytesRead;
          },
          write(stream, buffer, offset, length, pos) {
            for (var i = 0; i < length; i++) {
              try {
                output(buffer[offset + i]);
              } catch (e) {
                throw new FS.ErrnoError(29);
              }
            }
            if (length) {
              stream.node.timestamp = Date.now();
            }
            return i;
          }
        });
        return FS.mkdev(path, mode, dev);
      },
      forceLoadFile(obj) {
        if (obj.isDevice || obj.isFolder || obj.link || obj.contents) return true;
        if (typeof XMLHttpRequest != "undefined") {
          throw new Error("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
        } else if (read_) {
          try {
            obj.contents = intArrayFromString(read_(obj.url), true);
            obj.usedBytes = obj.contents.length;
          } catch (e) {
            throw new FS.ErrnoError(29);
          }
        } else {
          throw new Error("Cannot load without read() or XMLHttpRequest.");
        }
      },
      createLazyFile(parent, name, url, canRead, canWrite) {
        class LazyUint8Array {
          constructor() {
            this.lengthKnown = false;
            this.chunks = [];
          }
          get(idx) {
            if (idx > this.length - 1 || idx < 0) {
              return void 0;
            }
            var chunkOffset = idx % this.chunkSize;
            var chunkNum = idx / this.chunkSize | 0;
            return this.getter(chunkNum)[chunkOffset];
          }
          setDataGetter(getter) {
            this.getter = getter;
          }
          cacheLength() {
            var xhr = new XMLHttpRequest();
            xhr.open("HEAD", url, false);
            xhr.send(null);
            if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
            var datalength = Number(xhr.getResponseHeader("Content-length"));
            var header;
            var hasByteServing = (header = xhr.getResponseHeader("Accept-Ranges")) && header === "bytes";
            var usesGzip = (header = xhr.getResponseHeader("Content-Encoding")) && header === "gzip";
            var chunkSize = 1024 * 1024;
            if (!hasByteServing) chunkSize = datalength;
            var doXHR = (from, to) => {
              if (from > to) throw new Error("invalid range (" + from + ", " + to + ") or no bytes requested!");
              if (to > datalength - 1) throw new Error("only " + datalength + " bytes available! programmer error!");
              var xhr2 = new XMLHttpRequest();
              xhr2.open("GET", url, false);
              if (datalength !== chunkSize) xhr2.setRequestHeader("Range", "bytes=" + from + "-" + to);
              xhr2.responseType = "arraybuffer";
              if (xhr2.overrideMimeType) {
                xhr2.overrideMimeType("text/plain; charset=x-user-defined");
              }
              xhr2.send(null);
              if (!(xhr2.status >= 200 && xhr2.status < 300 || xhr2.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr2.status);
              if (xhr2.response !== void 0) {
                return new Uint8Array(
                  /** @type{Array<number>} */
                  xhr2.response || []
                );
              }
              return intArrayFromString(xhr2.responseText || "", true);
            };
            var lazyArray2 = this;
            lazyArray2.setDataGetter((chunkNum) => {
              var start = chunkNum * chunkSize;
              var end = (chunkNum + 1) * chunkSize - 1;
              end = Math.min(end, datalength - 1);
              if (typeof lazyArray2.chunks[chunkNum] == "undefined") {
                lazyArray2.chunks[chunkNum] = doXHR(start, end);
              }
              if (typeof lazyArray2.chunks[chunkNum] == "undefined") throw new Error("doXHR failed!");
              return lazyArray2.chunks[chunkNum];
            });
            if (usesGzip || !datalength) {
              chunkSize = datalength = 1;
              datalength = this.getter(0).length;
              chunkSize = datalength;
              out("LazyFiles on gzip forces download of the whole file when length is accessed");
            }
            this._length = datalength;
            this._chunkSize = chunkSize;
            this.lengthKnown = true;
          }
          get length() {
            if (!this.lengthKnown) {
              this.cacheLength();
            }
            return this._length;
          }
          get chunkSize() {
            if (!this.lengthKnown) {
              this.cacheLength();
            }
            return this._chunkSize;
          }
        }
        if (typeof XMLHttpRequest != "undefined") {
          if (!ENVIRONMENT_IS_WORKER) throw "Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc";
          var lazyArray = new LazyUint8Array();
          var properties = {
            isDevice: false,
            contents: lazyArray
          };
        } else {
          var properties = {
            isDevice: false,
            url
          };
        }
        var node = FS.createFile(parent, name, properties, canRead, canWrite);
        if (properties.contents) {
          node.contents = properties.contents;
        } else if (properties.url) {
          node.contents = null;
          node.url = properties.url;
        }
        Object.defineProperties(node, {
          usedBytes: {
            get: function() {
              return this.contents.length;
            }
          }
        });
        var stream_ops = {};
        var keys = Object.keys(node.stream_ops);
        keys.forEach((key) => {
          var fn = node.stream_ops[key];
          stream_ops[key] = (...args) => {
            FS.forceLoadFile(node);
            return fn(...args);
          };
        });
        function writeChunks(stream, buffer, offset, length, position) {
          var contents = stream.node.contents;
          if (position >= contents.length) return 0;
          var size = Math.min(contents.length - position, length);
          assert(size >= 0);
          if (contents.slice) {
            for (var i = 0; i < size; i++) {
              buffer[offset + i] = contents[position + i];
            }
          } else {
            for (var i = 0; i < size; i++) {
              buffer[offset + i] = contents.get(position + i);
            }
          }
          return size;
        }
        stream_ops.read = (stream, buffer, offset, length, position) => {
          FS.forceLoadFile(node);
          return writeChunks(stream, buffer, offset, length, position);
        };
        stream_ops.mmap = (stream, length, position, prot, flags) => {
          FS.forceLoadFile(node);
          var ptr = mmapAlloc(length);
          if (!ptr) {
            throw new FS.ErrnoError(48);
          }
          writeChunks(stream, HEAP8, ptr, length, position);
          return {
            ptr,
            allocated: true
          };
        };
        node.stream_ops = stream_ops;
        return node;
      },
      absolutePath() {
        abort("FS.absolutePath has been removed; use PATH_FS.resolve instead");
      },
      createFolder() {
        abort("FS.createFolder has been removed; use FS.mkdir instead");
      },
      createLink() {
        abort("FS.createLink has been removed; use FS.symlink instead");
      },
      joinPath() {
        abort("FS.joinPath has been removed; use PATH.join instead");
      },
      mmapAlloc() {
        abort("FS.mmapAlloc has been replaced by the top level function mmapAlloc");
      },
      standardizePath() {
        abort("FS.standardizePath has been removed; use PATH.normalize instead");
      }
    };
    var SYSCALLS = {
      DEFAULT_POLLMASK: 5,
      calculateAt(dirfd, path, allowEmpty) {
        if (PATH.isAbs(path)) {
          return path;
        }
        var dir;
        if (dirfd === -100) {
          dir = FS.cwd();
        } else {
          var dirstream = SYSCALLS.getStreamFromFD(dirfd);
          dir = dirstream.path;
        }
        if (path.length == 0) {
          if (!allowEmpty) {
            throw new FS.ErrnoError(44);
          }
          return dir;
        }
        return PATH.join2(dir, path);
      },
      doStat(func, path, buf) {
        var stat = func(path);
        SAFE_HEAP_STORE((buf >> 2) * 4, stat.dev, 4);
        SAFE_HEAP_STORE((buf + 4 >> 2) * 4, stat.mode, 4);
        SAFE_HEAP_STORE((buf + 8 >> 2) * 4, stat.nlink, 4);
        SAFE_HEAP_STORE((buf + 12 >> 2) * 4, stat.uid, 4);
        SAFE_HEAP_STORE((buf + 16 >> 2) * 4, stat.gid, 4);
        SAFE_HEAP_STORE((buf + 20 >> 2) * 4, stat.rdev, 4);
        tempI64 = [stat.size >>> 0, (tempDouble = stat.size, +Math.abs(tempDouble) >= 1 ? tempDouble > 0 ? +Math.floor(tempDouble / 4294967296) >>> 0 : ~~+Math.ceil((tempDouble - +(~~tempDouble >>> 0)) / 4294967296) >>> 0 : 0)], SAFE_HEAP_STORE((buf + 24 >> 2) * 4, tempI64[0], 4), SAFE_HEAP_STORE((buf + 28 >> 2) * 4, tempI64[1], 4);
        SAFE_HEAP_STORE((buf + 32 >> 2) * 4, 4096, 4);
        SAFE_HEAP_STORE((buf + 36 >> 2) * 4, stat.blocks, 4);
        var atime = stat.atime.getTime();
        var mtime = stat.mtime.getTime();
        var ctime = stat.ctime.getTime();
        tempI64 = [Math.floor(atime / 1e3) >>> 0, (tempDouble = Math.floor(atime / 1e3), +Math.abs(tempDouble) >= 1 ? tempDouble > 0 ? +Math.floor(tempDouble / 4294967296) >>> 0 : ~~+Math.ceil((tempDouble - +(~~tempDouble >>> 0)) / 4294967296) >>> 0 : 0)], SAFE_HEAP_STORE((buf + 40 >> 2) * 4, tempI64[0], 4), SAFE_HEAP_STORE((buf + 44 >> 2) * 4, tempI64[1], 4);
        SAFE_HEAP_STORE((buf + 48 >> 2) * 4, atime % 1e3 * 1e3, 4);
        tempI64 = [Math.floor(mtime / 1e3) >>> 0, (tempDouble = Math.floor(mtime / 1e3), +Math.abs(tempDouble) >= 1 ? tempDouble > 0 ? +Math.floor(tempDouble / 4294967296) >>> 0 : ~~+Math.ceil((tempDouble - +(~~tempDouble >>> 0)) / 4294967296) >>> 0 : 0)], SAFE_HEAP_STORE((buf + 56 >> 2) * 4, tempI64[0], 4), SAFE_HEAP_STORE((buf + 60 >> 2) * 4, tempI64[1], 4);
        SAFE_HEAP_STORE((buf + 64 >> 2) * 4, mtime % 1e3 * 1e3, 4);
        tempI64 = [Math.floor(ctime / 1e3) >>> 0, (tempDouble = Math.floor(ctime / 1e3), +Math.abs(tempDouble) >= 1 ? tempDouble > 0 ? +Math.floor(tempDouble / 4294967296) >>> 0 : ~~+Math.ceil((tempDouble - +(~~tempDouble >>> 0)) / 4294967296) >>> 0 : 0)], SAFE_HEAP_STORE((buf + 72 >> 2) * 4, tempI64[0], 4), SAFE_HEAP_STORE((buf + 76 >> 2) * 4, tempI64[1], 4);
        SAFE_HEAP_STORE((buf + 80 >> 2) * 4, ctime % 1e3 * 1e3, 4);
        tempI64 = [stat.ino >>> 0, (tempDouble = stat.ino, +Math.abs(tempDouble) >= 1 ? tempDouble > 0 ? +Math.floor(tempDouble / 4294967296) >>> 0 : ~~+Math.ceil((tempDouble - +(~~tempDouble >>> 0)) / 4294967296) >>> 0 : 0)], SAFE_HEAP_STORE((buf + 88 >> 2) * 4, tempI64[0], 4), SAFE_HEAP_STORE((buf + 92 >> 2) * 4, tempI64[1], 4);
        return 0;
      },
      doMsync(addr, stream, len, flags, offset) {
        if (!FS.isFile(stream.node.mode)) {
          throw new FS.ErrnoError(43);
        }
        if (flags & 2) {
          return 0;
        }
        var buffer = HEAPU8.slice(addr, addr + len);
        FS.msync(stream, buffer, offset, len, flags);
      },
      getStreamFromFD(fd) {
        var stream = FS.getStreamChecked(fd);
        return stream;
      },
      varargs: void 0,
      getStr(ptr) {
        var ret = UTF8ToString(ptr);
        return ret;
      }
    };
    function ___syscall_fcntl64(fd, cmd, varargs) {
      SYSCALLS.varargs = varargs;
      try {
        var stream = SYSCALLS.getStreamFromFD(fd);
        switch (cmd) {
          case 0: {
            var arg = syscallGetVarargI();
            if (arg < 0) {
              return -28;
            }
            while (FS.streams[arg]) {
              arg++;
            }
            var newStream;
            newStream = FS.dupStream(stream, arg);
            return newStream.fd;
          }
          case 1:
          case 2:
            return 0;
          case 3:
            return stream.flags;
          case 4: {
            var arg = syscallGetVarargI();
            stream.flags |= arg;
            return 0;
          }
          case 12: {
            var arg = syscallGetVarargP();
            var offset = 0;
            SAFE_HEAP_STORE((arg + offset >> 1) * 2, 2, 2);
            return 0;
          }
          case 13:
          case 14:
            return 0;
        }
        return -28;
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    function ___syscall_fstat64(fd, buf) {
      try {
        var stream = SYSCALLS.getStreamFromFD(fd);
        return SYSCALLS.doStat(FS.stat, stream.path, buf);
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    function ___syscall_ioctl(fd, op, varargs) {
      SYSCALLS.varargs = varargs;
      try {
        var stream = SYSCALLS.getStreamFromFD(fd);
        switch (op) {
          case 21509: {
            if (!stream.tty) return -59;
            return 0;
          }
          case 21505: {
            if (!stream.tty) return -59;
            if (stream.tty.ops.ioctl_tcgets) {
              var termios = stream.tty.ops.ioctl_tcgets(stream);
              var argp = syscallGetVarargP();
              SAFE_HEAP_STORE((argp >> 2) * 4, termios.c_iflag || 0, 4);
              SAFE_HEAP_STORE((argp + 4 >> 2) * 4, termios.c_oflag || 0, 4);
              SAFE_HEAP_STORE((argp + 8 >> 2) * 4, termios.c_cflag || 0, 4);
              SAFE_HEAP_STORE((argp + 12 >> 2) * 4, termios.c_lflag || 0, 4);
              for (var i = 0; i < 32; i++) {
                SAFE_HEAP_STORE(argp + i + 17, termios.c_cc[i] || 0, 1);
              }
              return 0;
            }
            return 0;
          }
          case 21510:
          case 21511:
          case 21512: {
            if (!stream.tty) return -59;
            return 0;
          }
          case 21506:
          case 21507:
          case 21508: {
            if (!stream.tty) return -59;
            if (stream.tty.ops.ioctl_tcsets) {
              var argp = syscallGetVarargP();
              var c_iflag = SAFE_HEAP_LOAD((argp >> 2) * 4, 4, 0);
              var c_oflag = SAFE_HEAP_LOAD((argp + 4 >> 2) * 4, 4, 0);
              var c_cflag = SAFE_HEAP_LOAD((argp + 8 >> 2) * 4, 4, 0);
              var c_lflag = SAFE_HEAP_LOAD((argp + 12 >> 2) * 4, 4, 0);
              var c_cc = [];
              for (var i = 0; i < 32; i++) {
                c_cc.push(SAFE_HEAP_LOAD(argp + i + 17, 1, 0));
              }
              return stream.tty.ops.ioctl_tcsets(stream.tty, op, {
                c_iflag,
                c_oflag,
                c_cflag,
                c_lflag,
                c_cc
              });
            }
            return 0;
          }
          case 21519: {
            if (!stream.tty) return -59;
            var argp = syscallGetVarargP();
            SAFE_HEAP_STORE((argp >> 2) * 4, 0, 4);
            return 0;
          }
          case 21520: {
            if (!stream.tty) return -59;
            return -28;
          }
          case 21531: {
            var argp = syscallGetVarargP();
            return FS.ioctl(stream, op, argp);
          }
          case 21523: {
            if (!stream.tty) return -59;
            if (stream.tty.ops.ioctl_tiocgwinsz) {
              var winsize = stream.tty.ops.ioctl_tiocgwinsz(stream.tty);
              var argp = syscallGetVarargP();
              SAFE_HEAP_STORE((argp >> 1) * 2, winsize[0], 2);
              SAFE_HEAP_STORE((argp + 2 >> 1) * 2, winsize[1], 2);
            }
            return 0;
          }
          case 21524: {
            if (!stream.tty) return -59;
            return 0;
          }
          case 21515: {
            if (!stream.tty) return -59;
            return 0;
          }
          default:
            return -28;
        }
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    function ___syscall_lstat64(path, buf) {
      try {
        path = SYSCALLS.getStr(path);
        return SYSCALLS.doStat(FS.lstat, path, buf);
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    function ___syscall_newfstatat(dirfd, path, buf, flags) {
      try {
        path = SYSCALLS.getStr(path);
        var nofollow = flags & 256;
        var allowEmpty = flags & 4096;
        flags = flags & ~6400;
        assert(!flags, `unknown flags in __syscall_newfstatat: ${flags}`);
        path = SYSCALLS.calculateAt(dirfd, path, allowEmpty);
        return SYSCALLS.doStat(nofollow ? FS.lstat : FS.stat, path, buf);
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    function ___syscall_openat(dirfd, path, flags, varargs) {
      SYSCALLS.varargs = varargs;
      try {
        path = SYSCALLS.getStr(path);
        path = SYSCALLS.calculateAt(dirfd, path);
        var mode = varargs ? syscallGetVarargI() : 0;
        return FS.open(path, flags, mode).fd;
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    function ___syscall_stat64(path, buf) {
      try {
        path = SYSCALLS.getStr(path);
        return SYSCALLS.doStat(FS.stat, path, buf);
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    var structRegistrations = {};
    var runDestructors = (destructors) => {
      while (destructors.length) {
        var ptr = destructors.pop();
        var del = destructors.pop();
        del(ptr);
      }
    };
    function readPointer(pointer) {
      return this["fromWireType"](SAFE_HEAP_LOAD((pointer >> 2) * 4, 4, 1));
    }
    var awaitingDependencies = {};
    var registeredTypes = {};
    var typeDependencies = {};
    var InternalError;
    var throwInternalError = (message) => {
      throw new InternalError(message);
    };
    var whenDependentTypesAreResolved = (myTypes, dependentTypes, getTypeConverters) => {
      myTypes.forEach(function(type) {
        typeDependencies[type] = dependentTypes;
      });
      function onComplete(typeConverters2) {
        var myTypeConverters = getTypeConverters(typeConverters2);
        if (myTypeConverters.length !== myTypes.length) {
          throwInternalError("Mismatched type converter count");
        }
        for (var i = 0; i < myTypes.length; ++i) {
          registerType(myTypes[i], myTypeConverters[i]);
        }
      }
      var typeConverters = new Array(dependentTypes.length);
      var unregisteredTypes = [];
      var registered = 0;
      dependentTypes.forEach((dt, i) => {
        if (registeredTypes.hasOwnProperty(dt)) {
          typeConverters[i] = registeredTypes[dt];
        } else {
          unregisteredTypes.push(dt);
          if (!awaitingDependencies.hasOwnProperty(dt)) {
            awaitingDependencies[dt] = [];
          }
          awaitingDependencies[dt].push(() => {
            typeConverters[i] = registeredTypes[dt];
            ++registered;
            if (registered === unregisteredTypes.length) {
              onComplete(typeConverters);
            }
          });
        }
      });
      if (0 === unregisteredTypes.length) {
        onComplete(typeConverters);
      }
    };
    var __embind_finalize_value_object = (structType) => {
      var reg = structRegistrations[structType];
      delete structRegistrations[structType];
      var rawConstructor = reg.rawConstructor;
      var rawDestructor = reg.rawDestructor;
      var fieldRecords = reg.fields;
      var fieldTypes = fieldRecords.map((field) => field.getterReturnType).concat(fieldRecords.map((field) => field.setterArgumentType));
      whenDependentTypesAreResolved([structType], fieldTypes, (fieldTypes2) => {
        var fields = {};
        fieldRecords.forEach((field, i) => {
          var fieldName = field.fieldName;
          var getterReturnType = fieldTypes2[i];
          var getter = field.getter;
          var getterContext = field.getterContext;
          var setterArgumentType = fieldTypes2[i + fieldRecords.length];
          var setter = field.setter;
          var setterContext = field.setterContext;
          fields[fieldName] = {
            read: (ptr) => getterReturnType["fromWireType"](getter(getterContext, ptr)),
            write: (ptr, o) => {
              var destructors = [];
              setter(setterContext, ptr, setterArgumentType["toWireType"](destructors, o));
              runDestructors(destructors);
            }
          };
        });
        return [{
          name: reg.name,
          "fromWireType": (ptr) => {
            var rv = {};
            for (var i in fields) {
              rv[i] = fields[i].read(ptr);
            }
            rawDestructor(ptr);
            return rv;
          },
          "toWireType": (destructors, o) => {
            for (var fieldName in fields) {
              if (!(fieldName in o)) {
                throw new TypeError(`Missing field: "${fieldName}"`);
              }
            }
            var ptr = rawConstructor();
            for (fieldName in fields) {
              fields[fieldName].write(ptr, o[fieldName]);
            }
            if (destructors !== null) {
              destructors.push(rawDestructor, ptr);
            }
            return ptr;
          },
          "argPackAdvance": GenericWireTypeSize,
          "readValueFromPointer": readPointer,
          destructorFunction: rawDestructor
        }];
      });
    };
    var __embind_register_bigint = (primitiveType, name, size, minRange, maxRange) => {
    };
    var embind_init_charCodes = () => {
      var codes = new Array(256);
      for (var i = 0; i < 256; ++i) {
        codes[i] = String.fromCharCode(i);
      }
      embind_charCodes = codes;
    };
    var embind_charCodes;
    var readLatin1String = (ptr) => {
      var ret = "";
      var c = ptr;
      while (SAFE_HEAP_LOAD(c, 1, 1)) {
        ret += embind_charCodes[SAFE_HEAP_LOAD(c++, 1, 1)];
      }
      return ret;
    };
    var BindingError;
    var throwBindingError = (message) => {
      throw new BindingError(message);
    };
    function sharedRegisterType(rawType, registeredInstance, options = {}) {
      var name = registeredInstance.name;
      if (!rawType) {
        throwBindingError(`type "${name}" must have a positive integer typeid pointer`);
      }
      if (registeredTypes.hasOwnProperty(rawType)) {
        if (options.ignoreDuplicateRegistrations) {
          return;
        } else {
          throwBindingError(`Cannot register type '${name}' twice`);
        }
      }
      registeredTypes[rawType] = registeredInstance;
      delete typeDependencies[rawType];
      if (awaitingDependencies.hasOwnProperty(rawType)) {
        var callbacks = awaitingDependencies[rawType];
        delete awaitingDependencies[rawType];
        callbacks.forEach((cb) => cb());
      }
    }
    function registerType(rawType, registeredInstance, options = {}) {
      if (!("argPackAdvance" in registeredInstance)) {
        throw new TypeError("registerType registeredInstance requires argPackAdvance");
      }
      return sharedRegisterType(rawType, registeredInstance, options);
    }
    var GenericWireTypeSize = 8;
    var __embind_register_bool = (rawType, name, trueValue, falseValue) => {
      name = readLatin1String(name);
      registerType(rawType, {
        name,
        "fromWireType": function(wt) {
          return !!wt;
        },
        "toWireType": function(destructors, o) {
          return o ? trueValue : falseValue;
        },
        "argPackAdvance": GenericWireTypeSize,
        "readValueFromPointer": function(pointer) {
          return this["fromWireType"](SAFE_HEAP_LOAD(pointer, 1, 1));
        },
        destructorFunction: null
      });
    };
    var shallowCopyInternalPointer = (o) => ({
      count: o.count,
      deleteScheduled: o.deleteScheduled,
      preservePointerOnDelete: o.preservePointerOnDelete,
      ptr: o.ptr,
      ptrType: o.ptrType,
      smartPtr: o.smartPtr,
      smartPtrType: o.smartPtrType
    });
    var throwInstanceAlreadyDeleted = (obj) => {
      function getInstanceTypeName(handle) {
        return handle.$$.ptrType.registeredClass.name;
      }
      throwBindingError(getInstanceTypeName(obj) + " instance already deleted");
    };
    var finalizationRegistry = false;
    var detachFinalizer = (handle) => {
    };
    var runDestructor = ($$) => {
      if ($$.smartPtr) {
        $$.smartPtrType.rawDestructor($$.smartPtr);
      } else {
        $$.ptrType.registeredClass.rawDestructor($$.ptr);
      }
    };
    var releaseClassHandle = ($$) => {
      $$.count.value -= 1;
      var toDelete = 0 === $$.count.value;
      if (toDelete) {
        runDestructor($$);
      }
    };
    var downcastPointer = (ptr, ptrClass, desiredClass) => {
      if (ptrClass === desiredClass) {
        return ptr;
      }
      if (void 0 === desiredClass.baseClass) {
        return null;
      }
      var rv = downcastPointer(ptr, ptrClass, desiredClass.baseClass);
      if (rv === null) {
        return null;
      }
      return desiredClass.downcast(rv);
    };
    var registeredPointers = {};
    var getInheritedInstanceCount = () => Object.keys(registeredInstances).length;
    var getLiveInheritedInstances = () => {
      var rv = [];
      for (var k in registeredInstances) {
        if (registeredInstances.hasOwnProperty(k)) {
          rv.push(registeredInstances[k]);
        }
      }
      return rv;
    };
    var deletionQueue = [];
    var flushPendingDeletes = () => {
      while (deletionQueue.length) {
        var obj = deletionQueue.pop();
        obj.$$.deleteScheduled = false;
        obj["delete"]();
      }
    };
    var delayFunction;
    var setDelayFunction = (fn) => {
      delayFunction = fn;
      if (deletionQueue.length && delayFunction) {
        delayFunction(flushPendingDeletes);
      }
    };
    var init_embind = () => {
      Module["getInheritedInstanceCount"] = getInheritedInstanceCount;
      Module["getLiveInheritedInstances"] = getLiveInheritedInstances;
      Module["flushPendingDeletes"] = flushPendingDeletes;
      Module["setDelayFunction"] = setDelayFunction;
    };
    var registeredInstances = {};
    var getBasestPointer = (class_, ptr) => {
      if (ptr === void 0) {
        throwBindingError("ptr should not be undefined");
      }
      while (class_.baseClass) {
        ptr = class_.upcast(ptr);
        class_ = class_.baseClass;
      }
      return ptr;
    };
    var getInheritedInstance = (class_, ptr) => {
      ptr = getBasestPointer(class_, ptr);
      return registeredInstances[ptr];
    };
    var makeClassHandle = (prototype, record) => {
      if (!record.ptrType || !record.ptr) {
        throwInternalError("makeClassHandle requires ptr and ptrType");
      }
      var hasSmartPtrType = !!record.smartPtrType;
      var hasSmartPtr = !!record.smartPtr;
      if (hasSmartPtrType !== hasSmartPtr) {
        throwInternalError("Both smartPtrType and smartPtr must be specified");
      }
      record.count = {
        value: 1
      };
      return attachFinalizer(Object.create(prototype, {
        $$: {
          value: record,
          writable: true
        }
      }));
    };
    function RegisteredPointer_fromWireType(ptr) {
      var rawPointer = this.getPointee(ptr);
      if (!rawPointer) {
        this.destructor(ptr);
        return null;
      }
      var registeredInstance = getInheritedInstance(this.registeredClass, rawPointer);
      if (void 0 !== registeredInstance) {
        if (0 === registeredInstance.$$.count.value) {
          registeredInstance.$$.ptr = rawPointer;
          registeredInstance.$$.smartPtr = ptr;
          return registeredInstance["clone"]();
        } else {
          var rv = registeredInstance["clone"]();
          this.destructor(ptr);
          return rv;
        }
      }
      function makeDefaultHandle() {
        if (this.isSmartPointer) {
          return makeClassHandle(this.registeredClass.instancePrototype, {
            ptrType: this.pointeeType,
            ptr: rawPointer,
            smartPtrType: this,
            smartPtr: ptr
          });
        } else {
          return makeClassHandle(this.registeredClass.instancePrototype, {
            ptrType: this,
            ptr
          });
        }
      }
      var actualType = this.registeredClass.getActualType(rawPointer);
      var registeredPointerRecord = registeredPointers[actualType];
      if (!registeredPointerRecord) {
        return makeDefaultHandle.call(this);
      }
      var toType;
      if (this.isConst) {
        toType = registeredPointerRecord.constPointerType;
      } else {
        toType = registeredPointerRecord.pointerType;
      }
      var dp = downcastPointer(rawPointer, this.registeredClass, toType.registeredClass);
      if (dp === null) {
        return makeDefaultHandle.call(this);
      }
      if (this.isSmartPointer) {
        return makeClassHandle(toType.registeredClass.instancePrototype, {
          ptrType: toType,
          ptr: dp,
          smartPtrType: this,
          smartPtr: ptr
        });
      } else {
        return makeClassHandle(toType.registeredClass.instancePrototype, {
          ptrType: toType,
          ptr: dp
        });
      }
    }
    var attachFinalizer = (handle) => {
      if ("undefined" === typeof FinalizationRegistry) {
        attachFinalizer = (handle2) => handle2;
        return handle;
      }
      finalizationRegistry = new FinalizationRegistry((info) => {
        console.warn(info.leakWarning.stack.replace(/^Error: /, ""));
        releaseClassHandle(info.$$);
      });
      attachFinalizer = (handle2) => {
        var $$ = handle2.$$;
        var hasSmartPtr = !!$$.smartPtr;
        if (hasSmartPtr) {
          var info = {
            $$
          };
          var cls = $$.ptrType.registeredClass;
          info.leakWarning = new Error(`Embind found a leaked C++ instance ${cls.name} <${ptrToString($$.ptr)}>.
We'll free it automatically in this case, but this functionality is not reliable across various environments.
Make sure to invoke .delete() manually once you're done with the instance instead.
Originally allocated`);
          if ("captureStackTrace" in Error) {
            Error.captureStackTrace(info.leakWarning, RegisteredPointer_fromWireType);
          }
          finalizationRegistry.register(handle2, info, handle2);
        }
        return handle2;
      };
      detachFinalizer = (handle2) => finalizationRegistry.unregister(handle2);
      return attachFinalizer(handle);
    };
    var init_ClassHandle = () => {
      Object.assign(ClassHandle.prototype, {
        "isAliasOf"(other) {
          if (!(this instanceof ClassHandle)) {
            return false;
          }
          if (!(other instanceof ClassHandle)) {
            return false;
          }
          var leftClass = this.$$.ptrType.registeredClass;
          var left = this.$$.ptr;
          other.$$ = /** @type {Object} */
          other.$$;
          var rightClass = other.$$.ptrType.registeredClass;
          var right = other.$$.ptr;
          while (leftClass.baseClass) {
            left = leftClass.upcast(left);
            leftClass = leftClass.baseClass;
          }
          while (rightClass.baseClass) {
            right = rightClass.upcast(right);
            rightClass = rightClass.baseClass;
          }
          return leftClass === rightClass && left === right;
        },
        "clone"() {
          if (!this.$$.ptr) {
            throwInstanceAlreadyDeleted(this);
          }
          if (this.$$.preservePointerOnDelete) {
            this.$$.count.value += 1;
            return this;
          } else {
            var clone = attachFinalizer(Object.create(Object.getPrototypeOf(this), {
              $$: {
                value: shallowCopyInternalPointer(this.$$)
              }
            }));
            clone.$$.count.value += 1;
            clone.$$.deleteScheduled = false;
            return clone;
          }
        },
        "delete"() {
          if (!this.$$.ptr) {
            throwInstanceAlreadyDeleted(this);
          }
          if (this.$$.deleteScheduled && !this.$$.preservePointerOnDelete) {
            throwBindingError("Object already scheduled for deletion");
          }
          detachFinalizer(this);
          releaseClassHandle(this.$$);
          if (!this.$$.preservePointerOnDelete) {
            this.$$.smartPtr = void 0;
            this.$$.ptr = void 0;
          }
        },
        "isDeleted"() {
          return !this.$$.ptr;
        },
        "deleteLater"() {
          if (!this.$$.ptr) {
            throwInstanceAlreadyDeleted(this);
          }
          if (this.$$.deleteScheduled && !this.$$.preservePointerOnDelete) {
            throwBindingError("Object already scheduled for deletion");
          }
          deletionQueue.push(this);
          if (deletionQueue.length === 1 && delayFunction) {
            delayFunction(flushPendingDeletes);
          }
          this.$$.deleteScheduled = true;
          return this;
        }
      });
    };
    function ClassHandle() {
    }
    var createNamedFunction = (name, body) => Object.defineProperty(body, "name", {
      value: name
    });
    var ensureOverloadTable = (proto, methodName, humanName) => {
      if (void 0 === proto[methodName].overloadTable) {
        var prevFunc = proto[methodName];
        proto[methodName] = function(...args) {
          if (!proto[methodName].overloadTable.hasOwnProperty(args.length)) {
            throwBindingError(`Function '${humanName}' called with an invalid number of arguments (${args.length}) - expects one of (${proto[methodName].overloadTable})!`);
          }
          return proto[methodName].overloadTable[args.length].apply(this, args);
        };
        proto[methodName].overloadTable = [];
        proto[methodName].overloadTable[prevFunc.argCount] = prevFunc;
      }
    };
    var exposePublicSymbol = (name, value, numArguments) => {
      if (Module.hasOwnProperty(name)) {
        {
          throwBindingError(`Cannot register public name '${name}' twice`);
        }
        ensureOverloadTable(Module, name, name);
        if (Module.hasOwnProperty(numArguments)) {
          throwBindingError(`Cannot register multiple overloads of a function with the same number of arguments (${numArguments})!`);
        }
        Module[name].overloadTable[numArguments] = value;
      } else {
        Module[name] = value;
      }
    };
    var char_0 = 48;
    var char_9 = 57;
    var makeLegalFunctionName = (name) => {
      if (void 0 === name) {
        return "_unknown";
      }
      name = name.replace(/[^a-zA-Z0-9_]/g, "$");
      var f = name.charCodeAt(0);
      if (f >= char_0 && f <= char_9) {
        return `_${name}`;
      }
      return name;
    };
    function RegisteredClass(name, constructor, instancePrototype, rawDestructor, baseClass, getActualType, upcast, downcast) {
      this.name = name;
      this.constructor = constructor;
      this.instancePrototype = instancePrototype;
      this.rawDestructor = rawDestructor;
      this.baseClass = baseClass;
      this.getActualType = getActualType;
      this.upcast = upcast;
      this.downcast = downcast;
      this.pureVirtualFunctions = [];
    }
    var upcastPointer = (ptr, ptrClass, desiredClass) => {
      while (ptrClass !== desiredClass) {
        if (!ptrClass.upcast) {
          throwBindingError(`Expected null or instance of ${desiredClass.name}, got an instance of ${ptrClass.name}`);
        }
        ptr = ptrClass.upcast(ptr);
        ptrClass = ptrClass.baseClass;
      }
      return ptr;
    };
    function constNoSmartPtrRawPointerToWireType(destructors, handle) {
      if (handle === null) {
        if (this.isReference) {
          throwBindingError(`null is not a valid ${this.name}`);
        }
        return 0;
      }
      if (!handle.$$) {
        throwBindingError(`Cannot pass "${embindRepr(handle)}" as a ${this.name}`);
      }
      if (!handle.$$.ptr) {
        throwBindingError(`Cannot pass deleted object as a pointer of type ${this.name}`);
      }
      var handleClass = handle.$$.ptrType.registeredClass;
      var ptr = upcastPointer(handle.$$.ptr, handleClass, this.registeredClass);
      return ptr;
    }
    function genericPointerToWireType(destructors, handle) {
      var ptr;
      if (handle === null) {
        if (this.isReference) {
          throwBindingError(`null is not a valid ${this.name}`);
        }
        if (this.isSmartPointer) {
          ptr = this.rawConstructor();
          if (destructors !== null) {
            destructors.push(this.rawDestructor, ptr);
          }
          return ptr;
        } else {
          return 0;
        }
      }
      if (!handle || !handle.$$) {
        throwBindingError(`Cannot pass "${embindRepr(handle)}" as a ${this.name}`);
      }
      if (!handle.$$.ptr) {
        throwBindingError(`Cannot pass deleted object as a pointer of type ${this.name}`);
      }
      if (!this.isConst && handle.$$.ptrType.isConst) {
        throwBindingError(`Cannot convert argument of type ${handle.$$.smartPtrType ? handle.$$.smartPtrType.name : handle.$$.ptrType.name} to parameter type ${this.name}`);
      }
      var handleClass = handle.$$.ptrType.registeredClass;
      ptr = upcastPointer(handle.$$.ptr, handleClass, this.registeredClass);
      if (this.isSmartPointer) {
        if (void 0 === handle.$$.smartPtr) {
          throwBindingError("Passing raw pointer to smart pointer is illegal");
        }
        switch (this.sharingPolicy) {
          case 0:
            if (handle.$$.smartPtrType === this) {
              ptr = handle.$$.smartPtr;
            } else {
              throwBindingError(`Cannot convert argument of type ${handle.$$.smartPtrType ? handle.$$.smartPtrType.name : handle.$$.ptrType.name} to parameter type ${this.name}`);
            }
            break;
          case 1:
            ptr = handle.$$.smartPtr;
            break;
          case 2:
            if (handle.$$.smartPtrType === this) {
              ptr = handle.$$.smartPtr;
            } else {
              var clonedHandle = handle["clone"]();
              ptr = this.rawShare(ptr, Emval.toHandle(() => clonedHandle["delete"]()));
              if (destructors !== null) {
                destructors.push(this.rawDestructor, ptr);
              }
            }
            break;
          default:
            throwBindingError("Unsupporting sharing policy");
        }
      }
      return ptr;
    }
    function nonConstNoSmartPtrRawPointerToWireType(destructors, handle) {
      if (handle === null) {
        if (this.isReference) {
          throwBindingError(`null is not a valid ${this.name}`);
        }
        return 0;
      }
      if (!handle.$$) {
        throwBindingError(`Cannot pass "${embindRepr(handle)}" as a ${this.name}`);
      }
      if (!handle.$$.ptr) {
        throwBindingError(`Cannot pass deleted object as a pointer of type ${this.name}`);
      }
      if (handle.$$.ptrType.isConst) {
        throwBindingError(`Cannot convert argument of type ${handle.$$.ptrType.name} to parameter type ${this.name}`);
      }
      var handleClass = handle.$$.ptrType.registeredClass;
      var ptr = upcastPointer(handle.$$.ptr, handleClass, this.registeredClass);
      return ptr;
    }
    var init_RegisteredPointer = () => {
      Object.assign(RegisteredPointer.prototype, {
        getPointee(ptr) {
          if (this.rawGetPointee) {
            ptr = this.rawGetPointee(ptr);
          }
          return ptr;
        },
        destructor(ptr) {
          this.rawDestructor?.(ptr);
        },
        "argPackAdvance": GenericWireTypeSize,
        "readValueFromPointer": readPointer,
        "fromWireType": RegisteredPointer_fromWireType
      });
    };
    function RegisteredPointer(name, registeredClass, isReference, isConst, isSmartPointer, pointeeType, sharingPolicy, rawGetPointee, rawConstructor, rawShare, rawDestructor) {
      this.name = name;
      this.registeredClass = registeredClass;
      this.isReference = isReference;
      this.isConst = isConst;
      this.isSmartPointer = isSmartPointer;
      this.pointeeType = pointeeType;
      this.sharingPolicy = sharingPolicy;
      this.rawGetPointee = rawGetPointee;
      this.rawConstructor = rawConstructor;
      this.rawShare = rawShare;
      this.rawDestructor = rawDestructor;
      if (!isSmartPointer && registeredClass.baseClass === void 0) {
        if (isConst) {
          this["toWireType"] = constNoSmartPtrRawPointerToWireType;
          this.destructorFunction = null;
        } else {
          this["toWireType"] = nonConstNoSmartPtrRawPointerToWireType;
          this.destructorFunction = null;
        }
      } else {
        this["toWireType"] = genericPointerToWireType;
      }
    }
    var replacePublicSymbol = (name, value, numArguments) => {
      if (!Module.hasOwnProperty(name)) {
        throwInternalError("Replacing nonexistent public symbol");
      }
      if (void 0 !== Module[name].overloadTable && void 0 !== numArguments) {
        Module[name].overloadTable[numArguments] = value;
      } else {
        Module[name] = value;
        Module[name].argCount = numArguments;
      }
    };
    var dynCallLegacy = (sig, ptr, args) => {
      sig = sig.replace(/p/g, "i");
      assert("dynCall_" + sig in Module, `bad function pointer type - dynCall function not found for sig '${sig}'`);
      if (args?.length) {
        assert(args.length === sig.substring(1).replace(/j/g, "--").length);
      } else {
        assert(sig.length == 1);
      }
      var f = Module["dynCall_" + sig];
      return f(ptr, ...args);
    };
    var wasmTableMirror = [];
    var wasmTable;
    var getWasmTableEntry = (funcPtr) => {
      var func = wasmTableMirror[funcPtr];
      if (!func) {
        if (funcPtr >= wasmTableMirror.length) wasmTableMirror.length = funcPtr + 1;
        wasmTableMirror[funcPtr] = func = wasmTable.get(funcPtr);
      }
      assert(wasmTable.get(funcPtr) == func, "JavaScript-side Wasm function table mirror is out of date!");
      return func;
    };
    var dynCall = (sig, ptr, args = []) => {
      if (sig.includes("j")) {
        return dynCallLegacy(sig, ptr, args);
      }
      assert(getWasmTableEntry(ptr), `missing table entry in dynCall: ${ptr}`);
      var rtn = getWasmTableEntry(ptr)(...args);
      return rtn;
    };
    var getDynCaller = (sig, ptr) => {
      assert(sig.includes("j") || sig.includes("p"), "getDynCaller should only be called with i64 sigs");
      return (...args) => dynCall(sig, ptr, args);
    };
    var embind__requireFunction = (signature, rawFunction) => {
      signature = readLatin1String(signature);
      function makeDynCaller() {
        if (signature.includes("j")) {
          return getDynCaller(signature, rawFunction);
        }
        return getWasmTableEntry(rawFunction);
      }
      var fp = makeDynCaller();
      if (typeof fp != "function") {
        throwBindingError(`unknown function pointer with signature ${signature}: ${rawFunction}`);
      }
      return fp;
    };
    var extendError = (baseErrorType, errorName) => {
      var errorClass = createNamedFunction(errorName, function(message) {
        this.name = errorName;
        this.message = message;
        var stack = new Error(message).stack;
        if (stack !== void 0) {
          this.stack = this.toString() + "\n" + stack.replace(/^Error(:[^\n]*)?\n/, "");
        }
      });
      errorClass.prototype = Object.create(baseErrorType.prototype);
      errorClass.prototype.constructor = errorClass;
      errorClass.prototype.toString = function() {
        if (this.message === void 0) {
          return this.name;
        } else {
          return `${this.name}: ${this.message}`;
        }
      };
      return errorClass;
    };
    var UnboundTypeError;
    var getTypeName = (type) => {
      var ptr = ___getTypeName(type);
      var rv = readLatin1String(ptr);
      _free(ptr);
      return rv;
    };
    var throwUnboundTypeError = (message, types) => {
      var unboundTypes = [];
      var seen = {};
      function visit(type) {
        if (seen[type]) {
          return;
        }
        if (registeredTypes[type]) {
          return;
        }
        if (typeDependencies[type]) {
          typeDependencies[type].forEach(visit);
          return;
        }
        unboundTypes.push(type);
        seen[type] = true;
      }
      types.forEach(visit);
      throw new UnboundTypeError(`${message}: ` + unboundTypes.map(getTypeName).join([", "]));
    };
    var __embind_register_class = (rawType, rawPointerType, rawConstPointerType, baseClassRawType, getActualTypeSignature, getActualType, upcastSignature, upcast, downcastSignature, downcast, name, destructorSignature, rawDestructor) => {
      name = readLatin1String(name);
      getActualType = embind__requireFunction(getActualTypeSignature, getActualType);
      upcast && (upcast = embind__requireFunction(upcastSignature, upcast));
      downcast && (downcast = embind__requireFunction(downcastSignature, downcast));
      rawDestructor = embind__requireFunction(destructorSignature, rawDestructor);
      var legalFunctionName = makeLegalFunctionName(name);
      exposePublicSymbol(legalFunctionName, function() {
        throwUnboundTypeError(`Cannot construct ${name} due to unbound types`, [baseClassRawType]);
      });
      whenDependentTypesAreResolved([rawType, rawPointerType, rawConstPointerType], baseClassRawType ? [baseClassRawType] : [], (base) => {
        var _a;
        base = base[0];
        var baseClass;
        var basePrototype;
        if (baseClassRawType) {
          baseClass = base.registeredClass;
          basePrototype = baseClass.instancePrototype;
        } else {
          basePrototype = ClassHandle.prototype;
        }
        var constructor = createNamedFunction(name, function(...args) {
          if (Object.getPrototypeOf(this) !== instancePrototype) {
            throw new BindingError("Use 'new' to construct " + name);
          }
          if (void 0 === registeredClass.constructor_body) {
            throw new BindingError(name + " has no accessible constructor");
          }
          var body = registeredClass.constructor_body[args.length];
          if (void 0 === body) {
            throw new BindingError(`Tried to invoke ctor of ${name} with invalid number of parameters (${args.length}) - expected (${Object.keys(registeredClass.constructor_body).toString()}) parameters instead!`);
          }
          return body.apply(this, args);
        });
        var instancePrototype = Object.create(basePrototype, {
          constructor: {
            value: constructor
          }
        });
        constructor.prototype = instancePrototype;
        var registeredClass = new RegisteredClass(name, constructor, instancePrototype, rawDestructor, baseClass, getActualType, upcast, downcast);
        if (registeredClass.baseClass) {
          (_a = registeredClass.baseClass).__derivedClasses ?? (_a.__derivedClasses = []);
          registeredClass.baseClass.__derivedClasses.push(registeredClass);
        }
        var referenceConverter = new RegisteredPointer(name, registeredClass, true, false, false);
        var pointerConverter = new RegisteredPointer(name + "*", registeredClass, false, false, false);
        var constPointerConverter = new RegisteredPointer(name + " const*", registeredClass, false, true, false);
        registeredPointers[rawType] = {
          pointerType: pointerConverter,
          constPointerType: constPointerConverter
        };
        replacePublicSymbol(legalFunctionName, constructor);
        return [referenceConverter, pointerConverter, constPointerConverter];
      });
    };
    function usesDestructorStack(argTypes) {
      for (var i = 1; i < argTypes.length; ++i) {
        if (argTypes[i] !== null && argTypes[i].destructorFunction === void 0) {
          return true;
        }
      }
      return false;
    }
    function newFunc(constructor, argumentList) {
      if (!(constructor instanceof Function)) {
        throw new TypeError(`new_ called with constructor type ${typeof constructor} which is not a function`);
      }
      var dummy = createNamedFunction(constructor.name || "unknownFunctionName", function() {
      });
      dummy.prototype = constructor.prototype;
      var obj = new dummy();
      var r = constructor.apply(obj, argumentList);
      return r instanceof Object ? r : obj;
    }
    function createJsInvoker(argTypes, isClassMethodFunc, returns, isAsync) {
      var needsDestructorStack = usesDestructorStack(argTypes);
      var argCount = argTypes.length;
      var argsList = "";
      var argsListWired = "";
      for (var i = 0; i < argCount - 2; ++i) {
        argsList += (i !== 0 ? ", " : "") + "arg" + i;
        argsListWired += (i !== 0 ? ", " : "") + "arg" + i + "Wired";
      }
      var invokerFnBody = `
        return function (${argsList}) {
        if (arguments.length !== ${argCount - 2}) {
          throwBindingError('function ' + humanName + ' called with ' + arguments.length + ' arguments, expected ${argCount - 2}');
        }`;
      if (needsDestructorStack) {
        invokerFnBody += "var destructors = [];\n";
      }
      var dtorStack = needsDestructorStack ? "destructors" : "null";
      var args1 = ["humanName", "throwBindingError", "invoker", "fn", "runDestructors", "retType", "classParam"];
      if (isClassMethodFunc) {
        invokerFnBody += "var thisWired = classParam['toWireType'](" + dtorStack + ", this);\n";
      }
      for (var i = 0; i < argCount - 2; ++i) {
        invokerFnBody += "var arg" + i + "Wired = argType" + i + "['toWireType'](" + dtorStack + ", arg" + i + ");\n";
        args1.push("argType" + i);
      }
      if (isClassMethodFunc) {
        argsListWired = "thisWired" + (argsListWired.length > 0 ? ", " : "") + argsListWired;
      }
      invokerFnBody += (returns || isAsync ? "var rv = " : "") + "invoker(fn" + (argsListWired.length > 0 ? ", " : "") + argsListWired + ");\n";
      if (needsDestructorStack) {
        invokerFnBody += "runDestructors(destructors);\n";
      } else {
        for (var i = isClassMethodFunc ? 1 : 2; i < argTypes.length; ++i) {
          var paramName = i === 1 ? "thisWired" : "arg" + (i - 2) + "Wired";
          if (argTypes[i].destructorFunction !== null) {
            invokerFnBody += `${paramName}_dtor(${paramName});
`;
            args1.push(`${paramName}_dtor`);
          }
        }
      }
      if (returns) {
        invokerFnBody += "var ret = retType['fromWireType'](rv);\nreturn ret;\n";
      }
      invokerFnBody += "}\n";
      invokerFnBody = `if (arguments.length !== ${args1.length}){ throw new Error(humanName + "Expected ${args1.length} closure arguments " + arguments.length + " given."); }
${invokerFnBody}`;
      return [args1, invokerFnBody];
    }
    function craftInvokerFunction(humanName, argTypes, classType, cppInvokerFunc, cppTargetFunc, isAsync) {
      var argCount = argTypes.length;
      if (argCount < 2) {
        throwBindingError("argTypes array size mismatch! Must at least get return value and 'this' types!");
      }
      assert(!isAsync, "Async bindings are only supported with JSPI.");
      var isClassMethodFunc = argTypes[1] !== null && classType !== null;
      var needsDestructorStack = usesDestructorStack(argTypes);
      var returns = argTypes[0].name !== "void";
      var closureArgs = [humanName, throwBindingError, cppInvokerFunc, cppTargetFunc, runDestructors, argTypes[0], argTypes[1]];
      for (var i = 0; i < argCount - 2; ++i) {
        closureArgs.push(argTypes[i + 2]);
      }
      if (!needsDestructorStack) {
        for (var i = isClassMethodFunc ? 1 : 2; i < argTypes.length; ++i) {
          if (argTypes[i].destructorFunction !== null) {
            closureArgs.push(argTypes[i].destructorFunction);
          }
        }
      }
      let [args, invokerFnBody] = createJsInvoker(argTypes, isClassMethodFunc, returns, isAsync);
      args.push(invokerFnBody);
      var invokerFn = newFunc(Function, args)(...closureArgs);
      return createNamedFunction(humanName, invokerFn);
    }
    var heap32VectorToArray = (count, firstElement) => {
      var array = [];
      for (var i = 0; i < count; i++) {
        array.push(SAFE_HEAP_LOAD((firstElement + i * 4 >> 2) * 4, 4, 1));
      }
      return array;
    };
    var getFunctionName = (signature) => {
      signature = signature.trim();
      const argsIndex = signature.indexOf("(");
      if (argsIndex !== -1) {
        assert(signature[signature.length - 1] == ")", "Parentheses for argument names should match.");
        return signature.substr(0, argsIndex);
      } else {
        return signature;
      }
    };
    var __embind_register_class_class_function = (rawClassType, methodName, argCount, rawArgTypesAddr, invokerSignature, rawInvoker, fn, isAsync) => {
      var rawArgTypes = heap32VectorToArray(argCount, rawArgTypesAddr);
      methodName = readLatin1String(methodName);
      methodName = getFunctionName(methodName);
      rawInvoker = embind__requireFunction(invokerSignature, rawInvoker);
      whenDependentTypesAreResolved([], [rawClassType], (classType) => {
        classType = classType[0];
        var humanName = `${classType.name}.${methodName}`;
        function unboundTypesHandler() {
          throwUnboundTypeError(`Cannot call ${humanName} due to unbound types`, rawArgTypes);
        }
        if (methodName.startsWith("@@")) {
          methodName = Symbol[methodName.substring(2)];
        }
        var proto = classType.registeredClass.constructor;
        if (void 0 === proto[methodName]) {
          unboundTypesHandler.argCount = argCount - 1;
          proto[methodName] = unboundTypesHandler;
        } else {
          ensureOverloadTable(proto, methodName, humanName);
          proto[methodName].overloadTable[argCount - 1] = unboundTypesHandler;
        }
        whenDependentTypesAreResolved([], rawArgTypes, (argTypes) => {
          var invokerArgsArray = [
            argTypes[0],
            /* return value */
            null
          ].concat(
            /* no class 'this'*/
            argTypes.slice(1)
          );
          var func = craftInvokerFunction(
            humanName,
            invokerArgsArray,
            null,
            /* no class 'this'*/
            rawInvoker,
            fn,
            isAsync
          );
          if (void 0 === proto[methodName].overloadTable) {
            func.argCount = argCount - 1;
            proto[methodName] = func;
          } else {
            proto[methodName].overloadTable[argCount - 1] = func;
          }
          if (classType.registeredClass.__derivedClasses) {
            for (const derivedClass of classType.registeredClass.__derivedClasses) {
              if (!derivedClass.constructor.hasOwnProperty(methodName)) {
                derivedClass.constructor[methodName] = func;
              }
            }
          }
          return [];
        });
        return [];
      });
    };
    var __embind_register_class_constructor = (rawClassType, argCount, rawArgTypesAddr, invokerSignature, invoker, rawConstructor) => {
      assert(argCount > 0);
      var rawArgTypes = heap32VectorToArray(argCount, rawArgTypesAddr);
      invoker = embind__requireFunction(invokerSignature, invoker);
      whenDependentTypesAreResolved([], [rawClassType], (classType) => {
        classType = classType[0];
        var humanName = `constructor ${classType.name}`;
        if (void 0 === classType.registeredClass.constructor_body) {
          classType.registeredClass.constructor_body = [];
        }
        if (void 0 !== classType.registeredClass.constructor_body[argCount - 1]) {
          throw new BindingError(`Cannot register multiple constructors with identical number of parameters (${argCount - 1}) for class '${classType.name}'! Overload resolution is currently only performed using the parameter count, not actual type info!`);
        }
        classType.registeredClass.constructor_body[argCount - 1] = () => {
          throwUnboundTypeError(`Cannot construct ${classType.name} due to unbound types`, rawArgTypes);
        };
        whenDependentTypesAreResolved([], rawArgTypes, (argTypes) => {
          argTypes.splice(1, 0, null);
          classType.registeredClass.constructor_body[argCount - 1] = craftInvokerFunction(humanName, argTypes, null, invoker, rawConstructor);
          return [];
        });
        return [];
      });
    };
    var __embind_register_class_function = (rawClassType, methodName, argCount, rawArgTypesAddr, invokerSignature, rawInvoker, context, isPureVirtual, isAsync) => {
      var rawArgTypes = heap32VectorToArray(argCount, rawArgTypesAddr);
      methodName = readLatin1String(methodName);
      methodName = getFunctionName(methodName);
      rawInvoker = embind__requireFunction(invokerSignature, rawInvoker);
      whenDependentTypesAreResolved([], [rawClassType], (classType) => {
        classType = classType[0];
        var humanName = `${classType.name}.${methodName}`;
        if (methodName.startsWith("@@")) {
          methodName = Symbol[methodName.substring(2)];
        }
        if (isPureVirtual) {
          classType.registeredClass.pureVirtualFunctions.push(methodName);
        }
        function unboundTypesHandler() {
          throwUnboundTypeError(`Cannot call ${humanName} due to unbound types`, rawArgTypes);
        }
        var proto = classType.registeredClass.instancePrototype;
        var method = proto[methodName];
        if (void 0 === method || void 0 === method.overloadTable && method.className !== classType.name && method.argCount === argCount - 2) {
          unboundTypesHandler.argCount = argCount - 2;
          unboundTypesHandler.className = classType.name;
          proto[methodName] = unboundTypesHandler;
        } else {
          ensureOverloadTable(proto, methodName, humanName);
          proto[methodName].overloadTable[argCount - 2] = unboundTypesHandler;
        }
        whenDependentTypesAreResolved([], rawArgTypes, (argTypes) => {
          var memberFunction = craftInvokerFunction(humanName, argTypes, classType, rawInvoker, context, isAsync);
          if (void 0 === proto[methodName].overloadTable) {
            memberFunction.argCount = argCount - 2;
            proto[methodName] = memberFunction;
          } else {
            proto[methodName].overloadTable[argCount - 2] = memberFunction;
          }
          return [];
        });
        return [];
      });
    };
    var validateThis = (this_, classType, humanName) => {
      if (!(this_ instanceof Object)) {
        throwBindingError(`${humanName} with invalid "this": ${this_}`);
      }
      if (!(this_ instanceof classType.registeredClass.constructor)) {
        throwBindingError(`${humanName} incompatible with "this" of type ${this_.constructor.name}`);
      }
      if (!this_.$$.ptr) {
        throwBindingError(`cannot call emscripten binding method ${humanName} on deleted object`);
      }
      return upcastPointer(this_.$$.ptr, this_.$$.ptrType.registeredClass, classType.registeredClass);
    };
    var __embind_register_class_property = (classType, fieldName, getterReturnType, getterSignature, getter, getterContext, setterArgumentType, setterSignature, setter, setterContext) => {
      fieldName = readLatin1String(fieldName);
      getter = embind__requireFunction(getterSignature, getter);
      whenDependentTypesAreResolved([], [classType], (classType2) => {
        classType2 = classType2[0];
        var humanName = `${classType2.name}.${fieldName}`;
        var desc = {
          get() {
            throwUnboundTypeError(`Cannot access ${humanName} due to unbound types`, [getterReturnType, setterArgumentType]);
          },
          enumerable: true,
          configurable: true
        };
        if (setter) {
          desc.set = () => throwUnboundTypeError(`Cannot access ${humanName} due to unbound types`, [getterReturnType, setterArgumentType]);
        } else {
          desc.set = (v) => throwBindingError(humanName + " is a read-only property");
        }
        Object.defineProperty(classType2.registeredClass.instancePrototype, fieldName, desc);
        whenDependentTypesAreResolved([], setter ? [getterReturnType, setterArgumentType] : [getterReturnType], (types) => {
          var getterReturnType2 = types[0];
          var desc2 = {
            get() {
              var ptr = validateThis(this, classType2, humanName + " getter");
              return getterReturnType2["fromWireType"](getter(getterContext, ptr));
            },
            enumerable: true
          };
          if (setter) {
            setter = embind__requireFunction(setterSignature, setter);
            var setterArgumentType2 = types[1];
            desc2.set = function(v) {
              var ptr = validateThis(this, classType2, humanName + " setter");
              var destructors = [];
              setter(setterContext, ptr, setterArgumentType2["toWireType"](destructors, v));
              runDestructors(destructors);
            };
          }
          Object.defineProperty(classType2.registeredClass.instancePrototype, fieldName, desc2);
          return [];
        });
        return [];
      });
    };
    var emval_freelist = [];
    var emval_handles = [];
    var __emval_decref = (handle) => {
      if (handle > 9 && 0 === --emval_handles[handle + 1]) {
        assert(emval_handles[handle] !== void 0, `Decref for unallocated handle.`);
        emval_handles[handle] = void 0;
        emval_freelist.push(handle);
      }
    };
    var count_emval_handles = () => emval_handles.length / 2 - 5 - emval_freelist.length;
    var init_emval = () => {
      emval_handles.push(0, 1, void 0, 1, null, 1, true, 1, false, 1);
      assert(emval_handles.length === 5 * 2);
      Module["count_emval_handles"] = count_emval_handles;
    };
    var Emval = {
      toValue: (handle) => {
        if (!handle) {
          throwBindingError("Cannot use deleted val. handle = " + handle);
        }
        assert(handle === 2 || emval_handles[handle] !== void 0 && handle % 2 === 0, `invalid handle: ${handle}`);
        return emval_handles[handle];
      },
      toHandle: (value) => {
        switch (value) {
          case void 0:
            return 2;
          case null:
            return 4;
          case true:
            return 6;
          case false:
            return 8;
          default: {
            const handle = emval_freelist.pop() || emval_handles.length;
            emval_handles[handle] = value;
            emval_handles[handle + 1] = 1;
            return handle;
          }
        }
      }
    };
    var EmValType = {
      name: "emscripten::val",
      "fromWireType": (handle) => {
        var rv = Emval.toValue(handle);
        __emval_decref(handle);
        return rv;
      },
      "toWireType": (destructors, value) => Emval.toHandle(value),
      "argPackAdvance": GenericWireTypeSize,
      "readValueFromPointer": readPointer,
      destructorFunction: null
    };
    var __embind_register_emval = (rawType) => registerType(rawType, EmValType);
    var enumReadValueFromPointer = (name, width, signed) => {
      switch (width) {
        case 1:
          return signed ? function(pointer) {
            return this["fromWireType"](SAFE_HEAP_LOAD(pointer, 1, 0));
          } : function(pointer) {
            return this["fromWireType"](SAFE_HEAP_LOAD(pointer, 1, 1));
          };
        case 2:
          return signed ? function(pointer) {
            return this["fromWireType"](SAFE_HEAP_LOAD((pointer >> 1) * 2, 2, 0));
          } : function(pointer) {
            return this["fromWireType"](SAFE_HEAP_LOAD((pointer >> 1) * 2, 2, 1));
          };
        case 4:
          return signed ? function(pointer) {
            return this["fromWireType"](SAFE_HEAP_LOAD((pointer >> 2) * 4, 4, 0));
          } : function(pointer) {
            return this["fromWireType"](SAFE_HEAP_LOAD((pointer >> 2) * 4, 4, 1));
          };
        default:
          throw new TypeError(`invalid integer width (${width}): ${name}`);
      }
    };
    var __embind_register_enum = (rawType, name, size, isSigned) => {
      name = readLatin1String(name);
      function ctor() {
      }
      ctor.values = {};
      registerType(rawType, {
        name,
        constructor: ctor,
        "fromWireType": function(c) {
          return this.constructor.values[c];
        },
        "toWireType": (destructors, c) => c.value,
        "argPackAdvance": GenericWireTypeSize,
        "readValueFromPointer": enumReadValueFromPointer(name, size, isSigned),
        destructorFunction: null
      });
      exposePublicSymbol(name, ctor);
    };
    var requireRegisteredType = (rawType, humanName) => {
      var impl = registeredTypes[rawType];
      if (void 0 === impl) {
        throwBindingError(`${humanName} has unknown type ${getTypeName(rawType)}`);
      }
      return impl;
    };
    var __embind_register_enum_value = (rawEnumType, name, enumValue) => {
      var enumType = requireRegisteredType(rawEnumType, "enum");
      name = readLatin1String(name);
      var Enum = enumType.constructor;
      var Value = Object.create(enumType.constructor.prototype, {
        value: {
          value: enumValue
        },
        constructor: {
          value: createNamedFunction(`${enumType.name}_${name}`, function() {
          })
        }
      });
      Enum.values[enumValue] = Value;
      Enum[name] = Value;
    };
    var embindRepr = (v) => {
      if (v === null) {
        return "null";
      }
      var t = typeof v;
      if (t === "object" || t === "array" || t === "function") {
        return v.toString();
      } else {
        return "" + v;
      }
    };
    var floatReadValueFromPointer = (name, width) => {
      switch (width) {
        case 4:
          return function(pointer) {
            return this["fromWireType"](SAFE_HEAP_LOAD_D((pointer >> 2) * 4, 4, 0));
          };
        case 8:
          return function(pointer) {
            return this["fromWireType"](SAFE_HEAP_LOAD_D((pointer >> 3) * 8, 8, 0));
          };
        default:
          throw new TypeError(`invalid float width (${width}): ${name}`);
      }
    };
    var __embind_register_float = (rawType, name, size) => {
      name = readLatin1String(name);
      registerType(rawType, {
        name,
        "fromWireType": (value) => value,
        "toWireType": (destructors, value) => {
          if (typeof value != "number" && typeof value != "boolean") {
            throw new TypeError(`Cannot convert ${embindRepr(value)} to ${this.name}`);
          }
          return value;
        },
        "argPackAdvance": GenericWireTypeSize,
        "readValueFromPointer": floatReadValueFromPointer(name, size),
        destructorFunction: null
      });
    };
    var integerReadValueFromPointer = (name, width, signed) => {
      switch (width) {
        case 1:
          return signed ? (pointer) => SAFE_HEAP_LOAD(pointer, 1, 0) : (pointer) => SAFE_HEAP_LOAD(pointer, 1, 1);
        case 2:
          return signed ? (pointer) => SAFE_HEAP_LOAD((pointer >> 1) * 2, 2, 0) : (pointer) => SAFE_HEAP_LOAD((pointer >> 1) * 2, 2, 1);
        case 4:
          return signed ? (pointer) => SAFE_HEAP_LOAD((pointer >> 2) * 4, 4, 0) : (pointer) => SAFE_HEAP_LOAD((pointer >> 2) * 4, 4, 1);
        default:
          throw new TypeError(`invalid integer width (${width}): ${name}`);
      }
    };
    var __embind_register_integer = (primitiveType, name, size, minRange, maxRange) => {
      name = readLatin1String(name);
      if (maxRange === -1) {
        maxRange = 4294967295;
      }
      var fromWireType = (value) => value;
      if (minRange === 0) {
        var bitshift = 32 - 8 * size;
        fromWireType = (value) => value << bitshift >>> bitshift;
      }
      var isUnsignedType = name.includes("unsigned");
      var checkAssertions = (value, toTypeName) => {
        if (typeof value != "number" && typeof value != "boolean") {
          throw new TypeError(`Cannot convert "${embindRepr(value)}" to ${toTypeName}`);
        }
        if (value < minRange || value > maxRange) {
          throw new TypeError(`Passing a number "${embindRepr(value)}" from JS side to C/C++ side to an argument of type "${name}", which is outside the valid range [${minRange}, ${maxRange}]!`);
        }
      };
      var toWireType;
      if (isUnsignedType) {
        toWireType = function(destructors, value) {
          checkAssertions(value, this.name);
          return value >>> 0;
        };
      } else {
        toWireType = function(destructors, value) {
          checkAssertions(value, this.name);
          return value;
        };
      }
      registerType(primitiveType, {
        name,
        "fromWireType": fromWireType,
        "toWireType": toWireType,
        "argPackAdvance": GenericWireTypeSize,
        "readValueFromPointer": integerReadValueFromPointer(name, size, minRange !== 0),
        destructorFunction: null
      });
    };
    var __embind_register_memory_view = (rawType, dataTypeIndex, name) => {
      var typeMapping = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];
      var TA = typeMapping[dataTypeIndex];
      function decodeMemoryView(handle) {
        var size = SAFE_HEAP_LOAD((handle >> 2) * 4, 4, 1);
        var data = SAFE_HEAP_LOAD((handle + 4 >> 2) * 4, 4, 1);
        return new TA(HEAP8.buffer, data, size);
      }
      name = readLatin1String(name);
      registerType(rawType, {
        name,
        "fromWireType": decodeMemoryView,
        "argPackAdvance": GenericWireTypeSize,
        "readValueFromPointer": decodeMemoryView
      }, {
        ignoreDuplicateRegistrations: true
      });
    };
    var __embind_register_optional = (rawOptionalType, rawType) => {
      __embind_register_emval(rawOptionalType);
    };
    var __embind_register_smart_ptr = (rawType, rawPointeeType, name, sharingPolicy, getPointeeSignature, rawGetPointee, constructorSignature, rawConstructor, shareSignature, rawShare, destructorSignature, rawDestructor) => {
      name = readLatin1String(name);
      rawGetPointee = embind__requireFunction(getPointeeSignature, rawGetPointee);
      rawConstructor = embind__requireFunction(constructorSignature, rawConstructor);
      rawShare = embind__requireFunction(shareSignature, rawShare);
      rawDestructor = embind__requireFunction(destructorSignature, rawDestructor);
      whenDependentTypesAreResolved([rawType], [rawPointeeType], (pointeeType) => {
        pointeeType = pointeeType[0];
        var registeredPointer = new RegisteredPointer(name, pointeeType.registeredClass, false, false, true, pointeeType, sharingPolicy, rawGetPointee, rawConstructor, rawShare, rawDestructor);
        return [registeredPointer];
      });
    };
    var stringToUTF8 = (str, outPtr, maxBytesToWrite) => {
      assert(typeof maxBytesToWrite == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!");
      return stringToUTF8Array(str, HEAPU8, outPtr, maxBytesToWrite);
    };
    var __embind_register_std_string = (rawType, name) => {
      name = readLatin1String(name);
      var stdStringIsUTF8 = name === "std::string";
      registerType(rawType, {
        name,
        "fromWireType"(value) {
          var length = SAFE_HEAP_LOAD((value >> 2) * 4, 4, 1);
          var payload = value + 4;
          var str;
          if (stdStringIsUTF8) {
            var decodeStartPtr = payload;
            for (var i = 0; i <= length; ++i) {
              var currentBytePtr = payload + i;
              if (i == length || SAFE_HEAP_LOAD(currentBytePtr, 1, 1) == 0) {
                var maxRead = currentBytePtr - decodeStartPtr;
                var stringSegment = UTF8ToString(decodeStartPtr, maxRead);
                if (str === void 0) {
                  str = stringSegment;
                } else {
                  str += String.fromCharCode(0);
                  str += stringSegment;
                }
                decodeStartPtr = currentBytePtr + 1;
              }
            }
          } else {
            var a = new Array(length);
            for (var i = 0; i < length; ++i) {
              a[i] = String.fromCharCode(SAFE_HEAP_LOAD(payload + i, 1, 1));
            }
            str = a.join("");
          }
          _free(value);
          return str;
        },
        "toWireType"(destructors, value) {
          if (value instanceof ArrayBuffer) {
            value = new Uint8Array(value);
          }
          var length;
          var valueIsOfTypeString = typeof value == "string";
          if (!(valueIsOfTypeString || value instanceof Uint8Array || value instanceof Uint8ClampedArray || value instanceof Int8Array)) {
            throwBindingError("Cannot pass non-string to std::string");
          }
          if (stdStringIsUTF8 && valueIsOfTypeString) {
            length = lengthBytesUTF8(value);
          } else {
            length = value.length;
          }
          var base = _malloc(4 + length + 1);
          var ptr = base + 4;
          SAFE_HEAP_STORE((base >> 2) * 4, length, 4);
          if (stdStringIsUTF8 && valueIsOfTypeString) {
            stringToUTF8(value, ptr, length + 1);
          } else {
            if (valueIsOfTypeString) {
              for (var i = 0; i < length; ++i) {
                var charCode = value.charCodeAt(i);
                if (charCode > 255) {
                  _free(ptr);
                  throwBindingError("String has UTF-16 code units that do not fit in 8 bits");
                }
                SAFE_HEAP_STORE(ptr + i, charCode, 1);
              }
            } else {
              for (var i = 0; i < length; ++i) {
                SAFE_HEAP_STORE(ptr + i, value[i], 1);
              }
            }
          }
          if (destructors !== null) {
            destructors.push(_free, base);
          }
          return base;
        },
        "argPackAdvance": GenericWireTypeSize,
        "readValueFromPointer": readPointer,
        destructorFunction(ptr) {
          _free(ptr);
        }
      });
    };
    var UTF16Decoder = typeof TextDecoder != "undefined" ? new TextDecoder("utf-16le") : void 0;
    var UTF16ToString = (ptr, maxBytesToRead) => {
      assert(ptr % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
      var endPtr = ptr;
      var idx = endPtr >> 1;
      var maxIdx = idx + maxBytesToRead / 2;
      while (!(idx >= maxIdx) && SAFE_HEAP_LOAD(idx * 2, 2, 1)) ++idx;
      endPtr = idx << 1;
      if (endPtr - ptr > 32 && UTF16Decoder) return UTF16Decoder.decode(HEAPU8.subarray(ptr, endPtr));
      var str = "";
      for (var i = 0; !(i >= maxBytesToRead / 2); ++i) {
        var codeUnit = SAFE_HEAP_LOAD((ptr + i * 2 >> 1) * 2, 2, 0);
        if (codeUnit == 0) break;
        str += String.fromCharCode(codeUnit);
      }
      return str;
    };
    var stringToUTF16 = (str, outPtr, maxBytesToWrite) => {
      assert(outPtr % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!");
      assert(typeof maxBytesToWrite == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!");
      maxBytesToWrite ?? (maxBytesToWrite = 2147483647);
      if (maxBytesToWrite < 2) return 0;
      maxBytesToWrite -= 2;
      var startPtr = outPtr;
      var numCharsToWrite = maxBytesToWrite < str.length * 2 ? maxBytesToWrite / 2 : str.length;
      for (var i = 0; i < numCharsToWrite; ++i) {
        var codeUnit = str.charCodeAt(i);
        SAFE_HEAP_STORE((outPtr >> 1) * 2, codeUnit, 2);
        outPtr += 2;
      }
      SAFE_HEAP_STORE((outPtr >> 1) * 2, 0, 2);
      return outPtr - startPtr;
    };
    var lengthBytesUTF16 = (str) => str.length * 2;
    var UTF32ToString = (ptr, maxBytesToRead) => {
      assert(ptr % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
      var i = 0;
      var str = "";
      while (!(i >= maxBytesToRead / 4)) {
        var utf32 = SAFE_HEAP_LOAD((ptr + i * 4 >> 2) * 4, 4, 0);
        if (utf32 == 0) break;
        ++i;
        if (utf32 >= 65536) {
          var ch = utf32 - 65536;
          str += String.fromCharCode(55296 | ch >> 10, 56320 | ch & 1023);
        } else {
          str += String.fromCharCode(utf32);
        }
      }
      return str;
    };
    var stringToUTF32 = (str, outPtr, maxBytesToWrite) => {
      assert(outPtr % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!");
      assert(typeof maxBytesToWrite == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!");
      maxBytesToWrite ?? (maxBytesToWrite = 2147483647);
      if (maxBytesToWrite < 4) return 0;
      var startPtr = outPtr;
      var endPtr = startPtr + maxBytesToWrite - 4;
      for (var i = 0; i < str.length; ++i) {
        var codeUnit = str.charCodeAt(i);
        if (codeUnit >= 55296 && codeUnit <= 57343) {
          var trailSurrogate = str.charCodeAt(++i);
          codeUnit = 65536 + ((codeUnit & 1023) << 10) | trailSurrogate & 1023;
        }
        SAFE_HEAP_STORE((outPtr >> 2) * 4, codeUnit, 4);
        outPtr += 4;
        if (outPtr + 4 > endPtr) break;
      }
      SAFE_HEAP_STORE((outPtr >> 2) * 4, 0, 4);
      return outPtr - startPtr;
    };
    var lengthBytesUTF32 = (str) => {
      var len = 0;
      for (var i = 0; i < str.length; ++i) {
        var codeUnit = str.charCodeAt(i);
        if (codeUnit >= 55296 && codeUnit <= 57343) ++i;
        len += 4;
      }
      return len;
    };
    var __embind_register_std_wstring = (rawType, charSize, name) => {
      name = readLatin1String(name);
      var decodeString, encodeString, readCharAt, lengthBytesUTF;
      if (charSize === 2) {
        decodeString = UTF16ToString;
        encodeString = stringToUTF16;
        lengthBytesUTF = lengthBytesUTF16;
        readCharAt = (pointer) => SAFE_HEAP_LOAD((pointer >> 1) * 2, 2, 1);
      } else if (charSize === 4) {
        decodeString = UTF32ToString;
        encodeString = stringToUTF32;
        lengthBytesUTF = lengthBytesUTF32;
        readCharAt = (pointer) => SAFE_HEAP_LOAD((pointer >> 2) * 4, 4, 1);
      }
      registerType(rawType, {
        name,
        "fromWireType": (value) => {
          var length = SAFE_HEAP_LOAD((value >> 2) * 4, 4, 1);
          var str;
          var decodeStartPtr = value + 4;
          for (var i = 0; i <= length; ++i) {
            var currentBytePtr = value + 4 + i * charSize;
            if (i == length || readCharAt(currentBytePtr) == 0) {
              var maxReadBytes = currentBytePtr - decodeStartPtr;
              var stringSegment = decodeString(decodeStartPtr, maxReadBytes);
              if (str === void 0) {
                str = stringSegment;
              } else {
                str += String.fromCharCode(0);
                str += stringSegment;
              }
              decodeStartPtr = currentBytePtr + charSize;
            }
          }
          _free(value);
          return str;
        },
        "toWireType": (destructors, value) => {
          if (!(typeof value == "string")) {
            throwBindingError(`Cannot pass non-string to C++ string type ${name}`);
          }
          var length = lengthBytesUTF(value);
          var ptr = _malloc(4 + length + charSize);
          SAFE_HEAP_STORE((ptr >> 2) * 4, length / charSize, 4);
          encodeString(value, ptr + 4, length + charSize);
          if (destructors !== null) {
            destructors.push(_free, ptr);
          }
          return ptr;
        },
        "argPackAdvance": GenericWireTypeSize,
        "readValueFromPointer": readPointer,
        destructorFunction(ptr) {
          _free(ptr);
        }
      });
    };
    var __embind_register_value_object = (rawType, name, constructorSignature, rawConstructor, destructorSignature, rawDestructor) => {
      structRegistrations[rawType] = {
        name: readLatin1String(name),
        rawConstructor: embind__requireFunction(constructorSignature, rawConstructor),
        rawDestructor: embind__requireFunction(destructorSignature, rawDestructor),
        fields: []
      };
    };
    var __embind_register_value_object_field = (structType, fieldName, getterReturnType, getterSignature, getter, getterContext, setterArgumentType, setterSignature, setter, setterContext) => {
      structRegistrations[structType].fields.push({
        fieldName: readLatin1String(fieldName),
        getterReturnType,
        getter: embind__requireFunction(getterSignature, getter),
        getterContext,
        setterArgumentType,
        setter: embind__requireFunction(setterSignature, setter),
        setterContext
      });
    };
    var __embind_register_void = (rawType, name) => {
      name = readLatin1String(name);
      registerType(rawType, {
        isVoid: true,
        name,
        "argPackAdvance": 0,
        "fromWireType": () => void 0,
        "toWireType": (destructors, o) => void 0
      });
    };
    var nowIsMonotonic = 1;
    var __emscripten_get_now_is_monotonic = () => nowIsMonotonic;
    var __emscripten_memcpy_js = (dest, src, num) => HEAPU8.copyWithin(dest, src, src + num);
    var __emscripten_throw_longjmp = () => {
      throw Infinity;
    };
    var emval_returnValue = (returnType, destructorsRef, handle) => {
      var destructors = [];
      var result = returnType["toWireType"](destructors, handle);
      if (destructors.length) {
        SAFE_HEAP_STORE((destructorsRef >> 2) * 4, Emval.toHandle(destructors), 4);
      }
      return result;
    };
    var __emval_as = (handle, returnType, destructorsRef) => {
      handle = Emval.toValue(handle);
      returnType = requireRegisteredType(returnType, "emval::as");
      return emval_returnValue(returnType, destructorsRef, handle);
    };
    var emval_methodCallers = [];
    var __emval_call = (caller, handle, destructorsRef, args) => {
      caller = emval_methodCallers[caller];
      handle = Emval.toValue(handle);
      return caller(null, handle, destructorsRef, args);
    };
    var emval_symbols = {};
    var getStringOrSymbol = (address) => {
      var symbol = emval_symbols[address];
      if (symbol === void 0) {
        return readLatin1String(address);
      }
      return symbol;
    };
    var __emval_call_method = (caller, objHandle, methodName, destructorsRef, args) => {
      caller = emval_methodCallers[caller];
      objHandle = Emval.toValue(objHandle);
      methodName = getStringOrSymbol(methodName);
      return caller(objHandle, objHandle[methodName], destructorsRef, args);
    };
    var emval_get_global = () => {
      if (typeof globalThis == "object") {
        return globalThis;
      }
      return (/* @__PURE__ */ (function() {
        return Function;
      })())("return this")();
    };
    var __emval_get_global = (name) => {
      if (name === 0) {
        return Emval.toHandle(emval_get_global());
      } else {
        name = getStringOrSymbol(name);
        return Emval.toHandle(emval_get_global()[name]);
      }
    };
    var emval_addMethodCaller = (caller) => {
      var id = emval_methodCallers.length;
      emval_methodCallers.push(caller);
      return id;
    };
    var emval_lookupTypes = (argCount, argTypes) => {
      var a = new Array(argCount);
      for (var i = 0; i < argCount; ++i) {
        a[i] = requireRegisteredType(SAFE_HEAP_LOAD((argTypes + i * 4 >> 2) * 4, 4, 1), "parameter " + i);
      }
      return a;
    };
    var __emval_get_method_caller = (argCount, argTypes, kind) => {
      var types = emval_lookupTypes(argCount, argTypes);
      var retType = types.shift();
      argCount--;
      var functionBody = `return function (obj, func, destructorsRef, args) {
`;
      var offset = 0;
      var argsList = [];
      if (kind === /* FUNCTION */
      0) {
        argsList.push("obj");
      }
      var params = ["retType"];
      var args = [retType];
      for (var i = 0; i < argCount; ++i) {
        argsList.push("arg" + i);
        params.push("argType" + i);
        args.push(types[i]);
        functionBody += `  var arg${i} = argType${i}.readValueFromPointer(args${offset ? "+" + offset : ""});
`;
        offset += types[i]["argPackAdvance"];
      }
      var invoker = kind === /* CONSTRUCTOR */
      1 ? "new func" : "func.call";
      functionBody += `  var rv = ${invoker}(${argsList.join(", ")});
`;
      if (!retType.isVoid) {
        params.push("emval_returnValue");
        args.push(emval_returnValue);
        functionBody += "  return emval_returnValue(retType, destructorsRef, rv);\n";
      }
      functionBody += "};\n";
      params.push(functionBody);
      var invokerFunction = newFunc(Function, params)(...args);
      var functionName = `methodCaller<(${types.map((t) => t.name).join(", ")}) => ${retType.name}>`;
      return emval_addMethodCaller(createNamedFunction(functionName, invokerFunction));
    };
    var __emval_get_module_property = (name) => {
      name = getStringOrSymbol(name);
      return Emval.toHandle(Module[name]);
    };
    var __emval_get_property = (handle, key) => {
      handle = Emval.toValue(handle);
      key = Emval.toValue(key);
      return Emval.toHandle(handle[key]);
    };
    var __emval_incref = (handle) => {
      if (handle > 9) {
        emval_handles[handle + 1] += 1;
      }
    };
    var __emval_instanceof = (object, constructor) => {
      object = Emval.toValue(object);
      constructor = Emval.toValue(constructor);
      return object instanceof constructor;
    };
    var __emval_is_number = (handle) => {
      handle = Emval.toValue(handle);
      return typeof handle == "number";
    };
    var __emval_new_cstring = (v) => Emval.toHandle(getStringOrSymbol(v));
    var __emval_run_destructors = (handle) => {
      var destructors = Emval.toValue(handle);
      runDestructors(destructors);
      __emval_decref(handle);
    };
    var __emval_take_value = (type, arg) => {
      type = requireRegisteredType(type, "_emval_take_value");
      var v = type["readValueFromPointer"](arg);
      return Emval.toHandle(v);
    };
    var convertI32PairToI53Checked = (lo, hi) => {
      assert(lo == lo >>> 0 || lo == (lo | 0));
      assert(hi === (hi | 0));
      return hi + 2097152 >>> 0 < 4194305 - !!lo ? (lo >>> 0) + hi * 4294967296 : NaN;
    };
    function __mmap_js(len, prot, flags, fd, offset_low, offset_high, allocated, addr) {
      var offset = convertI32PairToI53Checked(offset_low, offset_high);
      try {
        if (isNaN(offset)) return 61;
        var stream = SYSCALLS.getStreamFromFD(fd);
        var res = FS.mmap(stream, len, offset, prot, flags);
        var ptr = res.ptr;
        SAFE_HEAP_STORE((allocated >> 2) * 4, res.allocated, 4);
        SAFE_HEAP_STORE((addr >> 2) * 4, ptr, 4);
        return 0;
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    function __munmap_js(addr, len, prot, flags, fd, offset_low, offset_high) {
      var offset = convertI32PairToI53Checked(offset_low, offset_high);
      try {
        var stream = SYSCALLS.getStreamFromFD(fd);
        if (prot & 2) {
          SYSCALLS.doMsync(addr, stream, len, flags, offset);
        }
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return -e.errno;
      }
    }
    var _abort = () => {
      abort("native code called abort()");
    };
    var _emscripten_date_now = () => Date.now();
    var _emscripten_err = (str) => err(UTF8ToString(str));
    var JSEvents = {
      removeAllEventListeners() {
        while (JSEvents.eventHandlers.length) {
          JSEvents._removeHandler(JSEvents.eventHandlers.length - 1);
        }
        JSEvents.deferredCalls = [];
      },
      inEventHandler: 0,
      deferredCalls: [],
      deferCall(targetFunction, precedence, argsList) {
        function arraysHaveEqualContent(arrA, arrB) {
          if (arrA.length != arrB.length) return false;
          for (var i2 in arrA) {
            if (arrA[i2] != arrB[i2]) return false;
          }
          return true;
        }
        for (var i in JSEvents.deferredCalls) {
          var call = JSEvents.deferredCalls[i];
          if (call.targetFunction == targetFunction && arraysHaveEqualContent(call.argsList, argsList)) {
            return;
          }
        }
        JSEvents.deferredCalls.push({
          targetFunction,
          precedence,
          argsList
        });
        JSEvents.deferredCalls.sort((x, y) => x.precedence < y.precedence);
      },
      removeDeferredCalls(targetFunction) {
        for (var i = 0; i < JSEvents.deferredCalls.length; ++i) {
          if (JSEvents.deferredCalls[i].targetFunction == targetFunction) {
            JSEvents.deferredCalls.splice(i, 1);
            --i;
          }
        }
      },
      canPerformEventHandlerRequests() {
        if (navigator.userActivation) {
          return navigator.userActivation.isActive;
        }
        return JSEvents.inEventHandler && JSEvents.currentEventHandler.allowsDeferredCalls;
      },
      runDeferredCalls() {
        if (!JSEvents.canPerformEventHandlerRequests()) {
          return;
        }
        for (var i = 0; i < JSEvents.deferredCalls.length; ++i) {
          var call = JSEvents.deferredCalls[i];
          JSEvents.deferredCalls.splice(i, 1);
          --i;
          call.targetFunction(...call.argsList);
        }
      },
      eventHandlers: [],
      removeAllHandlersOnTarget: (target, eventTypeString) => {
        for (var i = 0; i < JSEvents.eventHandlers.length; ++i) {
          if (JSEvents.eventHandlers[i].target == target && (!eventTypeString || eventTypeString == JSEvents.eventHandlers[i].eventTypeString)) {
            JSEvents._removeHandler(i--);
          }
        }
      },
      _removeHandler(i) {
        var h = JSEvents.eventHandlers[i];
        h.target.removeEventListener(h.eventTypeString, h.eventListenerFunc, h.useCapture);
        JSEvents.eventHandlers.splice(i, 1);
      },
      registerOrRemoveHandler(eventHandler) {
        if (!eventHandler.target) {
          err("registerOrRemoveHandler: the target element for event handler registration does not exist, when processing the following event handler registration:");
          console.dir(eventHandler);
          return -4;
        }
        if (eventHandler.callbackfunc) {
          eventHandler.eventListenerFunc = function(event) {
            ++JSEvents.inEventHandler;
            JSEvents.currentEventHandler = eventHandler;
            JSEvents.runDeferredCalls();
            eventHandler.handlerFunc(event);
            JSEvents.runDeferredCalls();
            --JSEvents.inEventHandler;
          };
          eventHandler.target.addEventListener(eventHandler.eventTypeString, eventHandler.eventListenerFunc, eventHandler.useCapture);
          JSEvents.eventHandlers.push(eventHandler);
        } else {
          for (var i = 0; i < JSEvents.eventHandlers.length; ++i) {
            if (JSEvents.eventHandlers[i].target == eventHandler.target && JSEvents.eventHandlers[i].eventTypeString == eventHandler.eventTypeString) {
              JSEvents._removeHandler(i--);
            }
          }
        }
        return 0;
      },
      getNodeNameForTarget(target) {
        if (!target) return "";
        if (target == window) return "#window";
        if (target == screen) return "#screen";
        return target?.nodeName || "";
      },
      fullscreenEnabled() {
        return document.fullscreenEnabled || document.webkitFullscreenEnabled;
      }
    };
    var maybeCStringToJsString = (cString) => cString > 2 ? UTF8ToString(cString) : cString;
    var specialHTMLTargets = [0, typeof document != "undefined" ? document : 0, typeof window != "undefined" ? window : 0];
    var findEventTarget = (target) => {
      target = maybeCStringToJsString(target);
      var domElement = specialHTMLTargets[target] || (typeof document != "undefined" ? document.querySelector(target) : void 0);
      return domElement;
    };
    var findCanvasEventTarget = findEventTarget;
    var _emscripten_get_canvas_element_size = (target, width, height) => {
      var canvas = findCanvasEventTarget(target);
      if (!canvas) return -4;
      SAFE_HEAP_STORE((width >> 2) * 4, canvas.width, 4);
      SAFE_HEAP_STORE((height >> 2) * 4, canvas.height, 4);
    };
    var getHeapMax = () => 2147483648;
    var _emscripten_get_heap_max = () => getHeapMax();
    var _emscripten_get_now;
    _emscripten_get_now = () => performance.now();
    var webgl_enable_ANGLE_instanced_arrays = (ctx) => {
      var ext = ctx.getExtension("ANGLE_instanced_arrays");
      if (ext) {
        ctx["vertexAttribDivisor"] = (index, divisor) => ext["vertexAttribDivisorANGLE"](index, divisor);
        ctx["drawArraysInstanced"] = (mode, first, count, primcount) => ext["drawArraysInstancedANGLE"](mode, first, count, primcount);
        ctx["drawElementsInstanced"] = (mode, count, type, indices, primcount) => ext["drawElementsInstancedANGLE"](mode, count, type, indices, primcount);
        return 1;
      }
    };
    var webgl_enable_OES_vertex_array_object = (ctx) => {
      var ext = ctx.getExtension("OES_vertex_array_object");
      if (ext) {
        ctx["createVertexArray"] = () => ext["createVertexArrayOES"]();
        ctx["deleteVertexArray"] = (vao) => ext["deleteVertexArrayOES"](vao);
        ctx["bindVertexArray"] = (vao) => ext["bindVertexArrayOES"](vao);
        ctx["isVertexArray"] = (vao) => ext["isVertexArrayOES"](vao);
        return 1;
      }
    };
    var webgl_enable_WEBGL_draw_buffers = (ctx) => {
      var ext = ctx.getExtension("WEBGL_draw_buffers");
      if (ext) {
        ctx["drawBuffers"] = (n, bufs) => ext["drawBuffersWEBGL"](n, bufs);
        return 1;
      }
    };
    var webgl_enable_WEBGL_draw_instanced_base_vertex_base_instance = (ctx) => !!(ctx.dibvbi = ctx.getExtension("WEBGL_draw_instanced_base_vertex_base_instance"));
    var webgl_enable_WEBGL_multi_draw_instanced_base_vertex_base_instance = (ctx) => !!(ctx.mdibvbi = ctx.getExtension("WEBGL_multi_draw_instanced_base_vertex_base_instance"));
    var webgl_enable_WEBGL_multi_draw = (ctx) => !!(ctx.multiDrawWebgl = ctx.getExtension("WEBGL_multi_draw"));
    var getEmscriptenSupportedExtensions = (ctx) => {
      var supportedExtensions = ["ANGLE_instanced_arrays", "EXT_blend_minmax", "EXT_disjoint_timer_query", "EXT_frag_depth", "EXT_shader_texture_lod", "EXT_sRGB", "OES_element_index_uint", "OES_fbo_render_mipmap", "OES_standard_derivatives", "OES_texture_float", "OES_texture_half_float", "OES_texture_half_float_linear", "OES_vertex_array_object", "WEBGL_color_buffer_float", "WEBGL_depth_texture", "WEBGL_draw_buffers", "EXT_color_buffer_float", "EXT_conservative_depth", "EXT_disjoint_timer_query_webgl2", "EXT_texture_norm16", "NV_shader_noperspective_interpolation", "WEBGL_clip_cull_distance", "EXT_color_buffer_half_float", "EXT_depth_clamp", "EXT_float_blend", "EXT_texture_compression_bptc", "EXT_texture_compression_rgtc", "EXT_texture_filter_anisotropic", "KHR_parallel_shader_compile", "OES_texture_float_linear", "WEBGL_blend_func_extended", "WEBGL_compressed_texture_astc", "WEBGL_compressed_texture_etc", "WEBGL_compressed_texture_etc1", "WEBGL_compressed_texture_s3tc", "WEBGL_compressed_texture_s3tc_srgb", "WEBGL_debug_renderer_info", "WEBGL_debug_shaders", "WEBGL_lose_context", "WEBGL_multi_draw"];
      return (ctx.getSupportedExtensions() || []).filter((ext) => supportedExtensions.includes(ext));
    };
    var GL = {
      counter: 1,
      buffers: [],
      programs: [],
      framebuffers: [],
      renderbuffers: [],
      textures: [],
      shaders: [],
      vaos: [],
      contexts: [],
      offscreenCanvases: {},
      queries: [],
      samplers: [],
      transformFeedbacks: [],
      syncs: [],
      stringCache: {},
      stringiCache: {},
      unpackAlignment: 4,
      recordError: (errorCode) => {
        if (!GL.lastError) {
          GL.lastError = errorCode;
        }
      },
      getNewId: (table) => {
        var ret = GL.counter++;
        for (var i = table.length; i < ret; i++) {
          table[i] = null;
        }
        return ret;
      },
      genObject: (n, buffers, createFunction, objectTable) => {
        for (var i = 0; i < n; i++) {
          var buffer = GLctx[createFunction]();
          var id = buffer && GL.getNewId(objectTable);
          if (buffer) {
            buffer.name = id;
            objectTable[id] = buffer;
          } else {
            GL.recordError(1282);
          }
          SAFE_HEAP_STORE((buffers + i * 4 >> 2) * 4, id, 4);
        }
      },
      getSource: (shader, count, string, length) => {
        var source = "";
        for (var i = 0; i < count; ++i) {
          var len = length ? SAFE_HEAP_LOAD((length + i * 4 >> 2) * 4, 4, 1) : void 0;
          source += UTF8ToString(SAFE_HEAP_LOAD((string + i * 4 >> 2) * 4, 4, 1), len);
        }
        return source;
      },
      createContext: (canvas, webGLContextAttributes) => {
        if (!canvas.getContextSafariWebGL2Fixed) {
          let fixedGetContext2 = function(ver, attrs) {
            var gl = canvas.getContextSafariWebGL2Fixed(ver, attrs);
            return ver == "webgl" == gl instanceof WebGLRenderingContext ? gl : null;
          };
          canvas.getContextSafariWebGL2Fixed = canvas.getContext;
          canvas.getContext = fixedGetContext2;
        }
        var ctx = webGLContextAttributes.majorVersion > 1 ? canvas.getContext("webgl2", webGLContextAttributes) : canvas.getContext("webgl", webGLContextAttributes);
        if (!ctx) return 0;
        var handle = GL.registerContext(ctx, webGLContextAttributes);
        return handle;
      },
      registerContext: (ctx, webGLContextAttributes) => {
        var handle = GL.getNewId(GL.contexts);
        var context = {
          handle,
          attributes: webGLContextAttributes,
          version: webGLContextAttributes.majorVersion,
          GLctx: ctx
        };
        if (ctx.canvas) ctx.canvas.GLctxObject = context;
        GL.contexts[handle] = context;
        if (typeof webGLContextAttributes.enableExtensionsByDefault == "undefined" || webGLContextAttributes.enableExtensionsByDefault) {
          GL.initExtensions(context);
        }
        return handle;
      },
      makeContextCurrent: (contextHandle) => {
        GL.currentContext = GL.contexts[contextHandle];
        Module.ctx = GLctx = GL.currentContext?.GLctx;
        return !(contextHandle && !GLctx);
      },
      getContext: (contextHandle) => GL.contexts[contextHandle],
      deleteContext: (contextHandle) => {
        if (GL.currentContext === GL.contexts[contextHandle]) {
          GL.currentContext = null;
        }
        if (typeof JSEvents == "object") {
          JSEvents.removeAllHandlersOnTarget(GL.contexts[contextHandle].GLctx.canvas);
        }
        if (GL.contexts[contextHandle] && GL.contexts[contextHandle].GLctx.canvas) {
          GL.contexts[contextHandle].GLctx.canvas.GLctxObject = void 0;
        }
        GL.contexts[contextHandle] = null;
      },
      initExtensions: (context) => {
        context || (context = GL.currentContext);
        if (context.initExtensionsDone) return;
        context.initExtensionsDone = true;
        var GLctx2 = context.GLctx;
        webgl_enable_ANGLE_instanced_arrays(GLctx2);
        webgl_enable_OES_vertex_array_object(GLctx2);
        webgl_enable_WEBGL_draw_buffers(GLctx2);
        webgl_enable_WEBGL_draw_instanced_base_vertex_base_instance(GLctx2);
        webgl_enable_WEBGL_multi_draw_instanced_base_vertex_base_instance(GLctx2);
        if (context.version >= 2) {
          GLctx2.disjointTimerQueryExt = GLctx2.getExtension("EXT_disjoint_timer_query_webgl2");
        }
        if (context.version < 2 || !GLctx2.disjointTimerQueryExt) {
          GLctx2.disjointTimerQueryExt = GLctx2.getExtension("EXT_disjoint_timer_query");
        }
        webgl_enable_WEBGL_multi_draw(GLctx2);
        getEmscriptenSupportedExtensions(GLctx2).forEach((ext) => {
          if (!ext.includes("lose_context") && !ext.includes("debug")) {
            GLctx2.getExtension(ext);
          }
        });
      }
    };
    var _glActiveTexture = (x0) => GLctx.activeTexture(x0);
    var _emscripten_glActiveTexture = _glActiveTexture;
    var _glAttachShader = (program, shader) => {
      GLctx.attachShader(GL.programs[program], GL.shaders[shader]);
    };
    var _emscripten_glAttachShader = _glAttachShader;
    var _glBindBuffer = (target, buffer) => {
      if (target == 35051) {
        GLctx.currentPixelPackBufferBinding = buffer;
      } else if (target == 35052) {
        GLctx.currentPixelUnpackBufferBinding = buffer;
      }
      GLctx.bindBuffer(target, GL.buffers[buffer]);
    };
    var _emscripten_glBindBuffer = _glBindBuffer;
    var _glBindBufferRange = (target, index, buffer, offset, ptrsize) => {
      GLctx.bindBufferRange(target, index, GL.buffers[buffer], offset, ptrsize);
    };
    var _emscripten_glBindBufferRange = _glBindBufferRange;
    var _glBindFramebuffer = (target, framebuffer) => {
      GLctx.bindFramebuffer(target, GL.framebuffers[framebuffer]);
    };
    var _emscripten_glBindFramebuffer = _glBindFramebuffer;
    var _glBindRenderbuffer = (target, renderbuffer) => {
      GLctx.bindRenderbuffer(target, GL.renderbuffers[renderbuffer]);
    };
    var _emscripten_glBindRenderbuffer = _glBindRenderbuffer;
    var _glBindTexture = (target, texture) => {
      GLctx.bindTexture(target, GL.textures[texture]);
    };
    var _emscripten_glBindTexture = _glBindTexture;
    var _glBindVertexArray = (vao) => {
      GLctx.bindVertexArray(GL.vaos[vao]);
    };
    var _emscripten_glBindVertexArray = _glBindVertexArray;
    var _glBindVertexArrayOES = _glBindVertexArray;
    var _emscripten_glBindVertexArrayOES = _glBindVertexArrayOES;
    var _glBlendEquation = (x0) => GLctx.blendEquation(x0);
    var _emscripten_glBlendEquation = _glBlendEquation;
    var _glBlendEquationSeparate = (x0, x1) => GLctx.blendEquationSeparate(x0, x1);
    var _emscripten_glBlendEquationSeparate = _glBlendEquationSeparate;
    var _glBlendFunc = (x0, x1) => GLctx.blendFunc(x0, x1);
    var _emscripten_glBlendFunc = _glBlendFunc;
    var _glBlendFuncSeparate = (x0, x1, x2, x3) => GLctx.blendFuncSeparate(x0, x1, x2, x3);
    var _emscripten_glBlendFuncSeparate = _glBlendFuncSeparate;
    var _glBlitFramebuffer = (x0, x1, x2, x3, x4, x5, x6, x7, x8, x9) => GLctx.blitFramebuffer(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9);
    var _emscripten_glBlitFramebuffer = _glBlitFramebuffer;
    var _glBufferData = (target, size, data, usage) => {
      if (GL.currentContext.version >= 2) {
        if (data && size) {
          GLctx.bufferData(target, HEAPU8, usage, data, size);
        } else {
          GLctx.bufferData(target, size, usage);
        }
        return;
      }
      GLctx.bufferData(target, data ? HEAPU8.subarray(data, data + size) : size, usage);
    };
    var _emscripten_glBufferData = _glBufferData;
    var _glBufferSubData = (target, offset, size, data) => {
      if (GL.currentContext.version >= 2) {
        size && GLctx.bufferSubData(target, offset, HEAPU8, data, size);
        return;
      }
      GLctx.bufferSubData(target, offset, HEAPU8.subarray(data, data + size));
    };
    var _emscripten_glBufferSubData = _glBufferSubData;
    var _glCheckFramebufferStatus = (x0) => GLctx.checkFramebufferStatus(x0);
    var _emscripten_glCheckFramebufferStatus = _glCheckFramebufferStatus;
    var _glClear = (x0) => GLctx.clear(x0);
    var _emscripten_glClear = _glClear;
    var _glClearColor = (x0, x1, x2, x3) => GLctx.clearColor(x0, x1, x2, x3);
    var _emscripten_glClearColor = _glClearColor;
    var _glClearDepthf = (x0) => GLctx.clearDepth(x0);
    var _emscripten_glClearDepthf = _glClearDepthf;
    var _glClearStencil = (x0) => GLctx.clearStencil(x0);
    var _emscripten_glClearStencil = _glClearStencil;
    var convertI32PairToI53 = (lo, hi) => {
      assert(hi === (hi | 0));
      return (lo >>> 0) + hi * 4294967296;
    };
    var _glClientWaitSync = (sync, flags, timeout_low, timeout_high) => {
      var timeout = convertI32PairToI53(timeout_low, timeout_high);
      return GLctx.clientWaitSync(GL.syncs[sync], flags, timeout);
    };
    var _emscripten_glClientWaitSync = _glClientWaitSync;
    var _glColorMask = (red, green, blue, alpha) => {
      GLctx.colorMask(!!red, !!green, !!blue, !!alpha);
    };
    var _emscripten_glColorMask = _glColorMask;
    var _glCompileShader = (shader) => {
      GLctx.compileShader(GL.shaders[shader]);
    };
    var _emscripten_glCompileShader = _glCompileShader;
    var _glCopyTexSubImage2D = (x0, x1, x2, x3, x4, x5, x6, x7) => GLctx.copyTexSubImage2D(x0, x1, x2, x3, x4, x5, x6, x7);
    var _emscripten_glCopyTexSubImage2D = _glCopyTexSubImage2D;
    var _glCreateProgram = () => {
      var id = GL.getNewId(GL.programs);
      var program = GLctx.createProgram();
      program.name = id;
      program.maxUniformLength = program.maxAttributeLength = program.maxUniformBlockNameLength = 0;
      program.uniformIdCounter = 1;
      GL.programs[id] = program;
      return id;
    };
    var _emscripten_glCreateProgram = _glCreateProgram;
    var _glCreateShader = (shaderType) => {
      var id = GL.getNewId(GL.shaders);
      GL.shaders[id] = GLctx.createShader(shaderType);
      return id;
    };
    var _emscripten_glCreateShader = _glCreateShader;
    var _glDeleteBuffers = (n, buffers) => {
      for (var i = 0; i < n; i++) {
        var id = SAFE_HEAP_LOAD((buffers + i * 4 >> 2) * 4, 4, 0);
        var buffer = GL.buffers[id];
        if (!buffer) continue;
        GLctx.deleteBuffer(buffer);
        buffer.name = 0;
        GL.buffers[id] = null;
        if (id == GLctx.currentPixelPackBufferBinding) GLctx.currentPixelPackBufferBinding = 0;
        if (id == GLctx.currentPixelUnpackBufferBinding) GLctx.currentPixelUnpackBufferBinding = 0;
      }
    };
    var _emscripten_glDeleteBuffers = _glDeleteBuffers;
    var _glDeleteFramebuffers = (n, framebuffers) => {
      for (var i = 0; i < n; ++i) {
        var id = SAFE_HEAP_LOAD((framebuffers + i * 4 >> 2) * 4, 4, 0);
        var framebuffer = GL.framebuffers[id];
        if (!framebuffer) continue;
        GLctx.deleteFramebuffer(framebuffer);
        framebuffer.name = 0;
        GL.framebuffers[id] = null;
      }
    };
    var _emscripten_glDeleteFramebuffers = _glDeleteFramebuffers;
    var _glDeleteProgram = (id) => {
      if (!id) return;
      var program = GL.programs[id];
      if (!program) {
        GL.recordError(1281);
        return;
      }
      GLctx.deleteProgram(program);
      program.name = 0;
      GL.programs[id] = null;
    };
    var _emscripten_glDeleteProgram = _glDeleteProgram;
    var _glDeleteRenderbuffers = (n, renderbuffers) => {
      for (var i = 0; i < n; i++) {
        var id = SAFE_HEAP_LOAD((renderbuffers + i * 4 >> 2) * 4, 4, 0);
        var renderbuffer = GL.renderbuffers[id];
        if (!renderbuffer) continue;
        GLctx.deleteRenderbuffer(renderbuffer);
        renderbuffer.name = 0;
        GL.renderbuffers[id] = null;
      }
    };
    var _emscripten_glDeleteRenderbuffers = _glDeleteRenderbuffers;
    var _glDeleteShader = (id) => {
      if (!id) return;
      var shader = GL.shaders[id];
      if (!shader) {
        GL.recordError(1281);
        return;
      }
      GLctx.deleteShader(shader);
      GL.shaders[id] = null;
    };
    var _emscripten_glDeleteShader = _glDeleteShader;
    var _glDeleteSync = (id) => {
      if (!id) return;
      var sync = GL.syncs[id];
      if (!sync) {
        GL.recordError(1281);
        return;
      }
      GLctx.deleteSync(sync);
      sync.name = 0;
      GL.syncs[id] = null;
    };
    var _emscripten_glDeleteSync = _glDeleteSync;
    var _glDeleteTextures = (n, textures) => {
      for (var i = 0; i < n; i++) {
        var id = SAFE_HEAP_LOAD((textures + i * 4 >> 2) * 4, 4, 0);
        var texture = GL.textures[id];
        if (!texture) continue;
        GLctx.deleteTexture(texture);
        texture.name = 0;
        GL.textures[id] = null;
      }
    };
    var _emscripten_glDeleteTextures = _glDeleteTextures;
    var _glDeleteVertexArrays = (n, vaos) => {
      for (var i = 0; i < n; i++) {
        var id = SAFE_HEAP_LOAD((vaos + i * 4 >> 2) * 4, 4, 0);
        GLctx.deleteVertexArray(GL.vaos[id]);
        GL.vaos[id] = null;
      }
    };
    var _emscripten_glDeleteVertexArrays = _glDeleteVertexArrays;
    var _glDeleteVertexArraysOES = _glDeleteVertexArrays;
    var _emscripten_glDeleteVertexArraysOES = _glDeleteVertexArraysOES;
    var _glDepthFunc = (x0) => GLctx.depthFunc(x0);
    var _emscripten_glDepthFunc = _glDepthFunc;
    var _glDepthMask = (flag) => {
      GLctx.depthMask(!!flag);
    };
    var _emscripten_glDepthMask = _glDepthMask;
    var _glDisable = (x0) => GLctx.disable(x0);
    var _emscripten_glDisable = _glDisable;
    var _glDrawArrays = (mode, first, count) => {
      GLctx.drawArrays(mode, first, count);
    };
    var _emscripten_glDrawArrays = _glDrawArrays;
    var _glDrawArraysInstanced = (mode, first, count, primcount) => {
      GLctx.drawArraysInstanced(mode, first, count, primcount);
    };
    var _emscripten_glDrawArraysInstanced = _glDrawArraysInstanced;
    var _glDrawElements = (mode, count, type, indices) => {
      GLctx.drawElements(mode, count, type, indices);
    };
    var _emscripten_glDrawElements = _glDrawElements;
    var _glDrawElementsInstanced = (mode, count, type, indices, primcount) => {
      GLctx.drawElementsInstanced(mode, count, type, indices, primcount);
    };
    var _emscripten_glDrawElementsInstanced = _glDrawElementsInstanced;
    var _glEnable = (x0) => GLctx.enable(x0);
    var _emscripten_glEnable = _glEnable;
    var _glEnableVertexAttribArray = (index) => {
      GLctx.enableVertexAttribArray(index);
    };
    var _emscripten_glEnableVertexAttribArray = _glEnableVertexAttribArray;
    var _glFenceSync = (condition, flags) => {
      var sync = GLctx.fenceSync(condition, flags);
      if (sync) {
        var id = GL.getNewId(GL.syncs);
        sync.name = id;
        GL.syncs[id] = sync;
        return id;
      }
      return 0;
    };
    var _emscripten_glFenceSync = _glFenceSync;
    var _glFinish = () => GLctx.finish();
    var _emscripten_glFinish = _glFinish;
    var _glFlush = () => GLctx.flush();
    var _emscripten_glFlush = _glFlush;
    var _glFramebufferRenderbuffer = (target, attachment, renderbuffertarget, renderbuffer) => {
      GLctx.framebufferRenderbuffer(target, attachment, renderbuffertarget, GL.renderbuffers[renderbuffer]);
    };
    var _emscripten_glFramebufferRenderbuffer = _glFramebufferRenderbuffer;
    var _glFramebufferTexture2D = (target, attachment, textarget, texture, level) => {
      GLctx.framebufferTexture2D(target, attachment, textarget, GL.textures[texture], level);
    };
    var _emscripten_glFramebufferTexture2D = _glFramebufferTexture2D;
    var _glGenBuffers = (n, buffers) => {
      GL.genObject(n, buffers, "createBuffer", GL.buffers);
    };
    var _emscripten_glGenBuffers = _glGenBuffers;
    var _glGenFramebuffers = (n, ids) => {
      GL.genObject(n, ids, "createFramebuffer", GL.framebuffers);
    };
    var _emscripten_glGenFramebuffers = _glGenFramebuffers;
    var _glGenRenderbuffers = (n, renderbuffers) => {
      GL.genObject(n, renderbuffers, "createRenderbuffer", GL.renderbuffers);
    };
    var _emscripten_glGenRenderbuffers = _glGenRenderbuffers;
    var _glGenTextures = (n, textures) => {
      GL.genObject(n, textures, "createTexture", GL.textures);
    };
    var _emscripten_glGenTextures = _glGenTextures;
    var _glGenVertexArrays = (n, arrays) => {
      GL.genObject(n, arrays, "createVertexArray", GL.vaos);
    };
    var _emscripten_glGenVertexArrays = _glGenVertexArrays;
    var _glGenVertexArraysOES = _glGenVertexArrays;
    var _emscripten_glGenVertexArraysOES = _glGenVertexArraysOES;
    var _glGenerateMipmap = (x0) => GLctx.generateMipmap(x0);
    var _emscripten_glGenerateMipmap = _glGenerateMipmap;
    var _glGetAttribLocation = (program, name) => GLctx.getAttribLocation(GL.programs[program], UTF8ToString(name));
    var _emscripten_glGetAttribLocation = _glGetAttribLocation;
    var _glGetBufferSubData = (target, offset, size, data) => {
      if (!data) {
        GL.recordError(1281);
        return;
      }
      size && GLctx.getBufferSubData(target, offset, HEAPU8, data, size);
    };
    var _emscripten_glGetBufferSubData = _glGetBufferSubData;
    var _glGetError = () => {
      var error = GLctx.getError() || GL.lastError;
      GL.lastError = 0;
      return error;
    };
    var _emscripten_glGetError = _glGetError;
    var readI53FromI64 = (ptr) => SAFE_HEAP_LOAD((ptr >> 2) * 4, 4, 1) + SAFE_HEAP_LOAD((ptr + 4 >> 2) * 4, 4, 0) * 4294967296;
    var readI53FromU64 = (ptr) => SAFE_HEAP_LOAD((ptr >> 2) * 4, 4, 1) + SAFE_HEAP_LOAD((ptr + 4 >> 2) * 4, 4, 1) * 4294967296;
    var writeI53ToI64 = (ptr, num) => {
      SAFE_HEAP_STORE((ptr >> 2) * 4, num, 4);
      var lower = SAFE_HEAP_LOAD((ptr >> 2) * 4, 4, 1);
      SAFE_HEAP_STORE((ptr + 4 >> 2) * 4, (num - lower) / 4294967296, 4);
      var deserialized = num >= 0 ? readI53FromU64(ptr) : readI53FromI64(ptr);
      var offset = ptr >> 2;
      if (deserialized != num) warnOnce(`writeI53ToI64() out of range: serialized JS Number ${num} to Wasm heap as bytes lo=${ptrToString(SAFE_HEAP_LOAD(offset * 4, 4, 1))}, hi=${ptrToString(SAFE_HEAP_LOAD((offset + 1) * 4, 4, 1))}, which deserializes back to ${deserialized} instead!`);
    };
    var webglGetExtensions = function $webglGetExtensions() {
      var exts = getEmscriptenSupportedExtensions(GLctx);
      exts = exts.concat(exts.map((e) => "GL_" + e));
      return exts;
    };
    var emscriptenWebGLGet = (name_, p, type) => {
      if (!p) {
        GL.recordError(1281);
        return;
      }
      var ret = void 0;
      switch (name_) {
        case 36346:
          ret = 1;
          break;
        case 36344:
          return;
        case 34814:
        case 36345:
          ret = 0;
          break;
        case 34466:
          var formats = GLctx.getParameter(34467);
          ret = formats ? formats.length : 0;
          break;
        case 33309:
          if (GL.currentContext.version < 2) {
            GL.recordError(1282);
            return;
          }
          ret = webglGetExtensions().length;
          break;
        case 33307:
        case 33308:
          if (GL.currentContext.version < 2) {
            GL.recordError(1280);
            return;
          }
          ret = name_ == 33307 ? 3 : 0;
          break;
      }
      if (ret === void 0) {
        var result = GLctx.getParameter(name_);
        switch (typeof result) {
          case "number":
            ret = result;
            break;
          case "boolean":
            ret = result ? 1 : 0;
            break;
          case "string":
            GL.recordError(1280);
            return;
          case "object":
            if (result === null) {
              switch (name_) {
                case 34964:
                case 35725:
                case 34965:
                case 36006:
                case 36007:
                case 32873:
                case 34229:
                case 36662:
                case 36663:
                case 35053:
                case 35055:
                case 36010:
                case 35097:
                case 35869:
                case 32874:
                case 36389:
                case 35983:
                case 35368:
                case 34068: {
                  ret = 0;
                  break;
                }
                default: {
                  GL.recordError(1280);
                  return;
                }
              }
            } else if (result instanceof Float32Array || result instanceof Uint32Array || result instanceof Int32Array || result instanceof Array) {
              for (var i = 0; i < result.length; ++i) {
                switch (type) {
                  case 0:
                    SAFE_HEAP_STORE((p + i * 4 >> 2) * 4, result[i], 4);
                    break;
                  case 2:
                    SAFE_HEAP_STORE_D((p + i * 4 >> 2) * 4, result[i], 4);
                    break;
                  case 4:
                    SAFE_HEAP_STORE(p + i, result[i] ? 1 : 0, 1);
                    break;
                }
              }
              return;
            } else {
              try {
                ret = result.name | 0;
              } catch (e) {
                GL.recordError(1280);
                err(`GL_INVALID_ENUM in glGet${type}v: Unknown object returned from WebGL getParameter(${name_})! (error: ${e})`);
                return;
              }
            }
            break;
          default:
            GL.recordError(1280);
            err(`GL_INVALID_ENUM in glGet${type}v: Native code calling glGet${type}v(${name_}) and it returns ${result} of type ${typeof result}!`);
            return;
        }
      }
      switch (type) {
        case 1:
          writeI53ToI64(p, ret);
          break;
        case 0:
          SAFE_HEAP_STORE((p >> 2) * 4, ret, 4);
          break;
        case 2:
          SAFE_HEAP_STORE_D((p >> 2) * 4, ret, 4);
          break;
        case 4:
          SAFE_HEAP_STORE(p, ret ? 1 : 0, 1);
          break;
      }
    };
    var _glGetIntegerv = (name_, p) => emscriptenWebGLGet(name_, p, 0);
    var _emscripten_glGetIntegerv = _glGetIntegerv;
    var _glGetInternalformativ = (target, internalformat, pname, bufSize, params) => {
      if (bufSize < 0) {
        GL.recordError(1281);
        return;
      }
      if (!params) {
        GL.recordError(1281);
        return;
      }
      var ret = GLctx.getInternalformatParameter(target, internalformat, pname);
      if (ret === null) return;
      for (var i = 0; i < ret.length && i < bufSize; ++i) {
        SAFE_HEAP_STORE((params + i * 4 >> 2) * 4, ret[i], 4);
      }
    };
    var _emscripten_glGetInternalformativ = _glGetInternalformativ;
    var _glGetProgramInfoLog = (program, maxLength, length, infoLog) => {
      var log = GLctx.getProgramInfoLog(GL.programs[program]);
      if (log === null) log = "(unknown error)";
      var numBytesWrittenExclNull = maxLength > 0 && infoLog ? stringToUTF8(log, infoLog, maxLength) : 0;
      if (length) SAFE_HEAP_STORE((length >> 2) * 4, numBytesWrittenExclNull, 4);
    };
    var _emscripten_glGetProgramInfoLog = _glGetProgramInfoLog;
    var _glGetProgramiv = (program, pname, p) => {
      if (!p) {
        GL.recordError(1281);
        return;
      }
      if (program >= GL.counter) {
        GL.recordError(1281);
        return;
      }
      program = GL.programs[program];
      if (pname == 35716) {
        var log = GLctx.getProgramInfoLog(program);
        if (log === null) log = "(unknown error)";
        SAFE_HEAP_STORE((p >> 2) * 4, log.length + 1, 4);
      } else if (pname == 35719) {
        if (!program.maxUniformLength) {
          for (
            var i = 0;
            i < GLctx.getProgramParameter(program, 35718);
            /*GL_ACTIVE_UNIFORMS*/
            ++i
          ) {
            program.maxUniformLength = Math.max(program.maxUniformLength, GLctx.getActiveUniform(program, i).name.length + 1);
          }
        }
        SAFE_HEAP_STORE((p >> 2) * 4, program.maxUniformLength, 4);
      } else if (pname == 35722) {
        if (!program.maxAttributeLength) {
          for (
            var i = 0;
            i < GLctx.getProgramParameter(program, 35721);
            /*GL_ACTIVE_ATTRIBUTES*/
            ++i
          ) {
            program.maxAttributeLength = Math.max(program.maxAttributeLength, GLctx.getActiveAttrib(program, i).name.length + 1);
          }
        }
        SAFE_HEAP_STORE((p >> 2) * 4, program.maxAttributeLength, 4);
      } else if (pname == 35381) {
        if (!program.maxUniformBlockNameLength) {
          for (
            var i = 0;
            i < GLctx.getProgramParameter(program, 35382);
            /*GL_ACTIVE_UNIFORM_BLOCKS*/
            ++i
          ) {
            program.maxUniformBlockNameLength = Math.max(program.maxUniformBlockNameLength, GLctx.getActiveUniformBlockName(program, i).length + 1);
          }
        }
        SAFE_HEAP_STORE((p >> 2) * 4, program.maxUniformBlockNameLength, 4);
      } else {
        SAFE_HEAP_STORE((p >> 2) * 4, GLctx.getProgramParameter(program, pname), 4);
      }
    };
    var _emscripten_glGetProgramiv = _glGetProgramiv;
    var _glGetShaderInfoLog = (shader, maxLength, length, infoLog) => {
      var log = GLctx.getShaderInfoLog(GL.shaders[shader]);
      if (log === null) log = "(unknown error)";
      var numBytesWrittenExclNull = maxLength > 0 && infoLog ? stringToUTF8(log, infoLog, maxLength) : 0;
      if (length) SAFE_HEAP_STORE((length >> 2) * 4, numBytesWrittenExclNull, 4);
    };
    var _emscripten_glGetShaderInfoLog = _glGetShaderInfoLog;
    var _glGetShaderPrecisionFormat = (shaderType, precisionType, range, precision) => {
      var result = GLctx.getShaderPrecisionFormat(shaderType, precisionType);
      SAFE_HEAP_STORE((range >> 2) * 4, result.rangeMin, 4);
      SAFE_HEAP_STORE((range + 4 >> 2) * 4, result.rangeMax, 4);
      SAFE_HEAP_STORE((precision >> 2) * 4, result.precision, 4);
    };
    var _emscripten_glGetShaderPrecisionFormat = _glGetShaderPrecisionFormat;
    var _glGetShaderiv = (shader, pname, p) => {
      if (!p) {
        GL.recordError(1281);
        return;
      }
      if (pname == 35716) {
        var log = GLctx.getShaderInfoLog(GL.shaders[shader]);
        if (log === null) log = "(unknown error)";
        var logLength = log ? log.length + 1 : 0;
        SAFE_HEAP_STORE((p >> 2) * 4, logLength, 4);
      } else if (pname == 35720) {
        var source = GLctx.getShaderSource(GL.shaders[shader]);
        var sourceLength = source ? source.length + 1 : 0;
        SAFE_HEAP_STORE((p >> 2) * 4, sourceLength, 4);
      } else {
        SAFE_HEAP_STORE((p >> 2) * 4, GLctx.getShaderParameter(GL.shaders[shader], pname), 4);
      }
    };
    var _emscripten_glGetShaderiv = _glGetShaderiv;
    var stringToNewUTF8 = (str) => {
      var size = lengthBytesUTF8(str) + 1;
      var ret = _malloc(size);
      if (ret) stringToUTF8(str, ret, size);
      return ret;
    };
    var _glGetString = (name_) => {
      var ret = GL.stringCache[name_];
      if (!ret) {
        switch (name_) {
          case 7939:
            ret = stringToNewUTF8(webglGetExtensions().join(" "));
            break;
          case 7936:
          /* GL_VENDOR */
          case 7937:
          /* GL_RENDERER */
          case 37445:
          /* UNMASKED_VENDOR_WEBGL */
          case 37446:
            var s = GLctx.getParameter(name_);
            if (!s) {
              GL.recordError(1280);
            }
            ret = s ? stringToNewUTF8(s) : 0;
            break;
          case 7938:
            var glVersion = GLctx.getParameter(7938);
            if (GL.currentContext.version >= 2) glVersion = `OpenGL ES 3.0 (${glVersion})`;
            else {
              glVersion = `OpenGL ES 2.0 (${glVersion})`;
            }
            ret = stringToNewUTF8(glVersion);
            break;
          case 35724:
            var glslVersion = GLctx.getParameter(35724);
            var ver_re = /^WebGL GLSL ES ([0-9]\.[0-9][0-9]?)(?:$| .*)/;
            var ver_num = glslVersion.match(ver_re);
            if (ver_num !== null) {
              if (ver_num[1].length == 3) ver_num[1] = ver_num[1] + "0";
              glslVersion = `OpenGL ES GLSL ES ${ver_num[1]} (${glslVersion})`;
            }
            ret = stringToNewUTF8(glslVersion);
            break;
          default:
            GL.recordError(1280);
        }
        GL.stringCache[name_] = ret;
      }
      return ret;
    };
    var _emscripten_glGetString = _glGetString;
    var _glGetStringi = (name, index) => {
      if (GL.currentContext.version < 2) {
        GL.recordError(1282);
        return 0;
      }
      var stringiCache = GL.stringiCache[name];
      if (stringiCache) {
        if (index < 0 || index >= stringiCache.length) {
          GL.recordError(1281);
          return 0;
        }
        return stringiCache[index];
      }
      switch (name) {
        case 7939:
          var exts = webglGetExtensions().map(stringToNewUTF8);
          stringiCache = GL.stringiCache[name] = exts;
          if (index < 0 || index >= stringiCache.length) {
            GL.recordError(1281);
            return 0;
          }
          return stringiCache[index];
        default:
          GL.recordError(1280);
          return 0;
      }
    };
    var _emscripten_glGetStringi = _glGetStringi;
    var _glGetUniformBlockIndex = (program, uniformBlockName) => GLctx.getUniformBlockIndex(GL.programs[program], UTF8ToString(uniformBlockName));
    var _emscripten_glGetUniformBlockIndex = _glGetUniformBlockIndex;
    var jstoi_q = (str) => parseInt(str);
    var webglGetLeftBracePos = (name) => name.slice(-1) == "]" && name.lastIndexOf("[");
    var webglPrepareUniformLocationsBeforeFirstUse = (program) => {
      var uniformLocsById = program.uniformLocsById, uniformSizeAndIdsByName = program.uniformSizeAndIdsByName, i, j;
      if (!uniformLocsById) {
        program.uniformLocsById = uniformLocsById = {};
        program.uniformArrayNamesById = {};
        for (
          i = 0;
          i < GLctx.getProgramParameter(program, 35718);
          /*GL_ACTIVE_UNIFORMS*/
          ++i
        ) {
          var u = GLctx.getActiveUniform(program, i);
          var nm = u.name;
          var sz = u.size;
          var lb = webglGetLeftBracePos(nm);
          var arrayName = lb > 0 ? nm.slice(0, lb) : nm;
          var id = program.uniformIdCounter;
          program.uniformIdCounter += sz;
          uniformSizeAndIdsByName[arrayName] = [sz, id];
          for (j = 0; j < sz; ++j) {
            uniformLocsById[id] = j;
            program.uniformArrayNamesById[id++] = arrayName;
          }
        }
      }
    };
    var _glGetUniformLocation = (program, name) => {
      name = UTF8ToString(name);
      if (program = GL.programs[program]) {
        webglPrepareUniformLocationsBeforeFirstUse(program);
        var uniformLocsById = program.uniformLocsById;
        var arrayIndex = 0;
        var uniformBaseName = name;
        var leftBrace = webglGetLeftBracePos(name);
        if (leftBrace > 0) {
          arrayIndex = jstoi_q(name.slice(leftBrace + 1)) >>> 0;
          uniformBaseName = name.slice(0, leftBrace);
        }
        var sizeAndId = program.uniformSizeAndIdsByName[uniformBaseName];
        if (sizeAndId && arrayIndex < sizeAndId[0]) {
          arrayIndex += sizeAndId[1];
          if (uniformLocsById[arrayIndex] = uniformLocsById[arrayIndex] || GLctx.getUniformLocation(program, name)) {
            return arrayIndex;
          }
        }
      } else {
        GL.recordError(1281);
      }
      return -1;
    };
    var _emscripten_glGetUniformLocation = _glGetUniformLocation;
    var _glLinkProgram = (program) => {
      program = GL.programs[program];
      GLctx.linkProgram(program);
      program.uniformLocsById = 0;
      program.uniformSizeAndIdsByName = {};
    };
    var _emscripten_glLinkProgram = _glLinkProgram;
    var _glPixelStorei = (pname, param) => {
      if (pname == 3317) {
        GL.unpackAlignment = param;
      }
      GLctx.pixelStorei(pname, param);
    };
    var _emscripten_glPixelStorei = _glPixelStorei;
    var computeUnpackAlignedImageSize = (width, height, sizePerPixel, alignment) => {
      function roundedToNextMultipleOf(x, y) {
        return x + y - 1 & -y;
      }
      var plainRowSize = width * sizePerPixel;
      var alignedRowSize = roundedToNextMultipleOf(plainRowSize, alignment);
      return height * alignedRowSize;
    };
    var colorChannelsInGlTextureFormat = (format) => {
      var colorChannels = {
        5: 3,
        6: 4,
        8: 2,
        29502: 3,
        29504: 4,
        26917: 2,
        26918: 2,
        29846: 3,
        29847: 4
      };
      return colorChannels[format - 6402] || 1;
    };
    var heapObjectForWebGLType = (type) => {
      type -= 5120;
      if (type == 0) return HEAP8;
      if (type == 1) return HEAPU8;
      if (type == 2) return HEAP16;
      if (type == 4) return HEAP32;
      if (type == 6) return HEAPF32;
      if (type == 5 || type == 28922 || type == 28520 || type == 30779 || type == 30782) return HEAPU32;
      return HEAPU16;
    };
    var toTypedArrayIndex = (pointer, heap) => pointer >>> 31 - Math.clz32(heap.BYTES_PER_ELEMENT);
    var emscriptenWebGLGetTexPixelData = (type, format, width, height, pixels, internalFormat) => {
      var heap = heapObjectForWebGLType(type);
      var sizePerPixel = colorChannelsInGlTextureFormat(format) * heap.BYTES_PER_ELEMENT;
      var bytes = computeUnpackAlignedImageSize(width, height, sizePerPixel, GL.unpackAlignment);
      return heap.subarray(toTypedArrayIndex(pixels, heap), toTypedArrayIndex(pixels + bytes, heap));
    };
    var _glReadPixels = (x, y, width, height, format, type, pixels) => {
      if (GL.currentContext.version >= 2) {
        if (GLctx.currentPixelPackBufferBinding) {
          GLctx.readPixels(x, y, width, height, format, type, pixels);
          return;
        }
        var heap = heapObjectForWebGLType(type);
        var target = toTypedArrayIndex(pixels, heap);
        GLctx.readPixels(x, y, width, height, format, type, heap, target);
        return;
      }
      var pixelData = emscriptenWebGLGetTexPixelData(type, format, width, height, pixels);
      if (!pixelData) {
        GL.recordError(1280);
        return;
      }
      GLctx.readPixels(x, y, width, height, format, type, pixelData);
    };
    var _emscripten_glReadPixels = _glReadPixels;
    var _glRenderbufferStorage = (x0, x1, x2, x3) => GLctx.renderbufferStorage(x0, x1, x2, x3);
    var _emscripten_glRenderbufferStorage = _glRenderbufferStorage;
    var _glRenderbufferStorageMultisample = (x0, x1, x2, x3, x4) => GLctx.renderbufferStorageMultisample(x0, x1, x2, x3, x4);
    var _emscripten_glRenderbufferStorageMultisample = _glRenderbufferStorageMultisample;
    var _glScissor = (x0, x1, x2, x3) => GLctx.scissor(x0, x1, x2, x3);
    var _emscripten_glScissor = _glScissor;
    var _glShaderSource = (shader, count, string, length) => {
      var source = GL.getSource(shader, count, string, length);
      GLctx.shaderSource(GL.shaders[shader], source);
    };
    var _emscripten_glShaderSource = _glShaderSource;
    var _glStencilFunc = (x0, x1, x2) => GLctx.stencilFunc(x0, x1, x2);
    var _emscripten_glStencilFunc = _glStencilFunc;
    var _glStencilFuncSeparate = (x0, x1, x2, x3) => GLctx.stencilFuncSeparate(x0, x1, x2, x3);
    var _emscripten_glStencilFuncSeparate = _glStencilFuncSeparate;
    var _glStencilMask = (x0) => GLctx.stencilMask(x0);
    var _emscripten_glStencilMask = _glStencilMask;
    var _glStencilMaskSeparate = (x0, x1) => GLctx.stencilMaskSeparate(x0, x1);
    var _emscripten_glStencilMaskSeparate = _glStencilMaskSeparate;
    var _glStencilOp = (x0, x1, x2) => GLctx.stencilOp(x0, x1, x2);
    var _emscripten_glStencilOp = _glStencilOp;
    var _glStencilOpSeparate = (x0, x1, x2, x3) => GLctx.stencilOpSeparate(x0, x1, x2, x3);
    var _emscripten_glStencilOpSeparate = _glStencilOpSeparate;
    var _glTexImage2D = (target, level, internalFormat, width, height, border, format, type, pixels) => {
      if (GL.currentContext.version >= 2) {
        if (GLctx.currentPixelUnpackBufferBinding) {
          GLctx.texImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
          return;
        }
        if (pixels) {
          var heap = heapObjectForWebGLType(type);
          var index = toTypedArrayIndex(pixels, heap);
          GLctx.texImage2D(target, level, internalFormat, width, height, border, format, type, heap, index);
          return;
        }
      }
      var pixelData = pixels ? emscriptenWebGLGetTexPixelData(type, format, width, height, pixels) : null;
      GLctx.texImage2D(target, level, internalFormat, width, height, border, format, type, pixelData);
    };
    var _emscripten_glTexImage2D = _glTexImage2D;
    var _glTexParameteri = (x0, x1, x2) => GLctx.texParameteri(x0, x1, x2);
    var _emscripten_glTexParameteri = _glTexParameteri;
    var _glTexSubImage2D = (target, level, xoffset, yoffset, width, height, format, type, pixels) => {
      if (GL.currentContext.version >= 2) {
        if (GLctx.currentPixelUnpackBufferBinding) {
          GLctx.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
          return;
        }
        if (pixels) {
          var heap = heapObjectForWebGLType(type);
          GLctx.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, heap, toTypedArrayIndex(pixels, heap));
          return;
        }
      }
      var pixelData = pixels ? emscriptenWebGLGetTexPixelData(type, format, width, height, pixels) : null;
      GLctx.texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixelData);
    };
    var _emscripten_glTexSubImage2D = _glTexSubImage2D;
    var webglGetUniformLocation = (location) => {
      var p = GLctx.currentProgram;
      if (p) {
        var webglLoc = p.uniformLocsById[location];
        if (typeof webglLoc == "number") {
          p.uniformLocsById[location] = webglLoc = GLctx.getUniformLocation(p, p.uniformArrayNamesById[location] + (webglLoc > 0 ? `[${webglLoc}]` : ""));
        }
        return webglLoc;
      } else {
        GL.recordError(1282);
      }
    };
    var _glUniform1i = (location, v0) => {
      GLctx.uniform1i(webglGetUniformLocation(location), v0);
    };
    var _emscripten_glUniform1i = _glUniform1i;
    var _glUniformBlockBinding = (program, uniformBlockIndex, uniformBlockBinding) => {
      program = GL.programs[program];
      GLctx.uniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
    };
    var _emscripten_glUniformBlockBinding = _glUniformBlockBinding;
    var _glUseProgram = (program) => {
      program = GL.programs[program];
      GLctx.useProgram(program);
      GLctx.currentProgram = program;
    };
    var _emscripten_glUseProgram = _glUseProgram;
    var _glVertexAttribDivisor = (index, divisor) => {
      GLctx.vertexAttribDivisor(index, divisor);
    };
    var _emscripten_glVertexAttribDivisor = _glVertexAttribDivisor;
    var _glVertexAttribIPointer = (index, size, type, stride, ptr) => {
      GLctx.vertexAttribIPointer(index, size, type, stride, ptr);
    };
    var _emscripten_glVertexAttribIPointer = _glVertexAttribIPointer;
    var _glVertexAttribPointer = (index, size, type, normalized, stride, ptr) => {
      GLctx.vertexAttribPointer(index, size, type, !!normalized, stride, ptr);
    };
    var _emscripten_glVertexAttribPointer = _glVertexAttribPointer;
    var _glViewport = (x0, x1, x2, x3) => GLctx.viewport(x0, x1, x2, x3);
    var _emscripten_glViewport = _glViewport;
    var _glWaitSync = (sync, flags, timeout_low, timeout_high) => {
      var timeout = convertI32PairToI53(timeout_low, timeout_high);
      GLctx.waitSync(GL.syncs[sync], flags, timeout);
    };
    var _emscripten_glWaitSync = _glWaitSync;
    var growMemory = (size) => {
      var b = wasmMemory.buffer;
      var pages = (size - b.byteLength + 65535) / 65536;
      try {
        wasmMemory.grow(pages);
        updateMemoryViews();
        return 1;
      } catch (e) {
        err(`growMemory: Attempted to grow heap from ${b.byteLength} bytes to ${size} bytes, but got error: ${e}`);
      }
    };
    var _emscripten_resize_heap = (requestedSize) => {
      var oldSize = HEAPU8.length;
      requestedSize >>>= 0;
      assert(requestedSize > oldSize);
      var maxHeapSize = getHeapMax();
      if (requestedSize > maxHeapSize) {
        err(`Cannot enlarge memory, requested ${requestedSize} bytes, but the limit is ${maxHeapSize} bytes!`);
        return false;
      }
      var alignUp = (x, multiple) => x + (multiple - x % multiple) % multiple;
      for (var cutDown = 1; cutDown <= 4; cutDown *= 2) {
        var overGrownHeapSize = oldSize * (1 + 0.2 / cutDown);
        overGrownHeapSize = Math.min(overGrownHeapSize, requestedSize + 100663296);
        var newSize = Math.min(maxHeapSize, alignUp(Math.max(requestedSize, overGrownHeapSize), 65536));
        var replacement = growMemory(newSize);
        if (replacement) {
          return true;
        }
      }
      err(`Failed to grow the heap from ${oldSize} bytes to ${newSize} bytes, not enough memory!`);
      return false;
    };
    var webglPowerPreferences = ["default", "low-power", "high-performance"];
    var _emscripten_webgl_do_create_context = (target, attributes) => {
      assert(attributes);
      var a = attributes >> 2;
      var powerPreference = SAFE_HEAP_LOAD((a + (24 >> 2)) * 4, 4, 0);
      var contextAttributes = {
        "alpha": !!SAFE_HEAP_LOAD((a + (0 >> 2)) * 4, 4, 0),
        "depth": !!SAFE_HEAP_LOAD((a + (4 >> 2)) * 4, 4, 0),
        "stencil": !!SAFE_HEAP_LOAD((a + (8 >> 2)) * 4, 4, 0),
        "antialias": !!SAFE_HEAP_LOAD((a + (12 >> 2)) * 4, 4, 0),
        "premultipliedAlpha": !!SAFE_HEAP_LOAD((a + (16 >> 2)) * 4, 4, 0),
        "preserveDrawingBuffer": !!SAFE_HEAP_LOAD((a + (20 >> 2)) * 4, 4, 0),
        "powerPreference": webglPowerPreferences[powerPreference],
        "failIfMajorPerformanceCaveat": !!SAFE_HEAP_LOAD((a + (28 >> 2)) * 4, 4, 0),
        majorVersion: SAFE_HEAP_LOAD((a + (32 >> 2)) * 4, 4, 0),
        minorVersion: SAFE_HEAP_LOAD((a + (36 >> 2)) * 4, 4, 0),
        enableExtensionsByDefault: SAFE_HEAP_LOAD((a + (40 >> 2)) * 4, 4, 0),
        explicitSwapControl: SAFE_HEAP_LOAD((a + (44 >> 2)) * 4, 4, 0),
        proxyContextToMainThread: SAFE_HEAP_LOAD((a + (48 >> 2)) * 4, 4, 0),
        renderViaOffscreenBackBuffer: SAFE_HEAP_LOAD((a + (52 >> 2)) * 4, 4, 0)
      };
      var canvas = findCanvasEventTarget(target);
      if (!canvas) {
        return 0;
      }
      if (contextAttributes.explicitSwapControl) {
        return 0;
      }
      var contextHandle = GL.createContext(canvas, contextAttributes);
      return contextHandle;
    };
    var _emscripten_webgl_create_context = _emscripten_webgl_do_create_context;
    var _emscripten_webgl_destroy_context = (contextHandle) => {
      if (GL.currentContext == contextHandle) GL.currentContext = 0;
      GL.deleteContext(contextHandle);
    };
    var _emscripten_webgl_do_get_current_context = () => GL.currentContext ? GL.currentContext.handle : 0;
    var _emscripten_webgl_get_current_context = _emscripten_webgl_do_get_current_context;
    var _emscripten_webgl_make_context_current = (contextHandle) => {
      var success = GL.makeContextCurrent(contextHandle);
      return success ? 0 : -5;
    };
    var ENV = {};
    var getExecutableName = () => thisProgram || "./this.program";
    var getEnvStrings = () => {
      if (!getEnvStrings.strings) {
        var lang = (typeof navigator == "object" && navigator.languages && navigator.languages[0] || "C").replace("-", "_") + ".UTF-8";
        var env = {
          "USER": "web_user",
          "LOGNAME": "web_user",
          "PATH": "/",
          "PWD": "/",
          "HOME": "/home/web_user",
          "LANG": lang,
          "_": getExecutableName()
        };
        for (var x in ENV) {
          if (ENV[x] === void 0) delete env[x];
          else env[x] = ENV[x];
        }
        var strings = [];
        for (var x in env) {
          strings.push(`${x}=${env[x]}`);
        }
        getEnvStrings.strings = strings;
      }
      return getEnvStrings.strings;
    };
    var stringToAscii = (str, buffer) => {
      for (var i = 0; i < str.length; ++i) {
        assert(str.charCodeAt(i) === (str.charCodeAt(i) & 255));
        SAFE_HEAP_STORE(buffer++, str.charCodeAt(i), 1);
      }
      SAFE_HEAP_STORE(buffer, 0, 1);
    };
    var _environ_get = (__environ, environ_buf) => {
      var bufSize = 0;
      getEnvStrings().forEach((string, i) => {
        var ptr = environ_buf + bufSize;
        SAFE_HEAP_STORE((__environ + i * 4 >> 2) * 4, ptr, 4);
        stringToAscii(string, ptr);
        bufSize += string.length + 1;
      });
      return 0;
    };
    var _environ_sizes_get = (penviron_count, penviron_buf_size) => {
      var strings = getEnvStrings();
      SAFE_HEAP_STORE((penviron_count >> 2) * 4, strings.length, 4);
      var bufSize = 0;
      strings.forEach((string) => bufSize += string.length + 1);
      SAFE_HEAP_STORE((penviron_buf_size >> 2) * 4, bufSize, 4);
      return 0;
    };
    var runtimeKeepaliveCounter = 0;
    var keepRuntimeAlive = () => noExitRuntime || runtimeKeepaliveCounter > 0;
    var _proc_exit = (code) => {
      if (!keepRuntimeAlive()) {
        Module["onExit"]?.(code);
        ABORT = true;
      }
      quit_(code, new ExitStatus(code));
    };
    var exitJS = (status, implicit) => {
      checkUnflushedContent();
      if (keepRuntimeAlive() && !implicit) {
        var msg = `program exited (with status: ${status}), but keepRuntimeAlive() is set (counter=${runtimeKeepaliveCounter}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
        readyPromiseReject(msg);
        err(msg);
      }
      _proc_exit(status);
    };
    var _exit = exitJS;
    function _fd_close(fd) {
      try {
        var stream = SYSCALLS.getStreamFromFD(fd);
        FS.close(stream);
        return 0;
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return e.errno;
      }
    }
    var doReadv = (stream, iov, iovcnt, offset) => {
      var ret = 0;
      for (var i = 0; i < iovcnt; i++) {
        var ptr = SAFE_HEAP_LOAD((iov >> 2) * 4, 4, 1);
        var len = SAFE_HEAP_LOAD((iov + 4 >> 2) * 4, 4, 1);
        iov += 8;
        var curr = FS.read(stream, HEAP8, ptr, len, offset);
        if (curr < 0) return -1;
        ret += curr;
        if (curr < len) break;
      }
      return ret;
    };
    function _fd_read(fd, iov, iovcnt, pnum) {
      try {
        var stream = SYSCALLS.getStreamFromFD(fd);
        var num = doReadv(stream, iov, iovcnt);
        SAFE_HEAP_STORE((pnum >> 2) * 4, num, 4);
        return 0;
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return e.errno;
      }
    }
    function _fd_seek(fd, offset_low, offset_high, whence, newOffset) {
      var offset = convertI32PairToI53Checked(offset_low, offset_high);
      try {
        if (isNaN(offset)) return 61;
        var stream = SYSCALLS.getStreamFromFD(fd);
        FS.llseek(stream, offset, whence);
        tempI64 = [stream.position >>> 0, (tempDouble = stream.position, +Math.abs(tempDouble) >= 1 ? tempDouble > 0 ? +Math.floor(tempDouble / 4294967296) >>> 0 : ~~+Math.ceil((tempDouble - +(~~tempDouble >>> 0)) / 4294967296) >>> 0 : 0)], SAFE_HEAP_STORE((newOffset >> 2) * 4, tempI64[0], 4), SAFE_HEAP_STORE((newOffset + 4 >> 2) * 4, tempI64[1], 4);
        if (stream.getdents && offset === 0 && whence === 0) stream.getdents = null;
        return 0;
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return e.errno;
      }
    }
    var doWritev = (stream, iov, iovcnt, offset) => {
      var ret = 0;
      for (var i = 0; i < iovcnt; i++) {
        var ptr = SAFE_HEAP_LOAD((iov >> 2) * 4, 4, 1);
        var len = SAFE_HEAP_LOAD((iov + 4 >> 2) * 4, 4, 1);
        iov += 8;
        var curr = FS.write(stream, HEAP8, ptr, len, offset);
        if (curr < 0) return -1;
        ret += curr;
      }
      return ret;
    };
    function _fd_write(fd, iov, iovcnt, pnum) {
      try {
        var stream = SYSCALLS.getStreamFromFD(fd);
        var num = doWritev(stream, iov, iovcnt);
        SAFE_HEAP_STORE((pnum >> 2) * 4, num, 4);
        return 0;
      } catch (e) {
        if (typeof FS == "undefined" || !(e.name === "ErrnoError")) throw e;
        return e.errno;
      }
    }
    var isLeapYear = (year) => year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
    var arraySum = (array, index) => {
      var sum = 0;
      for (var i = 0; i <= index; sum += array[i++]) {
      }
      return sum;
    };
    var MONTH_DAYS_LEAP = [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    var MONTH_DAYS_REGULAR = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    var addDays = (date, days) => {
      var newDate = new Date(date.getTime());
      while (days > 0) {
        var leap = isLeapYear(newDate.getFullYear());
        var currentMonth = newDate.getMonth();
        var daysInCurrentMonth = (leap ? MONTH_DAYS_LEAP : MONTH_DAYS_REGULAR)[currentMonth];
        if (days > daysInCurrentMonth - newDate.getDate()) {
          days -= daysInCurrentMonth - newDate.getDate() + 1;
          newDate.setDate(1);
          if (currentMonth < 11) {
            newDate.setMonth(currentMonth + 1);
          } else {
            newDate.setMonth(0);
            newDate.setFullYear(newDate.getFullYear() + 1);
          }
        } else {
          newDate.setDate(newDate.getDate() + days);
          return newDate;
        }
      }
      return newDate;
    };
    var writeArrayToMemory = (array, buffer) => {
      assert(array.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)");
      HEAP8.set(array, buffer);
    };
    var _strftime = (s, maxsize, format, tm) => {
      var tm_zone = SAFE_HEAP_LOAD((tm + 40 >> 2) * 4, 4, 1);
      var date = {
        tm_sec: SAFE_HEAP_LOAD((tm >> 2) * 4, 4, 0),
        tm_min: SAFE_HEAP_LOAD((tm + 4 >> 2) * 4, 4, 0),
        tm_hour: SAFE_HEAP_LOAD((tm + 8 >> 2) * 4, 4, 0),
        tm_mday: SAFE_HEAP_LOAD((tm + 12 >> 2) * 4, 4, 0),
        tm_mon: SAFE_HEAP_LOAD((tm + 16 >> 2) * 4, 4, 0),
        tm_year: SAFE_HEAP_LOAD((tm + 20 >> 2) * 4, 4, 0),
        tm_wday: SAFE_HEAP_LOAD((tm + 24 >> 2) * 4, 4, 0),
        tm_yday: SAFE_HEAP_LOAD((tm + 28 >> 2) * 4, 4, 0),
        tm_isdst: SAFE_HEAP_LOAD((tm + 32 >> 2) * 4, 4, 0),
        tm_gmtoff: SAFE_HEAP_LOAD((tm + 36 >> 2) * 4, 4, 0),
        tm_zone: tm_zone ? UTF8ToString(tm_zone) : ""
      };
      var pattern = UTF8ToString(format);
      var EXPANSION_RULES_1 = {
        "%c": "%a %b %d %H:%M:%S %Y",
        "%D": "%m/%d/%y",
        "%F": "%Y-%m-%d",
        "%h": "%b",
        "%r": "%I:%M:%S %p",
        "%R": "%H:%M",
        "%T": "%H:%M:%S",
        "%x": "%m/%d/%y",
        "%X": "%H:%M:%S",
        "%Ec": "%c",
        "%EC": "%C",
        "%Ex": "%m/%d/%y",
        "%EX": "%H:%M:%S",
        "%Ey": "%y",
        "%EY": "%Y",
        "%Od": "%d",
        "%Oe": "%e",
        "%OH": "%H",
        "%OI": "%I",
        "%Om": "%m",
        "%OM": "%M",
        "%OS": "%S",
        "%Ou": "%u",
        "%OU": "%U",
        "%OV": "%V",
        "%Ow": "%w",
        "%OW": "%W",
        "%Oy": "%y"
      };
      for (var rule in EXPANSION_RULES_1) {
        pattern = pattern.replace(new RegExp(rule, "g"), EXPANSION_RULES_1[rule]);
      }
      var WEEKDAYS = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
      var MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
      function leadingSomething(value, digits, character) {
        var str = typeof value == "number" ? value.toString() : value || "";
        while (str.length < digits) {
          str = character[0] + str;
        }
        return str;
      }
      function leadingNulls(value, digits) {
        return leadingSomething(value, digits, "0");
      }
      function compareByDay(date1, date2) {
        function sgn(value) {
          return value < 0 ? -1 : value > 0 ? 1 : 0;
        }
        var compare;
        if ((compare = sgn(date1.getFullYear() - date2.getFullYear())) === 0) {
          if ((compare = sgn(date1.getMonth() - date2.getMonth())) === 0) {
            compare = sgn(date1.getDate() - date2.getDate());
          }
        }
        return compare;
      }
      function getFirstWeekStartDate(janFourth) {
        switch (janFourth.getDay()) {
          case 0:
            return new Date(janFourth.getFullYear() - 1, 11, 29);
          case 1:
            return janFourth;
          case 2:
            return new Date(janFourth.getFullYear(), 0, 3);
          case 3:
            return new Date(janFourth.getFullYear(), 0, 2);
          case 4:
            return new Date(janFourth.getFullYear(), 0, 1);
          case 5:
            return new Date(janFourth.getFullYear() - 1, 11, 31);
          case 6:
            return new Date(janFourth.getFullYear() - 1, 11, 30);
        }
      }
      function getWeekBasedYear(date2) {
        var thisDate = addDays(new Date(date2.tm_year + 1900, 0, 1), date2.tm_yday);
        var janFourthThisYear = new Date(thisDate.getFullYear(), 0, 4);
        var janFourthNextYear = new Date(thisDate.getFullYear() + 1, 0, 4);
        var firstWeekStartThisYear = getFirstWeekStartDate(janFourthThisYear);
        var firstWeekStartNextYear = getFirstWeekStartDate(janFourthNextYear);
        if (compareByDay(firstWeekStartThisYear, thisDate) <= 0) {
          if (compareByDay(firstWeekStartNextYear, thisDate) <= 0) {
            return thisDate.getFullYear() + 1;
          }
          return thisDate.getFullYear();
        }
        return thisDate.getFullYear() - 1;
      }
      var EXPANSION_RULES_2 = {
        "%a": (date2) => WEEKDAYS[date2.tm_wday].substring(0, 3),
        "%A": (date2) => WEEKDAYS[date2.tm_wday],
        "%b": (date2) => MONTHS[date2.tm_mon].substring(0, 3),
        "%B": (date2) => MONTHS[date2.tm_mon],
        "%C": (date2) => {
          var year = date2.tm_year + 1900;
          return leadingNulls(year / 100 | 0, 2);
        },
        "%d": (date2) => leadingNulls(date2.tm_mday, 2),
        "%e": (date2) => leadingSomething(date2.tm_mday, 2, " "),
        "%g": (date2) => getWeekBasedYear(date2).toString().substring(2),
        "%G": getWeekBasedYear,
        "%H": (date2) => leadingNulls(date2.tm_hour, 2),
        "%I": (date2) => {
          var twelveHour = date2.tm_hour;
          if (twelveHour == 0) twelveHour = 12;
          else if (twelveHour > 12) twelveHour -= 12;
          return leadingNulls(twelveHour, 2);
        },
        "%j": (date2) => leadingNulls(date2.tm_mday + arraySum(isLeapYear(date2.tm_year + 1900) ? MONTH_DAYS_LEAP : MONTH_DAYS_REGULAR, date2.tm_mon - 1), 3),
        "%m": (date2) => leadingNulls(date2.tm_mon + 1, 2),
        "%M": (date2) => leadingNulls(date2.tm_min, 2),
        "%n": () => "\n",
        "%p": (date2) => {
          if (date2.tm_hour >= 0 && date2.tm_hour < 12) {
            return "AM";
          }
          return "PM";
        },
        "%S": (date2) => leadingNulls(date2.tm_sec, 2),
        "%t": () => "	",
        "%u": (date2) => date2.tm_wday || 7,
        "%U": (date2) => {
          var days = date2.tm_yday + 7 - date2.tm_wday;
          return leadingNulls(Math.floor(days / 7), 2);
        },
        "%V": (date2) => {
          var val = Math.floor((date2.tm_yday + 7 - (date2.tm_wday + 6) % 7) / 7);
          if ((date2.tm_wday + 371 - date2.tm_yday - 2) % 7 <= 2) {
            val++;
          }
          if (!val) {
            val = 52;
            var dec31 = (date2.tm_wday + 7 - date2.tm_yday - 1) % 7;
            if (dec31 == 4 || dec31 == 5 && isLeapYear(date2.tm_year % 400 - 1)) {
              val++;
            }
          } else if (val == 53) {
            var jan1 = (date2.tm_wday + 371 - date2.tm_yday) % 7;
            if (jan1 != 4 && (jan1 != 3 || !isLeapYear(date2.tm_year))) val = 1;
          }
          return leadingNulls(val, 2);
        },
        "%w": (date2) => date2.tm_wday,
        "%W": (date2) => {
          var days = date2.tm_yday + 7 - (date2.tm_wday + 6) % 7;
          return leadingNulls(Math.floor(days / 7), 2);
        },
        "%y": (date2) => (date2.tm_year + 1900).toString().substring(2),
        "%Y": (date2) => date2.tm_year + 1900,
        "%z": (date2) => {
          var off = date2.tm_gmtoff;
          var ahead = off >= 0;
          off = Math.abs(off) / 60;
          off = off / 60 * 100 + off % 60;
          return (ahead ? "+" : "-") + String("0000" + off).slice(-4);
        },
        "%Z": (date2) => date2.tm_zone,
        "%%": () => "%"
      };
      pattern = pattern.replace(/%%/g, "\0\0");
      for (var rule in EXPANSION_RULES_2) {
        if (pattern.includes(rule)) {
          pattern = pattern.replace(new RegExp(rule, "g"), EXPANSION_RULES_2[rule](date));
        }
      }
      pattern = pattern.replace(/\0\0/g, "%");
      var bytes = intArrayFromString(pattern, false);
      if (bytes.length > maxsize) {
        return 0;
      }
      writeArrayToMemory(bytes, s);
      return bytes.length - 1;
    };
    var _strftime_l = (s, maxsize, format, tm, loc) => _strftime(s, maxsize, format, tm);
    FS.createPreloadedFile = FS_createPreloadedFile;
    FS.staticInit();
    InternalError = Module["InternalError"] = class InternalError extends Error {
      constructor(message) {
        super(message);
        this.name = "InternalError";
      }
    };
    embind_init_charCodes();
    BindingError = Module["BindingError"] = class BindingError extends Error {
      constructor(message) {
        super(message);
        this.name = "BindingError";
      }
    };
    init_ClassHandle();
    init_embind();
    init_RegisteredPointer();
    UnboundTypeError = Module["UnboundTypeError"] = extendError(Error, "UnboundTypeError");
    init_emval();
    var GLctx;
    function checkIncomingModuleAPI() {
      ignoredModuleProp("fetchSettings");
    }
    var wasmImports = {
      /** @export */
      __assert_fail: ___assert_fail,
      /** @export */
      __cxa_throw: ___cxa_throw,
      /** @export */
      __syscall_fcntl64: ___syscall_fcntl64,
      /** @export */
      __syscall_fstat64: ___syscall_fstat64,
      /** @export */
      __syscall_ioctl: ___syscall_ioctl,
      /** @export */
      __syscall_lstat64: ___syscall_lstat64,
      /** @export */
      __syscall_newfstatat: ___syscall_newfstatat,
      /** @export */
      __syscall_openat: ___syscall_openat,
      /** @export */
      __syscall_stat64: ___syscall_stat64,
      /** @export */
      _embind_finalize_value_object: __embind_finalize_value_object,
      /** @export */
      _embind_register_bigint: __embind_register_bigint,
      /** @export */
      _embind_register_bool: __embind_register_bool,
      /** @export */
      _embind_register_class: __embind_register_class,
      /** @export */
      _embind_register_class_class_function: __embind_register_class_class_function,
      /** @export */
      _embind_register_class_constructor: __embind_register_class_constructor,
      /** @export */
      _embind_register_class_function: __embind_register_class_function,
      /** @export */
      _embind_register_class_property: __embind_register_class_property,
      /** @export */
      _embind_register_emval: __embind_register_emval,
      /** @export */
      _embind_register_enum: __embind_register_enum,
      /** @export */
      _embind_register_enum_value: __embind_register_enum_value,
      /** @export */
      _embind_register_float: __embind_register_float,
      /** @export */
      _embind_register_integer: __embind_register_integer,
      /** @export */
      _embind_register_memory_view: __embind_register_memory_view,
      /** @export */
      _embind_register_optional: __embind_register_optional,
      /** @export */
      _embind_register_smart_ptr: __embind_register_smart_ptr,
      /** @export */
      _embind_register_std_string: __embind_register_std_string,
      /** @export */
      _embind_register_std_wstring: __embind_register_std_wstring,
      /** @export */
      _embind_register_value_object: __embind_register_value_object,
      /** @export */
      _embind_register_value_object_field: __embind_register_value_object_field,
      /** @export */
      _embind_register_void: __embind_register_void,
      /** @export */
      _emscripten_get_now_is_monotonic: __emscripten_get_now_is_monotonic,
      /** @export */
      _emscripten_memcpy_js: __emscripten_memcpy_js,
      /** @export */
      _emscripten_throw_longjmp: __emscripten_throw_longjmp,
      /** @export */
      _emval_as: __emval_as,
      /** @export */
      _emval_call: __emval_call,
      /** @export */
      _emval_call_method: __emval_call_method,
      /** @export */
      _emval_decref: __emval_decref,
      /** @export */
      _emval_get_global: __emval_get_global,
      /** @export */
      _emval_get_method_caller: __emval_get_method_caller,
      /** @export */
      _emval_get_module_property: __emval_get_module_property,
      /** @export */
      _emval_get_property: __emval_get_property,
      /** @export */
      _emval_incref: __emval_incref,
      /** @export */
      _emval_instanceof: __emval_instanceof,
      /** @export */
      _emval_is_number: __emval_is_number,
      /** @export */
      _emval_new_cstring: __emval_new_cstring,
      /** @export */
      _emval_run_destructors: __emval_run_destructors,
      /** @export */
      _emval_take_value: __emval_take_value,
      /** @export */
      _mmap_js: __mmap_js,
      /** @export */
      _munmap_js: __munmap_js,
      /** @export */
      abort: _abort,
      /** @export */
      alignfault,
      /** @export */
      emscripten_date_now: _emscripten_date_now,
      /** @export */
      emscripten_err: _emscripten_err,
      /** @export */
      emscripten_get_canvas_element_size: _emscripten_get_canvas_element_size,
      /** @export */
      emscripten_get_heap_max: _emscripten_get_heap_max,
      /** @export */
      emscripten_get_now: _emscripten_get_now,
      /** @export */
      emscripten_glActiveTexture: _emscripten_glActiveTexture,
      /** @export */
      emscripten_glAttachShader: _emscripten_glAttachShader,
      /** @export */
      emscripten_glBindBuffer: _emscripten_glBindBuffer,
      /** @export */
      emscripten_glBindBufferRange: _emscripten_glBindBufferRange,
      /** @export */
      emscripten_glBindFramebuffer: _emscripten_glBindFramebuffer,
      /** @export */
      emscripten_glBindRenderbuffer: _emscripten_glBindRenderbuffer,
      /** @export */
      emscripten_glBindTexture: _emscripten_glBindTexture,
      /** @export */
      emscripten_glBindVertexArray: _emscripten_glBindVertexArray,
      /** @export */
      emscripten_glBindVertexArrayOES: _emscripten_glBindVertexArrayOES,
      /** @export */
      emscripten_glBlendEquation: _emscripten_glBlendEquation,
      /** @export */
      emscripten_glBlendEquationSeparate: _emscripten_glBlendEquationSeparate,
      /** @export */
      emscripten_glBlendFunc: _emscripten_glBlendFunc,
      /** @export */
      emscripten_glBlendFuncSeparate: _emscripten_glBlendFuncSeparate,
      /** @export */
      emscripten_glBlitFramebuffer: _emscripten_glBlitFramebuffer,
      /** @export */
      emscripten_glBufferData: _emscripten_glBufferData,
      /** @export */
      emscripten_glBufferSubData: _emscripten_glBufferSubData,
      /** @export */
      emscripten_glCheckFramebufferStatus: _emscripten_glCheckFramebufferStatus,
      /** @export */
      emscripten_glClear: _emscripten_glClear,
      /** @export */
      emscripten_glClearColor: _emscripten_glClearColor,
      /** @export */
      emscripten_glClearDepthf: _emscripten_glClearDepthf,
      /** @export */
      emscripten_glClearStencil: _emscripten_glClearStencil,
      /** @export */
      emscripten_glClientWaitSync: _emscripten_glClientWaitSync,
      /** @export */
      emscripten_glColorMask: _emscripten_glColorMask,
      /** @export */
      emscripten_glCompileShader: _emscripten_glCompileShader,
      /** @export */
      emscripten_glCopyTexSubImage2D: _emscripten_glCopyTexSubImage2D,
      /** @export */
      emscripten_glCreateProgram: _emscripten_glCreateProgram,
      /** @export */
      emscripten_glCreateShader: _emscripten_glCreateShader,
      /** @export */
      emscripten_glDeleteBuffers: _emscripten_glDeleteBuffers,
      /** @export */
      emscripten_glDeleteFramebuffers: _emscripten_glDeleteFramebuffers,
      /** @export */
      emscripten_glDeleteProgram: _emscripten_glDeleteProgram,
      /** @export */
      emscripten_glDeleteRenderbuffers: _emscripten_glDeleteRenderbuffers,
      /** @export */
      emscripten_glDeleteShader: _emscripten_glDeleteShader,
      /** @export */
      emscripten_glDeleteSync: _emscripten_glDeleteSync,
      /** @export */
      emscripten_glDeleteTextures: _emscripten_glDeleteTextures,
      /** @export */
      emscripten_glDeleteVertexArrays: _emscripten_glDeleteVertexArrays,
      /** @export */
      emscripten_glDeleteVertexArraysOES: _emscripten_glDeleteVertexArraysOES,
      /** @export */
      emscripten_glDepthFunc: _emscripten_glDepthFunc,
      /** @export */
      emscripten_glDepthMask: _emscripten_glDepthMask,
      /** @export */
      emscripten_glDisable: _emscripten_glDisable,
      /** @export */
      emscripten_glDrawArrays: _emscripten_glDrawArrays,
      /** @export */
      emscripten_glDrawArraysInstanced: _emscripten_glDrawArraysInstanced,
      /** @export */
      emscripten_glDrawElements: _emscripten_glDrawElements,
      /** @export */
      emscripten_glDrawElementsInstanced: _emscripten_glDrawElementsInstanced,
      /** @export */
      emscripten_glEnable: _emscripten_glEnable,
      /** @export */
      emscripten_glEnableVertexAttribArray: _emscripten_glEnableVertexAttribArray,
      /** @export */
      emscripten_glFenceSync: _emscripten_glFenceSync,
      /** @export */
      emscripten_glFinish: _emscripten_glFinish,
      /** @export */
      emscripten_glFlush: _emscripten_glFlush,
      /** @export */
      emscripten_glFramebufferRenderbuffer: _emscripten_glFramebufferRenderbuffer,
      /** @export */
      emscripten_glFramebufferTexture2D: _emscripten_glFramebufferTexture2D,
      /** @export */
      emscripten_glGenBuffers: _emscripten_glGenBuffers,
      /** @export */
      emscripten_glGenFramebuffers: _emscripten_glGenFramebuffers,
      /** @export */
      emscripten_glGenRenderbuffers: _emscripten_glGenRenderbuffers,
      /** @export */
      emscripten_glGenTextures: _emscripten_glGenTextures,
      /** @export */
      emscripten_glGenVertexArrays: _emscripten_glGenVertexArrays,
      /** @export */
      emscripten_glGenVertexArraysOES: _emscripten_glGenVertexArraysOES,
      /** @export */
      emscripten_glGenerateMipmap: _emscripten_glGenerateMipmap,
      /** @export */
      emscripten_glGetAttribLocation: _emscripten_glGetAttribLocation,
      /** @export */
      emscripten_glGetBufferSubData: _emscripten_glGetBufferSubData,
      /** @export */
      emscripten_glGetError: _emscripten_glGetError,
      /** @export */
      emscripten_glGetIntegerv: _emscripten_glGetIntegerv,
      /** @export */
      emscripten_glGetInternalformativ: _emscripten_glGetInternalformativ,
      /** @export */
      emscripten_glGetProgramInfoLog: _emscripten_glGetProgramInfoLog,
      /** @export */
      emscripten_glGetProgramiv: _emscripten_glGetProgramiv,
      /** @export */
      emscripten_glGetShaderInfoLog: _emscripten_glGetShaderInfoLog,
      /** @export */
      emscripten_glGetShaderPrecisionFormat: _emscripten_glGetShaderPrecisionFormat,
      /** @export */
      emscripten_glGetShaderiv: _emscripten_glGetShaderiv,
      /** @export */
      emscripten_glGetString: _emscripten_glGetString,
      /** @export */
      emscripten_glGetStringi: _emscripten_glGetStringi,
      /** @export */
      emscripten_glGetUniformBlockIndex: _emscripten_glGetUniformBlockIndex,
      /** @export */
      emscripten_glGetUniformLocation: _emscripten_glGetUniformLocation,
      /** @export */
      emscripten_glLinkProgram: _emscripten_glLinkProgram,
      /** @export */
      emscripten_glPixelStorei: _emscripten_glPixelStorei,
      /** @export */
      emscripten_glReadPixels: _emscripten_glReadPixels,
      /** @export */
      emscripten_glRenderbufferStorage: _emscripten_glRenderbufferStorage,
      /** @export */
      emscripten_glRenderbufferStorageMultisample: _emscripten_glRenderbufferStorageMultisample,
      /** @export */
      emscripten_glScissor: _emscripten_glScissor,
      /** @export */
      emscripten_glShaderSource: _emscripten_glShaderSource,
      /** @export */
      emscripten_glStencilFunc: _emscripten_glStencilFunc,
      /** @export */
      emscripten_glStencilFuncSeparate: _emscripten_glStencilFuncSeparate,
      /** @export */
      emscripten_glStencilMask: _emscripten_glStencilMask,
      /** @export */
      emscripten_glStencilMaskSeparate: _emscripten_glStencilMaskSeparate,
      /** @export */
      emscripten_glStencilOp: _emscripten_glStencilOp,
      /** @export */
      emscripten_glStencilOpSeparate: _emscripten_glStencilOpSeparate,
      /** @export */
      emscripten_glTexImage2D: _emscripten_glTexImage2D,
      /** @export */
      emscripten_glTexParameteri: _emscripten_glTexParameteri,
      /** @export */
      emscripten_glTexSubImage2D: _emscripten_glTexSubImage2D,
      /** @export */
      emscripten_glUniform1i: _emscripten_glUniform1i,
      /** @export */
      emscripten_glUniformBlockBinding: _emscripten_glUniformBlockBinding,
      /** @export */
      emscripten_glUseProgram: _emscripten_glUseProgram,
      /** @export */
      emscripten_glVertexAttribDivisor: _emscripten_glVertexAttribDivisor,
      /** @export */
      emscripten_glVertexAttribIPointer: _emscripten_glVertexAttribIPointer,
      /** @export */
      emscripten_glVertexAttribPointer: _emscripten_glVertexAttribPointer,
      /** @export */
      emscripten_glViewport: _emscripten_glViewport,
      /** @export */
      emscripten_glWaitSync: _emscripten_glWaitSync,
      /** @export */
      emscripten_resize_heap: _emscripten_resize_heap,
      /** @export */
      emscripten_webgl_create_context: _emscripten_webgl_create_context,
      /** @export */
      emscripten_webgl_destroy_context: _emscripten_webgl_destroy_context,
      /** @export */
      emscripten_webgl_get_current_context: _emscripten_webgl_get_current_context,
      /** @export */
      emscripten_webgl_make_context_current: _emscripten_webgl_make_context_current,
      /** @export */
      environ_get: _environ_get,
      /** @export */
      environ_sizes_get: _environ_sizes_get,
      /** @export */
      exit: _exit,
      /** @export */
      fd_close: _fd_close,
      /** @export */
      fd_read: _fd_read,
      /** @export */
      fd_seek: _fd_seek,
      /** @export */
      fd_write: _fd_write,
      /** @export */
      invoke_i,
      /** @export */
      invoke_ii,
      /** @export */
      invoke_iii,
      /** @export */
      invoke_iiii,
      /** @export */
      invoke_iiiii,
      /** @export */
      invoke_iiiiii,
      /** @export */
      invoke_iiiiiii,
      /** @export */
      invoke_iiiiiiii,
      /** @export */
      invoke_iiiiiiiiii,
      /** @export */
      invoke_v,
      /** @export */
      invoke_vi,
      /** @export */
      invoke_vii,
      /** @export */
      invoke_viif,
      /** @export */
      invoke_viii,
      /** @export */
      invoke_viiii,
      /** @export */
      invoke_viiiiiii,
      /** @export */
      invoke_viiiiiiii,
      /** @export */
      segfault,
      /** @export */
      strftime_l: _strftime_l
    };
    var wasmExports = createWasm();
    var ___getTypeName = createExportWrapper("__getTypeName", 1);
    var _malloc = createExportWrapper("malloc", 1);
    var _free = createExportWrapper("free", 1);
    var _fflush = createExportWrapper("fflush", 1);
    var _emscripten_builtin_memalign = createExportWrapper("emscripten_builtin_memalign", 2);
    var _sbrk = createExportWrapper("sbrk", 1);
    var _setThrew = createExportWrapper("setThrew", 2);
    var _emscripten_stack_init = () => (_emscripten_stack_init = wasmExports["emscripten_stack_init"])();
    var _emscripten_stack_get_base = () => (_emscripten_stack_get_base = wasmExports["emscripten_stack_get_base"])();
    var _emscripten_stack_get_end = () => (_emscripten_stack_get_end = wasmExports["emscripten_stack_get_end"])();
    var __emscripten_stack_restore = (a0) => (__emscripten_stack_restore = wasmExports["_emscripten_stack_restore"])(a0);
    var _emscripten_stack_get_current = () => (_emscripten_stack_get_current = wasmExports["emscripten_stack_get_current"])();
    var ___cxa_is_pointer_type = createExportWrapper("__cxa_is_pointer_type", 1);
    Module["dynCall_iij"] = createExportWrapper("dynCall_iij", 4);
    Module["dynCall_vij"] = createExportWrapper("dynCall_vij", 4);
    Module["dynCall_viij"] = createExportWrapper("dynCall_viij", 5);
    Module["dynCall_viijf"] = createExportWrapper("dynCall_viijf", 6);
    Module["dynCall_ji"] = createExportWrapper("dynCall_ji", 2);
    Module["dynCall_jiji"] = createExportWrapper("dynCall_jiji", 5);
    Module["dynCall_viijii"] = createExportWrapper("dynCall_viijii", 7);
    Module["dynCall_iiiiij"] = createExportWrapper("dynCall_iiiiij", 7);
    Module["dynCall_iiiiijj"] = createExportWrapper("dynCall_iiiiijj", 9);
    Module["dynCall_iiiiiijj"] = createExportWrapper("dynCall_iiiiiijj", 10);
    function invoke_iii(index, a1, a2) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)(a1, a2);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_ii(index, a1) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)(a1);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_viii(index, a1, a2, a3) {
      var sp = stackSave();
      try {
        getWasmTableEntry(index)(a1, a2, a3);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_vii(index, a1, a2) {
      var sp = stackSave();
      try {
        getWasmTableEntry(index)(a1, a2);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_i(index) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)();
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_vi(index, a1) {
      var sp = stackSave();
      try {
        getWasmTableEntry(index)(a1);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_iiiiiiii(index, a1, a2, a3, a4, a5, a6, a7) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)(a1, a2, a3, a4, a5, a6, a7);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_viif(index, a1, a2, a3) {
      var sp = stackSave();
      try {
        getWasmTableEntry(index)(a1, a2, a3);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_v(index) {
      var sp = stackSave();
      try {
        getWasmTableEntry(index)();
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_viiiiiii(index, a1, a2, a3, a4, a5, a6, a7) {
      var sp = stackSave();
      try {
        getWasmTableEntry(index)(a1, a2, a3, a4, a5, a6, a7);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_iiiiiii(index, a1, a2, a3, a4, a5, a6) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)(a1, a2, a3, a4, a5, a6);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_iiii(index, a1, a2, a3) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)(a1, a2, a3);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_iiiiii(index, a1, a2, a3, a4, a5) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)(a1, a2, a3, a4, a5);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_viiiiiiii(index, a1, a2, a3, a4, a5, a6, a7, a8) {
      var sp = stackSave();
      try {
        getWasmTableEntry(index)(a1, a2, a3, a4, a5, a6, a7, a8);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_iiiii(index, a1, a2, a3, a4) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)(a1, a2, a3, a4);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_viiii(index, a1, a2, a3, a4) {
      var sp = stackSave();
      try {
        getWasmTableEntry(index)(a1, a2, a3, a4);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    function invoke_iiiiiiiiii(index, a1, a2, a3, a4, a5, a6, a7, a8, a9) {
      var sp = stackSave();
      try {
        return getWasmTableEntry(index)(a1, a2, a3, a4, a5, a6, a7, a8, a9);
      } catch (e) {
        stackRestore(sp);
        if (e !== e + 0) throw e;
        _setThrew(1, 0);
      }
    }
    Module["GL"] = GL;
    var missingLibrarySymbols = ["writeI53ToI64Clamped", "writeI53ToI64Signaling", "writeI53ToU64Clamped", "writeI53ToU64Signaling", "convertU32PairToI53", "stackAlloc", "getTempRet0", "setTempRet0", "ydayFromDate", "inetPton4", "inetNtop4", "inetPton6", "inetNtop6", "readSockaddr", "writeSockaddr", "emscriptenLog", "readEmAsmArgs", "listenOnce", "autoResumeAudioContext", "handleException", "runtimeKeepalivePush", "runtimeKeepalivePop", "callUserCallback", "maybeExit", "asmjsMangle", "HandleAllocator", "getNativeTypeSize", "STACK_SIZE", "STACK_ALIGN", "POINTER_SIZE", "ASSERTIONS", "getCFunc", "ccall", "cwrap", "uleb128Encode", "sigToWasmTypes", "generateFuncType", "convertJsFunctionToWasm", "getEmptyTableSlot", "updateTableMap", "getFunctionAddress", "addFunction", "removeFunction", "reallyNegative", "strLen", "reSign", "formatString", "intArrayToString", "AsciiToString", "stringToUTF8OnStack", "registerKeyEventCallback", "getBoundingClientRect", "fillMouseEventData", "registerMouseEventCallback", "registerWheelEventCallback", "registerUiEventCallback", "registerFocusEventCallback", "fillDeviceOrientationEventData", "registerDeviceOrientationEventCallback", "fillDeviceMotionEventData", "registerDeviceMotionEventCallback", "screenOrientation", "fillOrientationChangeEventData", "registerOrientationChangeEventCallback", "fillFullscreenChangeEventData", "registerFullscreenChangeEventCallback", "JSEvents_requestFullscreen", "JSEvents_resizeCanvasForFullscreen", "registerRestoreOldStyle", "hideEverythingExceptGivenElement", "restoreHiddenElements", "setLetterbox", "softFullscreenResizeWebGLRenderTarget", "doRequestFullscreen", "fillPointerlockChangeEventData", "registerPointerlockChangeEventCallback", "registerPointerlockErrorEventCallback", "requestPointerLock", "fillVisibilityChangeEventData", "registerVisibilityChangeEventCallback", "registerTouchEventCallback", "fillGamepadEventData", "registerGamepadEventCallback", "registerBeforeUnloadEventCallback", "fillBatteryEventData", "battery", "registerBatteryEventCallback", "setCanvasElementSize", "getCanvasElementSize", "jsStackTrace", "getCallstack", "convertPCtoSourceLocation", "checkWasiClock", "wasiRightsToMuslOFlags", "wasiOFlagsToMuslOFlags", "createDyncallWrapper", "safeSetTimeout", "setImmediateWrapped", "clearImmediateWrapped", "polyfillSetImmediate", "getPromise", "makePromise", "idsToPromises", "makePromiseCallback", "findMatchingCatch", "Browser_asyncPrepareDataCounter", "setMainLoop", "getSocketFromFD", "getSocketAddress", "FS_unlink", "FS_mkdirTree", "_setNetworkCallback", "emscriptenWebGLGetUniform", "emscriptenWebGLGetVertexAttrib", "__glGetActiveAttribOrUniform", "writeGLArray", "registerWebGlEventCallback", "runAndAbortIfError", "emscriptenWebGLGetIndexed", "ALLOC_NORMAL", "ALLOC_STACK", "allocate", "writeStringToMemory", "writeAsciiToMemory", "setErrNo", "demangle", "stackTrace", "getFunctionArgsName", "createJsInvokerSignature", "registerInheritedInstance", "unregisterInheritedInstance"];
    missingLibrarySymbols.forEach(missingLibrarySymbol);
    var unexportedSymbols = ["run", "addOnPreRun", "addOnInit", "addOnPreMain", "addOnExit", "addOnPostRun", "addRunDependency", "removeRunDependency", "FS_createFolder", "FS_createPath", "FS_createLazyFile", "FS_createLink", "FS_createDevice", "FS_readFile", "out", "err", "callMain", "abort", "wasmMemory", "wasmExports", "writeStackCookie", "checkStackCookie", "writeI53ToI64", "readI53FromI64", "readI53FromU64", "convertI32PairToI53", "convertI32PairToI53Checked", "stackSave", "stackRestore", "ptrToString", "zeroMemory", "exitJS", "getHeapMax", "growMemory", "ENV", "MONTH_DAYS_REGULAR", "MONTH_DAYS_LEAP", "MONTH_DAYS_REGULAR_CUMULATIVE", "MONTH_DAYS_LEAP_CUMULATIVE", "isLeapYear", "arraySum", "addDays", "ERRNO_CODES", "ERRNO_MESSAGES", "DNS", "Protocols", "Sockets", "initRandomFill", "randomFill", "timers", "warnOnce", "readEmAsmArgsArray", "jstoi_q", "jstoi_s", "getExecutableName", "dynCallLegacy", "getDynCaller", "dynCall", "keepRuntimeAlive", "asyncLoad", "alignMemory", "mmapAlloc", "wasmTable", "noExitRuntime", "freeTableIndexes", "functionsInTableMap", "unSign", "setValue", "getValue", "PATH", "PATH_FS", "UTF8Decoder", "UTF8ArrayToString", "UTF8ToString", "stringToUTF8Array", "stringToUTF8", "lengthBytesUTF8", "intArrayFromString", "stringToAscii", "UTF16Decoder", "UTF16ToString", "stringToUTF16", "lengthBytesUTF16", "UTF32ToString", "stringToUTF32", "lengthBytesUTF32", "stringToNewUTF8", "writeArrayToMemory", "JSEvents", "specialHTMLTargets", "maybeCStringToJsString", "findEventTarget", "findCanvasEventTarget", "currentFullscreenStrategy", "restoreOldWindowedStyle", "UNWIND_CACHE", "ExitStatus", "getEnvStrings", "doReadv", "doWritev", "promiseMap", "uncaughtExceptionCount", "exceptionLast", "exceptionCaught", "ExceptionInfo", "Browser", "getPreloadedImageData__data", "wget", "SYSCALLS", "preloadPlugins", "FS_createPreloadedFile", "FS_modeStringToFlags", "FS_getMode", "FS_stdin_getChar_buffer", "FS_stdin_getChar", "FS", "FS_createDataFile", "MEMFS", "TTY", "PIPEFS", "SOCKFS", "tempFixedLengthArray", "miniTempWebGLFloatBuffers", "miniTempWebGLIntBuffers", "heapObjectForWebGLType", "toTypedArrayIndex", "webgl_enable_ANGLE_instanced_arrays", "webgl_enable_OES_vertex_array_object", "webgl_enable_WEBGL_draw_buffers", "webgl_enable_WEBGL_multi_draw", "emscriptenWebGLGet", "computeUnpackAlignedImageSize", "colorChannelsInGlTextureFormat", "emscriptenWebGLGetTexPixelData", "webglGetUniformLocation", "webglPrepareUniformLocationsBeforeFirstUse", "webglGetLeftBracePos", "AL", "GLUT", "EGL", "GLEW", "IDBStore", "SDL", "SDL_gfx", "webgl_enable_WEBGL_draw_instanced_base_vertex_base_instance", "webgl_enable_WEBGL_multi_draw_instanced_base_vertex_base_instance", "allocateUTF8", "allocateUTF8OnStack", "InternalError", "BindingError", "throwInternalError", "throwBindingError", "registeredTypes", "awaitingDependencies", "typeDependencies", "tupleRegistrations", "structRegistrations", "sharedRegisterType", "whenDependentTypesAreResolved", "embind_charCodes", "embind_init_charCodes", "readLatin1String", "getTypeName", "getFunctionName", "heap32VectorToArray", "requireRegisteredType", "usesDestructorStack", "createJsInvoker", "UnboundTypeError", "PureVirtualError", "GenericWireTypeSize", "EmValType", "init_embind", "throwUnboundTypeError", "ensureOverloadTable", "exposePublicSymbol", "replacePublicSymbol", "extendError", "createNamedFunction", "embindRepr", "registeredInstances", "getBasestPointer", "getInheritedInstance", "getInheritedInstanceCount", "getLiveInheritedInstances", "registeredPointers", "registerType", "integerReadValueFromPointer", "enumReadValueFromPointer", "floatReadValueFromPointer", "readPointer", "runDestructors", "newFunc", "craftInvokerFunction", "embind__requireFunction", "genericPointerToWireType", "constNoSmartPtrRawPointerToWireType", "nonConstNoSmartPtrRawPointerToWireType", "init_RegisteredPointer", "RegisteredPointer", "RegisteredPointer_fromWireType", "runDestructor", "releaseClassHandle", "finalizationRegistry", "detachFinalizer_deps", "detachFinalizer", "attachFinalizer", "makeClassHandle", "init_ClassHandle", "ClassHandle", "throwInstanceAlreadyDeleted", "deletionQueue", "flushPendingDeletes", "delayFunction", "setDelayFunction", "RegisteredClass", "shallowCopyInternalPointer", "downcastPointer", "upcastPointer", "validateThis", "char_0", "char_9", "makeLegalFunctionName", "emval_freelist", "emval_handles", "emval_symbols", "init_emval", "count_emval_handles", "getStringOrSymbol", "Emval", "emval_get_global", "emval_returnValue", "emval_lookupTypes", "emval_methodCallers", "emval_addMethodCaller", "reflectConstruct"];
    unexportedSymbols.forEach(unexportedRuntimeSymbol);
    var calledRun;
    dependenciesFulfilled = function runCaller() {
      if (!calledRun) run();
      if (!calledRun) dependenciesFulfilled = runCaller;
    };
    function stackCheckInit() {
      _emscripten_stack_init();
      writeStackCookie();
    }
    function run() {
      if (runDependencies > 0) {
        return;
      }
      stackCheckInit();
      preRun();
      if (runDependencies > 0) {
        return;
      }
      function doRun() {
        if (calledRun) return;
        calledRun = true;
        Module["calledRun"] = true;
        if (ABORT) return;
        initRuntime();
        readyPromiseResolve(Module);
        if (Module["onRuntimeInitialized"]) Module["onRuntimeInitialized"]();
        assert(!Module["_main"], 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]');
        postRun();
      }
      if (Module["setStatus"]) {
        Module["setStatus"]("Running...");
        setTimeout(function() {
          setTimeout(function() {
            Module["setStatus"]("");
          }, 1);
          doRun();
        }, 1);
      } else {
        doRun();
      }
      checkStackCookie();
    }
    function checkUnflushedContent() {
      var oldOut = out;
      var oldErr = err;
      var has = false;
      out = err = (x) => {
        has = true;
      };
      try {
        _fflush(0);
        ["stdout", "stderr"].forEach(function(name) {
          var info = FS.analyzePath("/dev/" + name);
          if (!info) return;
          var stream = info.object;
          var rdev = stream.rdev;
          var tty = TTY.ttys[rdev];
          if (tty?.output?.length) {
            has = true;
          }
        });
      } catch (e) {
      }
      out = oldOut;
      err = oldErr;
      if (has) {
        warnOnce("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
      }
    }
    if (Module["preInit"]) {
      if (typeof Module["preInit"] == "function") Module["preInit"] = [Module["preInit"]];
      while (Module["preInit"].length > 0) {
        Module["preInit"].pop()();
      }
    }
    run();
    moduleRtn = readyPromise;
    for (const prop of Object.keys(Module)) {
      if (!(prop in moduleArg)) {
        Object.defineProperty(moduleArg, prop, {
          configurable: true,
          get() {
            abort(`Access to module property ('${prop}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
          }
        });
      }
    }
    return moduleRtn;
  });
})();

function PAGXInit(moduleOption = {}) {
  return PAGXViewerWasm(moduleOption).then((module) => {
    PAGXBind(module);
    return module;
  }).catch((error) => {
    console.error("PAGXInit failed:", error);
    const message = error instanceof Error ? error.message : String(error);
    throw new Error(`PAGXInit failed: ${message}`);
  });
}

export { PAGXInit, PAGXView };
//# sourceMappingURL=pagx-viewer.st.esm.js.map
