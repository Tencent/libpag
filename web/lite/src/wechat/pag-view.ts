import { PAGFile } from '../pag-file';
import { RenderingMode } from '../types';
import { PAGWebGLView } from './pag-webgl-view';
import { RenderOptions, View } from './view';

export class PAGView {
  /**
   * 实例化一个 PAGView 对象
   * @param data PAG文件数据
   * @param canvas 渲染画板
   * @param options 渲染选项
   * @returns PAGView 对象
   */
  public static init(data: ArrayBuffer, canvas: HTMLCanvasElement, options: RenderOptions = {}) {
    const opts = {
      renderingMode: RenderingMode.WebGL,
      ...options,
    };
    const pagFile = PAGFile.fromArrayBuffer(data);
    let pagView: View;
    pagView = new PAGWebGLView(pagFile, canvas, opts);
    return pagView;
  }
}
