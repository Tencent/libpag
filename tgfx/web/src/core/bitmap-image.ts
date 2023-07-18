export class BitmapImage {
  public bitmap: ImageBitmap | null;

  public constructor(bitmap: ImageBitmap | null) {
    this.bitmap = bitmap;
  }

  public setBitmap(bitmap: ImageBitmap) {
    if (this.bitmap) {
      this.bitmap.close();
    }
    this.bitmap = bitmap;
  }
}
