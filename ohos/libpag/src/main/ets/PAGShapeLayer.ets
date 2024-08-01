import { JsPAGLayer, JsPAGShapeLayer } from 'liblibpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGShapeLayer extends PAGLayer {
  constructor(nativeLayer: JsPAGLayer) {
    super(nativeLayer);
  }
}