import { getVideoParam } from './utils';
import { RenderingMode, EventName, ScaleMode } from '../types';
import { EventManager, Listener } from '../base/utils/event-manager';
import { destroyVerify } from '../decorators';

import type { PAGFile } from '../pag-file';
import type { VideoParam, DebugData } from '../types';
import type { VideoSequence } from '../base/video-sequence';

export interface RenderOptions {
  renderingMode?: RenderingMode;
  useScale?: boolean;
}

@destroyVerify
export class Context {
  protected canvas: HTMLCanvasElement | null;
  protected canvasSize = { width: 0, height: 0 };
  protected eventManager: EventManager;
  protected playing = false;
  protected videoParam: VideoParam;
  protected viewportSize = { x: 0, y: 0, width: 0, height: 0, scaleX: 1, scaleY: 1 }; // viewport尺寸 WebGL坐标轴轴心在左下角|Canvas2D坐标轴轴心在左上角
  protected destroyed = false;
  protected videoSequence: VideoSequence;
  protected renderTimer: number | null = null;
  protected repeatCount = 0; // 设置动画重复的次数。默认值为 0，只播放一次。如为 -1 动画则无限播放。

  private renderingMode: RenderingMode;
  private viewScaleMode = ScaleMode.LetterBox;
  private debugData: DebugData = {
    FPS: 0,
    decodePAGFile: 0,
    createDir: 0,
    coverMP4: 0,
    writeFile: 0,
    createDecoder: 0,
    getFrame: 0,
    draw: 0,
  };

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement, options: RenderOptions) {
    const videoSequence = pagFile.getVideoSequence();
    if (!videoSequence) throw new Error('PAGFile has no BMP video sequence!');
    delete videoSequence.composition;
    this.videoSequence = videoSequence;
    this.canvas = canvas;
    this.videoParam = getVideoParam(pagFile, videoSequence);
    this.eventManager = new EventManager();
    this.renderingMode = options.renderingMode || RenderingMode.WebGL;
    this.updateSize(options.useScale);
    this.setScaleMode();
  }
  /**
   * 是否播放中
   */
  public isPlaying() {
    return this.playing;
  }
  /**
   * 是否已经销毁
   */
  public isDestroyed() {
    return this.destroyed;
  }
  /**
   * 动画持续时间
   */
  public duration() {
    return this.videoSequence.frameCount / this.videoSequence.frameRate;
  }
  /**
   * 动画持续时间
   */
  public frameRate() {
    return this.videoSequence.frameRate;
  }
  /**
   * 设置动画重复的次数。默认值为 1，只播放一次。如为 0 动画则无限播放。
   */
  public setRepeatCount(repeatCount = 1) {
    this.repeatCount = repeatCount < 0 ? -1 : repeatCount - 1;
  }

  /**
   * 增加事件监听
   */
  public addListener(eventName: EventName, listener: Listener) {
    return this.eventManager.on(eventName, listener);
  }
  /**
   * 移除事件监听
   */
  public removeListener(eventName: EventName, listener?: Listener) {
    return this.eventManager.off(eventName, listener);
  }
  /**
   * 返回当前缩放模式
   */
  public scaleMode() {
    return this.viewScaleMode;
  }
  /**
   * 指定缩放内容的模式
   */
  public setScaleMode(scaleMode: ScaleMode = ScaleMode.LetterBox) {
    this.viewScaleMode = scaleMode;
    switch (scaleMode) {
      case ScaleMode.None:
        this.viewportSize = {
          x: 0,
          y: this.renderingMode === RenderingMode.WebGL ? this.canvas!.height - this.videoParam.height : 0,
          width: this.videoParam.width,
          height: this.videoParam.height,
          scaleX: 1,
          scaleY: 1,
        };
        break;
      case ScaleMode.Stretch:
        this.viewportSize = {
          x: 0,
          y: 0,
          width: this.canvas!.width,
          height: this.canvas!.height,
          scaleX: this.canvas!.width / this.videoParam.sequenceWidth,
          scaleY: this.canvas!.height / this.videoParam.sequenceHeight,
        };
        break;
      case ScaleMode.LetterBox:
        {
          const scaleX = this.canvas!.width / this.videoParam.sequenceWidth;
          const scaleY = this.canvas!.height / this.videoParam.sequenceHeight;
          const scale = Math.min(scaleX, scaleY);
          this.viewportSize = {
            x: (this.canvas!.width - this.videoParam.sequenceWidth * scale) / 2,
            y: (this.canvas!.height - this.videoParam.sequenceHeight * scale) / 2,
            width: this.videoParam.sequenceWidth * scale,
            height: this.videoParam.sequenceHeight * scale,
            scaleX: scale,
            scaleY: scale,
          };
        }
        break;
      case ScaleMode.Zoom:
        {
          const scaleX = this.canvas!.width / this.videoParam.sequenceWidth;
          const scaleY = this.canvas!.height / this.videoParam.sequenceHeight;
          const scale = Math.max(scaleX, scaleY);
          this.viewportSize = {
            x: (this.canvas!.width - this.videoParam.sequenceWidth * scale) / 2,
            y: (this.canvas!.height - this.videoParam.sequenceHeight * scale) / 2,
            width: this.videoParam.sequenceWidth * scale,
            height: this.videoParam.sequenceHeight * scale,
            scaleX: scale,
            scaleY: scale,
          };
        }
        break;
      default:
        break;
    }
  }

  public updateSize(useScale = true) {
    if (!this.canvas) {
      throw new Error('Canvas element is not found!');
    }
    let displaySize: { width: number; height: number };
    const styleDeclaration = getComputedStyle(this.canvas as HTMLCanvasElement);
    const computedSize = {
      width: Number(styleDeclaration.width.replace('px', '')),
      height: Number(styleDeclaration.height.replace('px', '')),
    };
    if (computedSize.width > 0 && computedSize.height > 0) {
      displaySize = computedSize;
    } else {
      const styleSize = {
        width: Number(this.canvas.style.width.replace('px', '')),
        height: Number(this.canvas.style.height.replace('px', '')),
      };
      if (styleSize.width > 0 && styleSize.height > 0) {
        displaySize = styleSize;
      } else {
        displaySize = {
          width: this.canvas.width,
          height: this.canvas.height,
        };
      }
    }

    if (!useScale) {
      this.canvas!.width = this.canvas!.width || displaySize.width;
      this.canvas!.height = this.canvas!.height || displaySize.height;
      return;
    }
    this.canvas.style.width = `${displaySize.width}px`;
    this.canvas.style.height = `${displaySize.height}px`;
    this.canvas.width = displaySize.width * window.devicePixelRatio;
    this.canvas.height = displaySize.height * window.devicePixelRatio;
  }

  public getDebugData() {
    return this.debugData;
  }

  public setDebugData(data: DebugData) {
    this.debugData = { ...this.debugData, ...data };
  }

  protected loadContext() {}

  protected clearRender() {}
}
