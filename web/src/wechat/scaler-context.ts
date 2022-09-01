import { offscreenManager } from './offscreen-canvas-manager';
import { ScalerContext as NativeScaleContext, resetTestCanvas } from '../core/scaler-context';

export class ScalerContext extends NativeScaleContext {
  public static isEmoji(text: string): boolean {
    if (!this.testCanvas) {
      const testCanvasNode = offscreenManager.getCanvasNode();
      this.testCanvas = testCanvasNode.canvas;
      this.testCanvas.width = 1;
      this.testCanvas.height = 1;
      this.testContext = this.testCanvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
      resetTestCanvas(this.testContext);
    }
    this.testContext.fillText(text, 0, 0);
    const color = this.testContext.getImageData(0, 0, 1, 1).data.toString();
    return !color.includes('0,0,0,');
  }

  protected override loadCanvas() {
    if (!ScalerContext.canvas) {
      const canvasNode = offscreenManager.getCanvasNode();
      ScalerContext.canvas = canvasNode.canvas;
      ScalerContext.canvas.width = 10;
      ScalerContext.canvas.height = 10;
      ScalerContext.context = ScalerContext.canvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    }
  }
}
