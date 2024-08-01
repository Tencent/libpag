import { JPAGLayer, JPAGSolidLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGSolidLayer extends PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }
}