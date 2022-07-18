export interface VideoParam {
  width: number; // VideoComposition width
  height: number; // VideoComposition height
  MP4Width: number; // MP4 width (with alpha)
  MP4Height: number; // MP4 height (with alpha)
  sequenceWidth: number; // VideoSequence width (without alpha)
  sequenceHeight: number; // VideoSequence width (without alpha)
  hasAlpha: boolean; // Whether the video has alpha
  alphaStartX: number; // Alpha start x
  alphaStartY: number; // Alpha start y
}

export enum RenderingMode {
  Canvas2D = '2d',
  WebGL = 'WebGL',
}

export enum EventName {
  /**
   * Notifies the start of the animation.
   */
  onAnimationStart = 'onAnimationStart',
  /**
   * Notifies the end of the animation.
   */
  onAnimationEnd = 'onAnimationEnd',
  /**
   * Notifies the cancellation of the animation.
   */
  onAnimationCancel = 'onAnimationCancel',
  /**
   * Notifies the repetition of the animation.
   */
  onAnimationRepeat = 'onAnimationRepeat',
  /**
   * Notifies the update of the animation.
   */
  onAnimationUpdate = 'onAnimationUpdate',
  /**
   * Notifies the play of the animation.
   */
  onAnimationPlay = 'onAnimationPlay',
  /**
   * Notifies the pause of the animation.
   */
  onAnimationPause = 'onAnimationPause',
}

export enum ScaleMode {
  None = 'None',
  /**
   * 拉伸内容到适应画布
   */
  Stretch = 'Stretch',
  /**
   * 根据原始比例缩放内容
   */
  LetterBox = 'LetterBox',
  /**
   * 根据原始比例被缩放适应，一个轴会被裁剪
   */
  Zoom = 'Zoom',
}
