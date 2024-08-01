import { JPAGLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGImageLayer extends PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }
}