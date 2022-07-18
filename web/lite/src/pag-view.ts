import { PAGFile } from './pag-file';
import { RenderingMode, ScaleMode } from './types';
import { PAG2dView } from './view/pag-2d-view';
import { PAGWebGLView } from './view/pag-webgl-view';

import type { RenderOptions, View } from './view/view';

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
      scaleMode: ScaleMode.LetterBox,
      ...options,
    };
    const pagFile = PAGFile.fromArrayBuffer(data);
    let pagView: View;
    if (opts.renderingMode === RenderingMode.WebGL) {
      pagView = new PAGWebGLView(pagFile, canvas, opts);
    } else {
      pagView = new PAG2dView(pagFile, canvas, opts);
    }

    return pagView;
  }
}
