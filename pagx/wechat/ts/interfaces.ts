/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

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
    /** Seek position in milliseconds. */
    position: number,
  ) => Promise<void>;
  start: (option: VideoDecoderStartOption) => Promise<any>;
  remove: () => Promise<any>;
  off: (
    /** Event name. */
    eventName: string,
    /** Callback function triggered when the event fires. */
    callback: (...args: any[]) => any,
  ) => void;
  on: (
    /** Event name.
     *
     * Possible values for eventName:
     * - 'start': Start event. Returns \{ width, height \}.
     * - 'stop': Stop event.
     * - 'seek': Seek completion event.
     * - 'bufferchange': Buffer change event.
     * - 'ended': Decoding ended event. */
    eventName: 'start' | 'stop' | 'seek' | 'bufferchange' | 'ended',
    /** Callback function triggered when the event fires. */
    callback: (...args: any[]) => any,
  ) => void;
}

/** Video frame data. Returns null if unavailable. May pause when the buffer is empty. */
export interface FrameDataOptions {
  /** Frame data. */
  data: ArrayBuffer;
  /** Frame data height. */
  height: number;
  /** Original DTS of the frame. */
  pkDts: number;
  /** Original PTS of the frame. */
  pkPts: number;
  /** Frame data width. */
  width: number;
}

export interface VideoDecoderStartOption {
  /** Video source file to decode. Versions below 2.13.0 only support local paths. From 2.13.0 onwards, http:// and https:// remote paths are also supported. */
  source: string;
  /** Decoding mode. 0: decode by PTS; 1: decode at maximum speed. */
  mode?: number;
}

export interface SystemInfo {
  /** Client platform. */
  platform: 'ios' | 'android' | 'windows' | 'mac' | 'devtools';
  /** Device pixel ratio. */
  pixelRatio: number;
}
