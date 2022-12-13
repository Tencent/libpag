import { VideoReader } from './video-reader';
import { ScalerContext } from './scaler-context';
import { WebMask } from './web-mask';
import { PAGView } from './pag-view';
import { PAGFile } from './pag-file';
import { PAGImage } from './pag-image';
import { PAGFont } from './pag-font';
import { NativeImage } from './native-image';

import { PAG } from '../types';
import { setPAGModule } from '../pag-module';
import { PAGSurface } from '../pag-surface';
import { PAGPlayer } from '../pag-player';
import { PAGLayer } from '../pag-layer';
import { GlobalCanvas } from '../core/global-canvas';
import { BackendContext } from '../core/backend-context';
import { PAGComposition } from '../pag-composition';
import { PAGTextLayer } from '../pag-text-layer';
import { PAGImageLayer } from '../pag-image-layer';
import { PAGSolidLayer } from '../pag-solid-layer';
import { Matrix } from '../core/matrix';
import { RenderCanvas } from '../core/render-canvas';

/**
 * Binding pag js module on pag webassembly module.
 */
export const binding = (module: PAG) => {
  setPAGModule(module);
  module.PAG = module;
  module.PAGFile = PAGFile;
  module.PAGPlayer = PAGPlayer;
  module.PAGView = PAGView;
  module.PAGFont = PAGFont;
  module.PAGImage = PAGImage;
  module.PAGLayer = PAGLayer;
  module.PAGComposition = PAGComposition;
  module.PAGSurface = PAGSurface;
  module.PAGTextLayer = PAGTextLayer;
  module.PAGImageLayer = PAGImageLayer;
  module.PAGSolidLayer = PAGSolidLayer;
  module.VideoReader = VideoReader;
  module.NativeImage = NativeImage;
  module.ScalerContext = ScalerContext;
  module.WebMask = WebMask;
  module.GlobalCanvas = GlobalCanvas;
  module.BackendContext = BackendContext;
  module.Matrix = Matrix;
  module.RenderCanvas = RenderCanvas;
  module.traceImage = function (info, pixels) {
    const canvas = document.createElement('canvas');
    canvas.width = info.width;
    canvas.height = info.height;
    const context = canvas.getContext('2d') as CanvasRenderingContext2D;
    const imageData = new ImageData(new Uint8ClampedArray(pixels), canvas.width, canvas.height);
    context.putImageData(imageData, 0, 0);
    document.body.appendChild(canvas);
  };
  module.registerSoftwareDecoderFactory = function (factory = null) {
    module._registerSoftwareDecoderFactory(factory);
  };
  module.SDKVersion = function () {
    return module._SDKVersion();
  };
  module.currentPlayer = null;
};
