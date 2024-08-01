import { JsPAGLayer, JsPAGSolidLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGSolidLayer extends PAGLayer {
  constructor(nativeLayer: JsPAGLayer) {
    super(nativeLayer);
  }
}