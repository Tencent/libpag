import { MP4_CACHE_PATH } from './constant';
import { clearDirectory } from './file-utils';
import { PAGView } from './pag-view';
import * as types from '../types';

const clearCache = () => {
  return clearDirectory(MP4_CACHE_PATH);
};

export { PAGView, types, clearCache };
