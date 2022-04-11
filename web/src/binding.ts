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
import { PAGTextLayer } from './pag-text-layer';
import { GlobalCanvas } from './core/global-canvas';

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
  module.PAGSurface = PAGSurface;
  PAGSurface.module = module;
  module.PAGTextLayer = PAGTextLayer;
  PAGTextLayer.module = module;
  module.VideoReader = VideoReader;
  module.NativeImage = NativeImage;
  module.ScalerContext = ScalerContext;
  module.WebMask = WebMask;
  WebMask.module = module;
  module.GlobalCanvas = GlobalCanvas;
  GlobalCanvas.module = module;
  module.traceImage = function (info, pixels) {
    const canvas = document.createElement('canvas');
    canvas.width = info.width;
    canvas.height = info.height;
    const context = canvas.getContext('2d') as CanvasRenderingContext2D;
    const imageData = new ImageData(new Uint8ClampedArray(pixels), canvas.width, canvas.height);
    context.putImageData(imageData, 0, 0);
    document.body.appendChild(canvas);
  };
};
