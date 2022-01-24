interface CanvasStackItem {
  id: number,
  free: boolean,
  canvas: any
}

declare const wx: any;

export class WXOffscreenManager {
  private canvasStacks: CanvasStackItem[];
  public constructor() {
    this.canvasStacks = [];
  }
  public getCanvasNewId() {
    return this.canvasStacks.length;
  }
  public getFreeCanvas() {
    const freeNode = this.canvasStacks.find(node => node.free);
    if (!freeNode) {
      const newNode = this.createFreeCanvas();
      newNode.free = false;
      this.canvasStacks.push(newNode);
      return newNode;
    } else {
      freeNode.free = false;
      return freeNode;
    }
  }
  public getCanvasNodeById(id = 0) {
    return this.canvasStacks.find(node => node.id === id);
  }
  public createFreeCanvas() {
    const canvas = wx.createOffscreenCanvas({ type: '2d' });
    const id = this.getCanvasNewId();
    const stack = {
      id,
      free: true,
      canvas
    }
    return stack;
  }
  public freeCanvas(id) {
    if(!isNaN(id)) {
      const target = this.getCanvasNodeById(id);
      if (target) {
        target.free = true;
      }
    }
  }
}

export const wxOffscreenManager = new WXOffscreenManager();
