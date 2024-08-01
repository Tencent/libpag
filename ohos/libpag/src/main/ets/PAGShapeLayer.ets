import { JsPAGLayer, JsPAGShapeLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGShapeLayer extends PAGLayer {
  constructor(nativeLayer: JsPAGLayer) {
    super(nativeLayer);
  }
}