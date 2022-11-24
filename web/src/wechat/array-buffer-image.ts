import { EmscriptenGL } from '../types';

export class ArrayBufferImage {
  private buffer: ArrayBuffer;
  private _width: number;
  private _height: number;
  public constructor(buffer: ArrayBuffer, width: number, height: number) {
    this.buffer = buffer;
    this._width = width;
    this._height = height;
  }

  public width(): number {
    return this._width;
  }

  public height(): number {
    return this._height;
  }

  public upload(GL: EmscriptenGL) {
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
    gl.texSubImage2D(
      gl.TEXTURE_2D,
      0,
      0,
      0,
      this._width,
      this._height,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(this.buffer),
    );
    gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
  }
}
