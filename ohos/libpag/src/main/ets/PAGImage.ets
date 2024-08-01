import { JsPAGImage } from 'libpag.so'

export class PAGImage {
  constructor(nativeImage: JsPAGImage) {
    this.nativeImage = nativeImage;
  }

  nativeImage: JsPAGImage;
}