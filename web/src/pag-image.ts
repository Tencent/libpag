import { Matrix, PAG, ScaleMode } from './types';
import { NativeImage } from './core/native-image';
import { wasmAwaitRewind, wasmAsyncMethod } from './utils/decorators';

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
  public static fromSource(source: HTMLVideoElement | HTMLImageElement): PAGImage {
    const nativeImage = new NativeImage(source);
    const wasmIns = this.module._PAGImage._FromNativeImage(nativeImage);
    return new PAGImage(wasmIns);
  }

  public wasmIns;

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
  public scaleMode(): ScaleMode {
    return this.wasmIns._scaleMode() as ScaleMode;
  }
  /**
   * Specifies the rule of how to scale the content to fit the image layer's original size.
   * The matrix changes when this method is called.
   */
  public setScaleMode(scaleMode: ScaleMode) {
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
  }
}
