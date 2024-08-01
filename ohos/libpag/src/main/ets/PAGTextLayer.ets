import { JsPAGLayer, JsPAGTextLayer } from 'liblibpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGText {

}

export class PAGTextLayer extends PAGLayer {
  constructor(nativeLayer: JsPAGLayer) {
    super(nativeLayer);
  }
}