export interface wx {
  env: {
    USER_DATA_PATH: string;
  };
  getFileSystemManager: () => FileSystemManager;
  getFileInfo: (object: { filePath: string; success?: () => void; fail?: () => void; complete?: () => void }) => void;
  createVideoDecoder: () => VideoDecoder;
  getSystemInfoSync: () => SystemInfo;
  createOffscreenCanvas: (
    object?: { type: 'webgl' | '2d' },
    width?: number,
    height?: number,
    compInst?: any,
  ) => OffscreenCanvas;
  getPerformance: () => Performance;
}

export interface FileSystemManager {
  accessSync: (path: string) => void;
  mkdirSync: (path: string) => void;
  writeFileSync: (path: string, data: string | ArrayBuffer, encoding: string) => void;
  unlinkSync: (path: string) => void;
  readdirSync: (path: string) => string[];
}

export interface VideoDecoder {
  getFrameData: () => FrameDataOptions;
  seek: (
    /** 跳转的解码位置，单位 ms */
    position: number,
  ) => Promise<void>;
  start: (option: VideoDecoderStartOption) => Promise<any>;
  remove: () => Promise<any>;
  off: (
    /** 事件名 */
    eventName: string,
    /** 事件触发时执行的回调函数 */
    callback: (...args: any[]) => any,
  ) => void;
  on: (
    /** 事件名
     *
     * 参数 eventName 可选值：
     * - 'start': 开始事件。返回 \{ width, height \};
     * - 'stop': 结束事件。;
     * - 'seek': seek 完成事件。;
     * - 'bufferchange': 缓冲区变化事件。;
     * - 'ended': 解码结束事件。; */
    eventName: 'start' | 'stop' | 'seek' | 'bufferchange' | 'ended',
    /** 事件触发时执行的回调函数 */
    callback: (...args: any[]) => any,
  ) => void;
}

/** 视频帧数据，若取不到则返回 null。当缓冲区为空的时候可能暂停取不到数据。 */
export interface FrameDataOptions {
  /** 帧数据 */
  data: ArrayBuffer;
  /** 帧数据高度 */
  height: number;
  /** 帧原始 dts */
  pkDts: number;
  /** 帧原始 pts */
  pkPts: number;
  /** 帧数据宽度 */
  width: number;
}

export interface VideoDecoderStartOption {
  /** 需要解码的视频源文件。基础库 2.13.0 以下的版本只支持本地路径。 2.13.0 开始支持 http:// 和 https:// 协议的远程路径。 */
  source: string;
  /** 解码模式。0：按 pts 解码；1：以最快速度解码 */
  mode?: number;
}

export interface SystemInfo {
  /** 客户端平台     */
  platform: 'ios' | 'android' | 'windows' | 'mac' | 'devtools';
  /** 设备像素比 */
  pixelRatio: number;
}
