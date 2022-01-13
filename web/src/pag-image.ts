import { PAG } from './types';
import { NativeImage } from './core/native-image';

export class PAGImage {
  public static module: PAG;
  /**
   * Create pag image from image file.
   */
  public static async fromFile(data: File): Promise<PAGImage> {
    return new Promise((resolve, reject) => {
      const image = new Image();
      image.onload = async () => {
        resolve(await PAGImage.fromSource(image));
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
  public static async fromSource(source: HTMLVideoElement | HTMLImageElement): Promise<PAGImage> {
    const nativeImage = new NativeImage(source);
    return this.module._PAGImage.FromNativeImage(nativeImage);
  }

  public pagImageWasm;

  public constructor(pagImageWasm) {
    this.pagImageWasm = pagImageWasm;
  }
  /**
   * Returns the width in pixels.
   */
  public async width(): Promise<number> {
    return (await PAGImage.module.webAssemblyQueue.exec(this.pagImageWasm._width, this.pagImageWasm)) as number;
  }
  /**
   * Returns the height in pixels.
   */
  public async height(): Promise<number> {
    return (await PAGImage.module.webAssemblyQueue.exec(this.pagImageWasm._height, this.pagImageWasm)) as number;
  }
  /**
   * Destroy the pag image.
   */
  public async destroy() {
    await PAGImage.module.webAssemblyQueue.exec(this.pagImageWasm.delete, this.pagImageWasm);
  }
}
