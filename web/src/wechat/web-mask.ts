import { CanvasNode, offscreenManager } from './offscreen-canvas-manager';
import { WebMask as NativeWebMask } from '../core/web-mask';
import type { EmscriptenGL } from '../types';

export class WebMask extends NativeWebMask {
  public static create(width: number, height: number) {
    const canvasNode = offscreenManager.getCanvasNode();
    const webMask = new WebMask(canvasNode.canvas, width, height);
    webMask.canvasNode = canvasNode;
    return webMask;
  }

  private canvasNode: CanvasNode | null = null;

  public update(GL: EmscriptenGL) {
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    const ctx = this.canvas.getContext('2d') as CanvasRenderingContext2D;
    const imgData = ctx.getImageData(0, 0, this.canvas.width, this.canvas.height);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      imgData.width,
      imgData.height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(imgData.data),
    );
    offscreenManager.freeCanvasNode(this.canvasNode!.id);
  }
}
