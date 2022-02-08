import { convertMp4 } from '../h264/h264';

declare const wx: any;
export class VideoReader {
  private readonly frameRate: number;
  private currentFrame: number;
  private fileTarget: string;
  private videoDecoderReady = false;
  private videoDecoder: any;
  private framebuffer: any;

  public constructor(width, height, frameRate, h264Headers, h264Frames, ptsList) {
    this.frameRate = frameRate;
    this.currentFrame = -1;
    const mp4 = convertMp4(h264Frames, h264Headers, width, height, this.frameRate, ptsList);
    const fs = wx.getFileSystemManager();
    const pagPath = `${wx.env.USER_DATA_PATH}/pag/`;
    this.fileTarget = `${pagPath}${new Date().getTime()}.mp4`;
    try {
      fs.accessSync(pagPath);
    } catch (e) {
      try {
        fs.mkdirSync(pagPath);
      } catch (err) {
        console.error(e);
      }
    }
    fs.writeFileSync(this.fileTarget, mp4.buffer, 'utf8');
    this.videoDecoder = wx.createVideoDecoder();
    this.videoDecoderStart(this.fileTarget);
    this.videoDecoder.on('ended', async () => {
      await this.videoDecoderSeek(0);
      this.currentFrame = -1;
    });
    this.framebuffer = null;
  }
  public videoDecoderSeek(position) {
    return new Promise((resolve) => {
      const onSeeked = (e) => {
        this.videoDecoder.off('seek', onSeeked);
        resolve(true);
      };
      this.videoDecoder.on('seek', onSeeked);
      this.videoDecoder.seek(position);
    });
  }
  public getFrameData() {
    return new Promise((resolve) => {
      const loop = () => {
        const frameData = this.videoDecoder.getFrameData();
        if (frameData !== null) {
          this.framebuffer = frameData;
          resolve(true);
          return;
        }
        setTimeout(() => {
          loop();
        }, 1);
      };
      loop();
    });
  }
  public prepareAsync(targetFrame) {
    if (targetFrame === this.currentFrame) {
      this.currentFrame = targetFrame;
      return Promise.resolve(true);
    } else {
      if (!this.videoDecoderReady) {
        this.videoDecoderStart(this.fileTarget);
      }
      this.currentFrame = targetFrame;
      return Promise.resolve(this.getFrameData());
    }
  }
  public renderToTexture(GL, textureID) {
    const gl = GL.currentContext.GLctx;
    gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      this.framebuffer.width,
      this.framebuffer.height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(this.framebuffer.data, 0, this.framebuffer.data.length),
    );
  }
  public onDestroy() {
    this.currentFrame = -1;
  }
  private videoDecoderStart(path) {
    return new Promise((resolve) => {
      const onStarted = () => {
        this.videoDecoderReady = true;
        this.videoDecoder.off('start', onStarted);
        resolve(true);
      };
      this.videoDecoder.on('start', onStarted);
      this.videoDecoder.start({ source: path, mode: 0 });
    });
  }
}
