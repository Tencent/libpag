import { JPAGImageLayer, JPAGLayer } from 'libpag.so';
import { PAGImage } from './PAGImage';
import { PAGLayer } from './PAGLayer';
import { PAGVideoRange } from './PAGVideoRange';

export class PAGImageLayer extends PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }

  static Make(width: number, height: number, duration: number): PAGImageLayer | null {
    let nativeImageLayer = JPAGImageLayer.Make(width, height, duration);
    if (nativeImageLayer == null) {
      return null;
    } else {
      return new PAGImageLayer(nativeImageLayer);
    }
  }

  contentDuration(): number {
    return this.getNativeImageLayer().contentDuration();
  }

  getVideoRanges(): Array<PAGVideoRange> {
    return this.getNativeImageLayer().getVideoRanges() as Array<PAGVideoRange>;
  }

  setImage(image: PAGImage) {
    this.getNativeImageLayer().setImage(image.nativeImage);
  }

  imageBytes(): ArrayBuffer | null {
    return this.getNativeImageLayer().imageBytes();
  }

  getNativeImageLayer(): JPAGImageLayer {
    return this.nativeLayer as JPAGImageLayer;
  }


}