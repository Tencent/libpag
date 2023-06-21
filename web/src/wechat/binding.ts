import { VideoReader } from './video-reader';
import { ScalerContext } from './scaler-context';
import { PAGView } from './pag-view';
import { PAGFile } from './pag-file';
import { PAGImage } from './pag-image';
import { PAGFont } from './pag-font';
import * as tgfx from './tgfx';

import { PAG } from '../types';
import { setPAGModule } from '../pag-module';
import { PAGSurface } from '../pag-surface';
import { PAGPlayer } from '../pag-player';
import { PAGLayer } from '../pag-layer';
import { GlobalCanvas } from '../core/global-canvas';
import { BackendContext } from '../core/backend-context';
import { Matrix  } from '../core/matrix';
import { WebMask } from '../core/web-mask';
import { PAGComposition } from '../pag-composition';
import { PAGTextLayer } from '../pag-text-layer';
import { PAGImageLayer } from '../pag-image-layer';
import { PAGSolidLayer } from '../pag-solid-layer';
import { RenderCanvas } from '../core/render-canvas';
import { setMixin } from '../utils/mixin';

/**
 * Binding pag js module on pag webassembly module.
 */
export const binding = (module: PAG) => {
  setPAGModule(module);
  module.module = module;
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
  module.ScalerContext = ScalerContext;
  module.WebMask = WebMask;
  module.GlobalCanvas = GlobalCanvas;
  module.BackendContext = BackendContext;
  module.Matrix = Matrix;
  module.RenderCanvas = RenderCanvas;
  setMixin(module);
  module.currentPlayer = null;
  module.tgfx = { ...tgfx };
};
