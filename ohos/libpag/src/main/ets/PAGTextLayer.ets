import { JsPAGLayer, JsPAGTextLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGText {

}

export class PAGTextLayer extends PAGLayer {
  constructor(nativeLayer: JsPAGLayer) {
    super(nativeLayer);
  }
}