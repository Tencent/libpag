import { destroyVerify } from '../decorators';
import { PAGFile } from '../pag-file';
import { RenderOptions, View } from './view';

@destroyVerify
export class PAG2dView extends View {
  private context: CanvasRenderingContext2D;
  private renderCanvas2D: HTMLCanvasElement;
  private renderCanvas2DContext: CanvasRenderingContext2D;

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement, options: RenderOptions) {
    super(pagFile, canvas, options);
    const context = this.canvas?.getContext('2d');
    if (!context) throw new Error("Can't get 2d context!");
    this.context = context;
    this.renderCanvas2D = document.createElement('canvas');
    this.renderCanvas2D.width = this.videoParam.MP4Width;
    this.renderCanvas2D.height = this.videoParam.MP4Height;
    const renderCanvas2DContext = this.renderCanvas2D.getContext('2d');
    if (!renderCanvas2DContext) throw new Error("Can't get 2d context!");
    this.renderCanvas2DContext = renderCanvas2DContext;
  }

  protected override flushInternal() {
    if (this.videoParam.hasAlpha) {
      this.renderCanvas2DContext.clearRect(0, 0, this.renderCanvas2D.width, this.renderCanvas2D.height);
      this.renderCanvas2DContext.drawImage(
        this.videoElement as HTMLVideoElement,
        0,
        0,
        this.renderCanvas2D.width,
        this.renderCanvas2D.height,
      );
      const frameOne = this.renderCanvas2DContext.getImageData(
        0,
        0,
        this.videoParam.sequenceWidth,
        this.videoParam.sequenceHeight,
      );
      const frameTwo = this.renderCanvas2DContext.getImageData(
        this.videoParam.alphaStartX,
        this.videoParam.alphaStartY,
        this.videoParam.sequenceWidth,
        this.videoParam.sequenceHeight,
      );
      const length = frameOne.data.length / 4;
      for (let i = 0; i < length; i++) {
        frameOne.data[i * 4 + 3] = frameTwo.data[i * 4 + 0];
      }
      this.renderCanvas2DContext.clearRect(0, 0, this.renderCanvas2D.width, this.renderCanvas2D.height);
      this.renderCanvas2DContext.putImageData(
        frameOne,
        0,
        0,
        0,
        0,
        this.videoParam.sequenceWidth,
        this.videoParam.sequenceHeight,
      );
      this.context.clearRect(0, 0, this.canvas!.width, this.canvas!.height);
      this.context.drawImage(
        this.renderCanvas2D,
        0,
        0,
        this.videoParam.sequenceWidth,
        this.videoParam.sequenceHeight,
        this.viewportSize.x,
        this.viewportSize.y,
        this.viewportSize.width,
        this.viewportSize.height,
      );
    } else {
      this.context.drawImage(
        this.videoElement as HTMLVideoElement,
        0,
        0,
        this.videoElement!.width,
        this.videoElement!.height,
        this.viewportSize.x,
        this.viewportSize.y,
        this.viewportSize.width,
        this.viewportSize.height,
      );
    }
  }

  protected override clearRender() {
    this.context.clearRect(0, 0, this.canvas!.width, this.canvas!.height);
  }
}
