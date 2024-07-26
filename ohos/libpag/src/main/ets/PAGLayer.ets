import { JsPAGLayer } from 'liblibpag.so'


export class PAGLayer {
  constructor(nativeLayer: JsPAGLayer) {
    this.nativeLayer = nativeLayer;
  }

  readonly nativeLayer: JsPAGLayer;
}
