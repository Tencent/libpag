import { JPAGLayer, JPAGShapeLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGShapeLayer extends PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }
}