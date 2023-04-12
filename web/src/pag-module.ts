import type { PAG } from './types';

export let PAGModule: PAG;

export const setPAGModule = (module: PAG) => {
  PAGModule = module;
};

export const getPAGModule = () => PAGModule;
