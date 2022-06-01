import { AlphaType, ColorType, Matrix, PAG, PAGScaleMode } from './types';
import { NativeImage } from './core/native-image';
import { wasmAwaitRewind, wasmAsyncMethod, destroyVerify } from './utils/decorators';

@destroyVerify
@wasmAwaitRewind
export class PAGImage {
  public static module: PAG;
  /**
   * Create pag image from image file.
   */
  @wasmAsyncMethod
  public static async fromFile(data: File): Promise<PAGImage> {
    return new Promise((resolve, reject) => {
      const image = new Image();
      image.onload = async () => {
        resolve(PAGImage.fromSource(image));
      };
      image.onerror = (error) => {
        reject(error);
      };
      image.src = URL.createObjectURL(data);
    });
  }
  /**
   * Create pag image from image source or video source.
   * Make sure the target pixel is shown on the screen.
   * Like
   * ``` javascript
   * Image.onload = async () => {
   *   return await PAGImage.fromSource(Image)
   * }
   * ```
   */
  public static fromSource(source: TexImageSource): PAGImage {
    const nativeImage = new NativeImage(source);
    const wasmIns = this.module._PAGImage._FromNativeImage(nativeImage);
    return new PAGImage(wasmIns);
  }
  /**
   *  Creates a PAGImage object from an array of pixel data, return null if it's not valid pixels.
   */
  public static fromPixels(
    pixels: Uint8Array,
    width: number,
    height: number,
    colorType: ColorType,
    alphaType: AlphaType,
  ): PAGImage {
    const rowBytes = width * (colorType === ColorType.ALPHA_8 ? 1 : 4);
    const dataPtr = PAGImage.module._malloc(pixels.byteLength);
    const dataOnHeap = new Uint8Array(PAGImage.module.HEAPU8.buffer, dataPtr, pixels.byteLength);
    dataOnHeap.set(pixels);
    const wasmIns = this.module._PAGImage._FromPixels(dataPtr, width, height, rowBytes, colorType, alphaType);
    return new PAGImage(wasmIns);
  }
  /**
   * Creates a PAGImage object from the specified backend texture, return null if the texture is
   * invalid.
   */
  public static fromTexture(textureID: number, width: number, height: number, flipY: boolean) {
    return new PAGImage(PAGImage.module._PAGImage._FromTexture(textureID, width, height, flipY));
  }

  public wasmIns;
  public isDestroyed = false;

  public constructor(wasmIns: any) {
    this.wasmIns = wasmIns;
  }
  /**
   * Returns the width in pixels.
   */
  public width(): number {
    return this.wasmIns._width() as number;
  }
  /**
   * Returns the height in pixels.
   */
  public height(): number {
    return this.wasmIns._height() as number;
  }
  /**
   * Returns the current scale mode. The default value is PAGScaleMode::LetterBox.
   */
  public scaleMode(): PAGScaleMode {
    return this.wasmIns._scaleMode() as PAGScaleMode;
  }
  /**
   * Specifies the rule of how to scale the content to fit the image layer's original size.
   * The matrix changes when this method is called.
   */
  public setScaleMode(scaleMode: PAGScaleMode) {
    this.wasmIns._setScaleMode(scaleMode);
  }
  /**
   * Returns a copy of current matrix.
   */
  public matrix(): Matrix {
    return this.wasmIns._matrix() as Matrix;
  }
  /**
   * Set the transformation which will be applied to the content.
   * The scaleMode property will be set to PAGScaleMode::None when this method is called.
   */
  public setMatrix(matrix: Matrix) {
    this.wasmIns._setMatrix(matrix);
  }
  /**
   * Destroy the pag image.
   */
  public destroy(): void {
    this.wasmIns.delete();
    this.isDestroyed = true;
  }
}
