import { JPAGImage } from 'libpag.so'

export class PAGImage {
  constructor(nativeImage: JPAGImage) {
    this.nativeImage = nativeImage;
  }

  nativeImage: JPAGImage;
}