import { PAG } from './types';
import { setPAGModule } from './pag-module';
import { PAGFile } from './pag-file';
import { PAGSurface } from './pag-surface';
import { PAGPlayer } from './pag-player';
import { PAGImage } from './pag-image';
import { PAGView } from './pag-view';
import { PAGFont } from './pag-font';
import { PAGLayer } from './pag-layer';
import { VideoReader } from './core/video-reader';
import { GlobalCanvas } from './core/global-canvas';
import { BackendContext } from './core/backend-context';
import { PAGComposition } from './pag-composition';
import { PAGTextLayer } from './pag-text-layer';
import { PAGImageLayer } from './pag-image-layer';
import { PAGSolidLayer } from './pag-solid-layer';
import { Matrix } from './core/matrix';
import { RenderCanvas } from './core/render-canvas';
import { setMixin } from './utils/mixin';
import { TGFXBind } from '@tgfx/binding';

/**
 * Binding pag js module on pag webassembly module.
 */
export const PAGBind = (module: PAG) => {
  TGFXBind(module);
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
  module.GlobalCanvas = GlobalCanvas;
  module.BackendContext = BackendContext;
  module.Matrix = Matrix;
  module.RenderCanvas = RenderCanvas;
  module._useSoftwareDecoder = false;
  setMixin(module);
};
