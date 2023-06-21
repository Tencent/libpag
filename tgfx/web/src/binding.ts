import { TGFX } from './types'
import { setTGFXModule } from './tgfx-module'
import * as tgfx from './tgfx'
import { Matrix } from './core/matrix'
import { ScalerContext } from './core/scaler-context';
import { WebMask } from './core/web-mask';

export const binding = (module: TGFX) => {
  setTGFXModule(module)
  module.module = module
  module.ScalerContext = ScalerContext
  module.WebMask = WebMask
  module.Matrix = Matrix
  module.tgfx = { ...tgfx };
}
