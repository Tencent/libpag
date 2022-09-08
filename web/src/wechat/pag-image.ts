import { PAGImage as NativePAGImage } from '../pag-image';
import { PAGModule } from '../pag-module';
import { wasmAwaitRewind } from '../utils/decorators';
import { NativeImage } from './native-image';

@wasmAwaitRewind
export class PAGImage extends NativePAGImage {
  public static fromArrayBuffer(buffer: ArrayBuffer, width: number, height: number): PAGImage {
    const nativeImage = new NativeImage(buffer, width, height);
    const wasmIns = PAGModule._PAGImage._FromNativeImage(nativeImage);
    if (!wasmIns) throw new Error('Make PAGImage from array buffer fail!');
    return new PAGImage(wasmIns);
  }
}
