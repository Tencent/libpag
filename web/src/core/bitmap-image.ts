export class BitMapImage {
  public bitmap: ImageBitmap | null;

  public constructor(bitmap: ImageBitmap | null) {
    this.bitmap = bitmap;
  }

  public setBitMap(bitmap: ImageBitmap) {
    if (this.bitmap) {
      this.bitmap.close();
    }
    this.bitmap = bitmap;
  }
}
