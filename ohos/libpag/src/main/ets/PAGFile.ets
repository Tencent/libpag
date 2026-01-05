/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

import { JPAGDiskCache, JPAGFile, JPAGLayer } from 'libpag.so';
import { PAGComposition } from './PAGComposition';
import { resourceManager } from '@kit.LocalizationKit';
import { PAGLayer, PAGLayerType } from './PAGLayer';
import { PAGText } from './PAGText';
import { PAGImage } from './PAGImage';
import { PAGInit } from './PAGInit';
import { PAGUtils } from './private/PAGUtils';
import { http } from '@kit.NetworkKit';

/**
 * Defines the rules on how to stretch the timeline of content to fit the specified duration.
 */
export enum PAGTimeStretchMode {
  /**
   * Keep the original playing speed, and display the last frame if the content's duration is less
   * than target duration.
   */
  None = 0,
  /*
   * Change the playing speed of the content to fit target duration.
   */
  Scale = 1,
  /**
   * Keep the original playing speed, but repeat the content if the content's duration is less than
   * target duration. This is the default mode.
   */
  Repeat = 2,
  /**
   * Keep the original playing speed, but repeat the content in reversed if the content's duration
   * is less than target duration.
   */
  RepeatInverted = 3
}

export class PAGFile extends PAGComposition {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }

  /**
   * The maximum tag level current SDK supports.
   */
  static MaxSupportedTagLevel(): number {
    return JPAGFile.MaxSupportedTagLevel();
  };

  private static IsNetWorkPath(filePath: string): boolean {
    if (filePath == null) {
      return false;
    }
    const regex = /^(http|https|ftp):\/\/[^\s/$.?#].[^\s]*$/i;
    return regex.test(filePath);
  }

  /**
   * Load a pag file from the local path, returns null if the file does not exist or the
   * data is not a pag file.
   * Note: All PAGFiles loaded by the same path share the same internal cache. The internal
   * cache is alive until all PAGFiles are released. Use 'PAGFile.LoadFromBytes()' instead
   * if you don't want to load a PAGFile from the internal caches.
   */
  static LoadFromPath(filePath: string): PAGFile | null {
    PAGInit.Init();
    let native = JPAGFile.LoadFromPath(filePath);
    if (native == null) {
      return null;
    }
    return new PAGFile(native);
  }

  /**
   * Load a pag file from the specified path which can be a network path or a local path, returns null if the file does
   * not exist or the data is not a pag file.
   * Note: All PAGFiles loaded by the same path share the same internal cache. The internal
   * cache is alive until all PAGFiles are released. Use 'PAGFile.LoadFromBytes()' instead
   * if you don't want to load a PAGFile from the internal caches.
   */
  static LoadFromPathAsync(filePath: string): Promise<PAGFile | null> {
    PAGInit.Init();
    return new Promise<PAGFile | null>((resolve, reject) => {
      if (filePath == null) {
        resolve(null);
      }
      if (PAGFile.IsNetWorkPath(filePath)) {
        let cacheData = JPAGDiskCache.ReadFile(filePath);
        if (cacheData) {
          resolve(PAGFile.LoadFromBytes(cacheData, filePath));
        } else {
          let httpRequest = http.createHttp();
          let opt: http.HttpRequestOptions = {
            method: http.RequestMethod.GET,
            expectDataType: http.HttpDataType.ARRAY_BUFFER
          }
          httpRequest.request(filePath, opt)
            .then((resp) => {
              if (resp.responseCode == http.ResponseCode.OK) {
                let data = resp.result as ArrayBuffer;
                JPAGDiskCache.WriteFile(filePath, data);
                resolve(PAGFile.LoadFromBytes(data, filePath));
              } else {
                console.error(`download file failed! filePath: ${filePath}`);
                resolve(null);
              }
            })
            .catch((error: Error) => {
              console.error(`download file failed! filePath: ${filePath}`);
              reject(error);
            })
            .finally(() => {
              httpRequest.destroy()
            })
        }
      } else {
        let native = JPAGFile.LoadFromPath(filePath);
        if (native == null) {
          resolve(null);
        }
        resolve(new PAGFile(native as JPAGLayer));
      }
    })
  }

  /**
   * Load a pag file from ArrayBuffer, return null if data is empty or it’s not a valid pag file.
   * @param data
   * @param filePath
   * @returns
   */
  static LoadFromBytes(data: ArrayBuffer, filePath?: string): PAGFile | null {
    PAGInit.Init();
    let native = JPAGFile.LoadFromBytes(data, filePath);
    if (native == null) {
      return null;
    }
    return new PAGFile(native);
  }

  /**
   * Load a pag file from assets, returns null if the file does not exist or the data is not a
   * pag file.
   * Note: The same path shares resource in memory until the file is released. If the file
   * may be changed, please use 'PAGFile.LoadFromBytes()' instead
   */
  static LoadFromAssets(manager: resourceManager.ResourceManager, name: string): PAGFile | null {
    PAGInit.Init();
    let native = JPAGFile.LoadFromAssets(manager, name);
    if (native == null) {
      return null;
    }
    return new PAGFile(native);
  }

  /**
   * The tag level this pag file requires.
   */
  tagLevel(): number {
    return this.getNativeFile().tagLevel();
  }

  /**
   * The number of editable texts.
   */
  numTexts(): number {
    return this.getNativeFile().numTexts();
  }

  /**
   * The number of replaceable images.
   */
  numImages(): number {
    return this.getNativeFile().numImages();
  }

  /**
   * The number of video compositions.
   */
  numVideos(): number {
    return this.getNativeFile().numVideos();
  }


  /**
   * The path string of this file, returns empty string if the file is loaded from byte stream.
   */
  path(): string {
    return this.getNativeFile().path();
  }

  /**
   * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
   * Note: It always returns the default text data.
   */
  getTextData(index: number): PAGText | null {
    let nativeFile = this.getNativeFile();
    let result = nativeFile.getTextData(index);
    if (result == null) {
      return null;
    }
    return PAGUtils.ToPAGText(result);
  }

  /**
   * Replace the text data of the specified index. The index ranges from 0 to PAGFile.numTexts - 1. Passing in null
   * for the textData parameter will reset it to default text data.
   */
  replaceText(editableTextIndex: number, textData: PAGText | null) {
    if (textData == null) {
      this.getNativeFile().replaceText(editableTextIndex, null);
    } else {
      this.getNativeFile().replaceText(editableTextIndex, PAGUtils.ToJPAGText(textData));
    }
  }

  /**
   * Replace the image data of the specified index. The index ranges from 0 to PAGFile.numImages - 1. Passing in null
   * for the image parameter will reset it to default image data.
   */
  replaceImage(editableImageIndex: number, image: PAGImage | null) {
    if (image == null) {
      this.getNativeFile().replaceImage(editableImageIndex, null);
    } else {
      this.getNativeFile().replaceImage(editableImageIndex, image.nativeImage);
    }
  }

  /**
   * Replace the image data of the specified layer name. Passing in null
   * for the image parameter will reset it to default image data.
   */
  replaceImageByName(layerName: string, image: PAGImage | null) {
    if (image == null) {
      this.getNativeFile().replaceImageByName(layerName, null);
    } else {
      this.getNativeFile().replaceImageByName(layerName, image.nativeImage);
    }
  }

  /**
   * Return an array of layers by specified editable index and layer type.
   */
  getLayersByEditableIndex(editableIndex: number, layerType: PAGLayerType): Array<PAGLayer> {
    return PAGUtils.WarpJPAGLayers(this.getNativeFile().getLayersByEditableIndex(editableIndex, layerType));
  }

  /**
   * Returns the indices of the editable layers in this PAGFile.
   * If the editableIndex of a PAGLayer is not present in the returned indices, the PAGLayer should
   * not be treated as editable.
   */
  getEditableIndices(layerType: PAGLayerType): Array<number> {
    return this.getNativeFile().getEditableIndices(layerType);
  }

  /**
   * Indicate how to stretch the original duration to fit target duration when file's duration is changed.
   * The default value is PAGTimeStretchMode::Repeat.
   */
  timeStretchMode(): PAGTimeStretchMode {
    return this.getNativeFile().timeStretchMode();
  }

  /**
   * Set the timeStretchMode of this file.
   */
  setTimeStretchMode(value: PAGTimeStretchMode) {
    this.getNativeFile().setTimeStretchMode(value);
  }

  /**
   * Set the duration of this PAGFile. Passing a value less than or equal to 0 resets the duration to its default value.
   */
  setDuration(duration: number) {
    this.getNativeFile().setDuration(duration);
  }

  /**
   * Make a copy of the original file, any modification to current file has no effect on the result file.
   */
  copyOriginal(): PAGFile {
    return new PAGFile(this.getNativeFile().copyOriginal());
  }

  private getNativeFile(): JPAGFile {
    return this.nativeLayer as JPAGFile;
  }
}
