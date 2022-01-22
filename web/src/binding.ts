import { PAG } from './types';
import { PAGFile } from './pag-file';
import { PAGSurface } from './pag-surface';
import { PAGPlayer } from './pag-player';
import { PAGImage } from './pag-image';
import { PAGView } from './pag-view';
import { PAGFont } from './pag-font';
import { PAGLayer } from './pag-layer';
import { PAGComposition } from './pag-composition';
import { VideoReader } from './core/video-reader';
import { ScalerContext } from './core/scaler-context';
import { WebMask } from './core/web-mask';
import { NativeImage } from './core/native-image';

/**
 * Binding pag js module on pag webassembly module.
 */
export const binding = (module: PAG) => {
  module.PAGFile = PAGFile;
  PAGFile.module = module;
  module.PAGPlayer = PAGPlayer;
  PAGPlayer.module = module;
  module.PAGView = PAGView;
  PAGView.module = module;
  module.PAGFont = PAGFont;
  PAGFont.module = module;
  module.PAGImage = PAGImage;
  PAGImage.module = module;
  module.PAGLayer = PAGLayer;
  PAGLayer.module = module;
  module.PAGComposition = PAGComposition;
  PAGComposition.module = module;
  module.VideoReader = VideoReader;
  module.NativeImage = NativeImage;
  module.ScalerContext = ScalerContext;
  module.WebMask = WebMask;
  WebMask.module = module;
  module.registerFontPath = function (path) {
    this._RegisterFontPath(path);
  };
  module.setFallbackFontNames = function (fontNames) {
    this._SetFallbackFontNames(fontNames);
  };
  module._PAGFile.Load = async function (bytes, length): Promise<PAGFile> {
    const wasmIns = await module.webAssemblyQueue.exec(this._Load, this, bytes, length);
    return new PAGFile(wasmIns);
  };
  module._PAGSurface.FromCanvas = async function (canvasID): Promise<PAGSurface> {
    const pagSurfaceWasm = await module.webAssemblyQueue.exec(this._FromCanvas, this, canvasID);
    return new PAGSurface(module, pagSurfaceWasm);
  };
  module._PAGSurface.FromTexture = async function (textureID, width, height, flipY): Promise<PAGSurface> {
    const pagSurfaceWasm = await module.webAssemblyQueue.exec(this._FromTexture, this, textureID, width, height, flipY);
    return new PAGSurface(module, pagSurfaceWasm);
  };
  module._PAGSurface.FromFrameBuffer = async function (frameBufferID, width, height, flipY): Promise<PAGSurface> {
    const pagSurfaceWasm = await module.webAssemblyQueue.exec(
      this._FromFrameBuffer,
      this,
      frameBufferID,
      width,
      height,
      flipY,
    );
    return new PAGSurface(module, pagSurfaceWasm);
  };
  module._PAGImage.FromBytes = async function (bytes, length): Promise<PAGImage> {
    const pagImageWasm = await module.webAssemblyQueue.exec(this._FromBytes, this, bytes, length);
    return new PAGImage(pagImageWasm);
  };
  module._PAGImage.FromNativeImage = async function (nativeImage): Promise<PAGImage> {
    const pagImageWasm = await module.webAssemblyQueue.exec(this._FromNativeImage, this, nativeImage);
    return new PAGImage(pagImageWasm);
  };
  module._PAGPlayer.create = async function () {
    const pagPlayerWasm = await module.webAssemblyQueue.exec(async () => await new module._PAGPlayer(), this);
    return new PAGPlayer(pagPlayerWasm);
  };
  module.traceImage = function (info, pixels) {
    const canvas = document.createElement('canvas');
    canvas.width = info.width;
    canvas.height = info.height;
    const context = canvas.getContext('2d');
    const imageData = new ImageData(new Uint8ClampedArray(pixels), canvas.width, canvas.height);
    context.putImageData(imageData, 0, 0);
    document.body.appendChild(canvas);
  };
};
