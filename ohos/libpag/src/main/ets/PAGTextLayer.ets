import { JPAGLayer, JPAGTextLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGText {

}

export class PAGTextLayer extends PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }
}