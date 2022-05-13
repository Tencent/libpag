import { PAG } from './types';
import { PAGFile } from './pag-file';
import { PAGSurface } from './pag-surface';
import { PAGPlayer } from './pag-player';
import { PAGImage } from './pag-image';
import { PAGView } from './pag-view';
import { PAGFont } from './pag-font';
import { PAGLayer } from './pag-layer';
import { VideoReader } from './core/video-reader';
import { ScalerContext } from './core/scaler-context';
import { WebMask } from './core/web-mask';
import { NativeImage } from './core/native-image';
import { GlobalCanvas } from './core/global-canvas';
import { PAGGlContext } from './core/pag-gl-context';
import { PAGGlUnit } from './core/pag-gl-unit';
import { PAGComposition } from './pag-composition';
import { PAGTextLayer } from './pag-text-layer';
import { PAGGlTexture } from './core/pag-gl-texture';
import { PAGGlFrameBuffer } from './core/pag-gl-framebuffer';

const interactiveBindingClasses = [
  PAGFile,
  PAGPlayer,
  PAGView,
  PAGFont,
  PAGImage,
  PAGLayer,
  PAGSurface,
  WebMask,
  GlobalCanvas,
  PAGGlUnit,
  PAGGlContext,
];

/**
 * Binding pag js module on pag webassembly module.
 */
export const binding = (module: PAG) => {
  module.PAG = module;
  module.PAGComposition = PAGComposition;
  module.PAGGlFrameBuffer = PAGGlFrameBuffer;
  module.PAGGlTexture = PAGGlTexture;
  module.PAGTextLayer = PAGTextLayer;
  module.VideoReader = VideoReader;
  module.NativeImage = NativeImage;
  module.ScalerContext = ScalerContext;
  interactiveBindingClasses.forEach((item) => {
    module[item.name] = item;
    item.module = module;
  });
  module.traceImage = function (info, pixels) {
    const canvas = document.createElement('canvas');
    canvas.width = info.width;
    canvas.height = info.height;
    const context = canvas.getContext('2d') as CanvasRenderingContext2D;
    const imageData = new ImageData(new Uint8ClampedArray(pixels), canvas.width, canvas.height);
    context.putImageData(imageData, 0, 0);
    document.body.appendChild(canvas);
  };
  module.registerSoftwareDecoderFactory = function (factory) {
    module._registerSoftwareDecoderFactory(factory);
  };
};
