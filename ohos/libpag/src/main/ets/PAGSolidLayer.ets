import { JPAGLayer, JPAGSolidLayer } from 'libpag.so';
import { PAGLayer } from './PAGLayer';

export class PAGSolidLayer extends PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }

  solidColor(): number {
    return this.getNativeSolidColor().solidColor();
  }

  setSolidColor(color: number) {
    this.getNativeSolidColor().setSolidColor(color);
  }

  getNativeSolidColor(): JPAGSolidLayer {
    return this.nativeLayer as JPAGSolidLayer;
  }
}