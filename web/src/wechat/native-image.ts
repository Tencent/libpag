import { NativeImage as WebImage } from '../core/native-image';
import { releaseCanvas2D } from './canvas';

export class NativeImage extends WebImage {
  public onDestroy() {
    if (this.reuse) {
      releaseCanvas2D(this.source as OffscreenCanvas);
    }
  }
}
