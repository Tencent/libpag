import { AlphaType, ColorType, PAGScaleMode } from './types';
import { destroyVerify } from './utils/decorators';
import { PAGModule } from './pag-module';
import { Matrix } from './core/matrix';
import { resolveTextureHandle, unregisterWebGPUTexture, WebGPUTextureLike } from './core/backend-context';

@destroyVerify
export class PAGImage {
  /**
   * Create pag image from image file.
   */
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
    const wasmIns = PAGModule._PAGImage._FromNativeImage(source);
    if (!wasmIns) throw new Error('Make PAGImage from source fail!');
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
    const wasmIns = PAGModule._PAGImage._FromPixels(pixels, width, height, rowBytes, colorType, alphaType);
    if (!wasmIns) throw new Error('Make PAGImage from pixels fail!');
    return new PAGImage(wasmIns);
  }
  /**
   * Creates a PAGImage object from the specified backend texture, return null if the texture is
   * invalid. On the WebGL backend, pass the numeric texture handle registered via
   * BackendContext.registerTexture(). On the WebGPU backend, pass the host GPUTexture directly;
   * it must be created from the same GPUDevice that drives PAG rendering (the device passed to
   * PAGInit) and with the TEXTURE_BINDING usage flag.
   */
  public static fromTexture(texture: number | WebGPUTextureLike, width: number, height: number, flipY: boolean) {
    const { textureID, format, webGPUHandle } = resolveTextureHandle('PAGImage.fromTexture()', texture);
    const wasmIns = PAGModule._PAGImage._FromTexture(textureID, width, height, flipY, format);
    if (!wasmIns) {
      if (webGPUHandle > 0) unregisterWebGPUTexture(webGPUHandle);
      throw new Error('Make PAGImage from texture fail!');
    }
    return new PAGImage(wasmIns, webGPUHandle);
  }

  public wasmIns;
  public isDestroyed = false;

  private webGPUTextureHandle = 0;

  public constructor(wasmIns: any, webGPUTextureHandle = 0) {
    this.wasmIns = wasmIns;
    this.webGPUTextureHandle = webGPUTextureHandle;
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
    const wasmIns = this.wasmIns._matrix();
    if (!wasmIns) throw new Error('Get matrix fail!');
    return new Matrix(wasmIns);
  }
  /**
   * Set the transformation which will be applied to the content.
   * The scaleMode property will be set to PAGScaleMode::None when this method is called.
   */
  public setMatrix(matrix: Matrix) {
    this.wasmIns._setMatrix(matrix.wasmIns);
  }
  /**
   * Destroy the pag image.
   */
  public destroy(): void {
    this.wasmIns.delete();
    if (this.webGPUTextureHandle > 0) {
      unregisterWebGPUTexture(this.webGPUTextureHandle);
      this.webGPUTextureHandle = 0;
    }
    this.isDestroyed = true;
  }
}
