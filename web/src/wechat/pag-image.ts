import { PAGImage as NativePAGImage } from '../pag-image';
import { PAGModule } from '../pag-module';
import { wasmAwaitRewind } from '../utils/decorators';
import { ArrayBufferImage } from './array-buffer-image';
import { NativeImage } from './native-image';

@wasmAwaitRewind
export class PAGImage extends NativePAGImage {
  /**
   * Create pag image from ArrayBuffer.
   */
  public static fromArrayBuffer(buffer: ArrayBuffer, width: number, height: number): PAGImage {
    const nativeImage = new ArrayBufferImage(buffer, width, height);
    const wasmIns = PAGModule._PAGImage._FromNativeImage(nativeImage);
    if (!wasmIns) throw new Error('Make PAGImage from array buffer fail!');
    return new PAGImage(wasmIns);
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
    const wasmIns = PAGModule._PAGImage._FromNativeImage(nativeImage);
    if (!wasmIns) throw new Error('Make PAGImage from source fail!');
    return new PAGImage(wasmIns);
  }
}
