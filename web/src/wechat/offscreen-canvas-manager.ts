import type { wx } from './interfaces';

export interface CanvasNode {
  id: number;
  free: boolean;
  canvas: OffscreenCanvas;
}

declare const wx: wx;

export class OffscreenManager {
  private canvasStacks: CanvasNode[];
  public constructor() {
    this.canvasStacks = [];
  }
  public getCanvasNewId() {
    return this.canvasStacks.length;
  }
  public getCanvasNode() {
    const freeNode = this.canvasStacks.find((node) => node.free);
    if (!freeNode) {
      const newNode = this.createCanvasNode();
      newNode.free = false;
      this.canvasStacks.push(newNode);
      return newNode;
    } else {
      freeNode.free = false;
      return freeNode;
    }
  }
  public getCanvasNodeById(id = 0) {
    return this.canvasStacks.find((node) => node.id === id);
  }
  public createCanvasNode() {
    const canvas = wx.createOffscreenCanvas({ type: '2d' });
    const id = this.getCanvasNewId();
    const stack = {
      id,
      free: true,
      canvas,
    };
    return stack;
  }
  public freeCanvasNode(id: number) {
    if (!isNaN(id)) {
      const target = this.getCanvasNodeById(id);
      if (target) {
        target.free = true;
      }
    }
  }
}

export const offscreenManager = new OffscreenManager();
