import { JsPAGLayer, JsPAGSolidLayer } from 'liblibpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGSolidLayer extends PAGLayer {
  constructor(nativeLayer: JsPAGLayer) {
    super(nativeLayer);
  }
}