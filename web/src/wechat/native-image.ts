import { NativeImage as WebImage } from '../core/native-image';
import { EmscriptenGL } from '../types';
import { releaseCanvas2D } from './canvas';

export class NativeImage extends WebImage {
  public upload(GL: EmscriptenGL) {
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.source);
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
  }

  public onDestroy() {
    if (this.reuse) {
      releaseCanvas2D(this.source as OffscreenCanvas);
    }
  }
}
