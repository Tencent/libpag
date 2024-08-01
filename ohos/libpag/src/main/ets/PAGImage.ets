import { JsPAGImage } from 'liblibpag.so'

export class PAGImage {
  constructor(nativeImage: JsPAGImage) {
    this.nativeImage = nativeImage;
  }

  nativeImage: JsPAGImage;
}