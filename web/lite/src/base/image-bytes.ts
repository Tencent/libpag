import { ZERO_ID } from '../constant';
import { Cache } from './cache';
import { verifyAndReturn } from './utils/verify';

export class ImageBytes {
  /**
   * A unique identifier for this image bytes.
   */
  public id: number = ZERO_ID;
  /**
   * The width of the image. It is set when the image is stripped of its transparent border,
   * otherwise the value would be 0.
   */
  public width = 0;
  /**
   * The height of the image. It is set when the image is stripped of its transparent border,
   * otherwise the value would be 0.
   */
  public height = 0;
  /**
   * The anchor x of the image. It is set when the image is stripped of its transparent border.
   */
  public anchorX = 0;
  /**
   * The anchor y of the image. It is set when the image is stripped of its transparent border.
   */
  public anchorY = 0;
  /**
   * The scale factor of image, ranges from 0.0 to 1.0.
   */
  public scaleFactor = 1.0;
  /**
   * The file data of this image bytes.
   */
  public fileBytes: ArrayBuffer | undefined;
  public cacheID = 0;

  public constructor() {
    this.cacheID = Cache.generateID();
  }

  public verify(): boolean {
    return verifyAndReturn(this.fileBytes !== undefined && this.fileBytes.byteLength > 0 && this.scaleFactor > 0);
  }
}
