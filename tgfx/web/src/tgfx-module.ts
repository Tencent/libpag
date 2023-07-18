import type { TGFX } from './types';

export let TGFXModule: TGFX;

export const setTGFXModule = (module: TGFX) => {
  TGFXModule = module;
};

export const getTGFXModule = () => TGFXModule;
