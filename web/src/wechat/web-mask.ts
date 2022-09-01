import { offscreenManager } from './offscreen-canvas-manager';
import { WebMask as NativeWebMask } from '../core/web-mask';

export class WebMask extends NativeWebMask {
  protected override loadCanvas() {
    const canvasNode = offscreenManager.getCanvasNode();
    return canvasNode.canvas;
  }
}
