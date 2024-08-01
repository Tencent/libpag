import { JsPAGLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGImageLayer extends PAGLayer {
  constructor(nativeLayer: JsPAGLayer) {
    super(nativeLayer);
  }
}