import { PAGX } from './types';
import { View } from './pagx-view';
import { TGFXBind } from '@tgfx/wechat/binding';
import type { EmscriptenGL, WindowColorSpace } from '@tgfx/types';

/**
 * Binding pag js module on pag webassembly module.
 */
export const binding = (module: PAGX) => {
  TGFXBind(module);
  module.View = View;
};
