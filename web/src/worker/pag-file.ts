import { WorkerMessageType } from './events';
import { postMessage } from './utils';
import { transferToArrayBuffer } from '../utils/common';
import { destroyVerify } from '../utils/decorators';

import type { WorkerPAGImage } from './pag-image';
import type { TextDocument } from '../types';

@destroyVerify
export class WorkerPAGFile {
  /**
   * Load pag file from file.
   */
  public static async load(worker: Worker, data: File | Blob | ArrayBuffer) {
    const buffer = await transferToArrayBuffer(data);
    if (!buffer)
      throw new Error(
        'Initialize PAGFile data type error, please put check data type must to be File ｜ Blob | ArrayBuffer!',
      );
    return await postMessage(
      worker,
      { name: WorkerMessageType.PAGFile_load, args: [data] },
      (key) => new WorkerPAGFile(worker, key),
    );
  }

  public key: number;
  public worker: Worker;
  public isDestroyed = false;

  public constructor(worker: Worker, key: number) {
    this.worker = worker;
    this.key = key;
  }
  /**
   * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
   * Note: It always returns the default text data.
   */
  public getTextData(editableTextIndex: number) {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGFile_getTextData, args: [this.key, editableTextIndex] },
      (res: TextDocument & { key: number }) => {
        res.delete = () => {
          return postMessage(
            this.worker,
            { name: WorkerMessageType.TextDocument_delete, args: [res.key] },
            () => undefined,
          );
        };
        return res;
      },
    );
  }
  /**
   * Replace the text data of the specified index. The index ranges from 0 to PAGFile.numTexts - 1.
   * Passing in null for the textData parameter will reset it to default text data.
   */
  public replaceText(editableTextIndex: number, textData: TextDocument & { [prop: string]: any }): Promise<void> {
    const textDocument: { [prop: string]: any } = {};
    for (const key in textData) {
      if (key !== 'delete') {
        textDocument[key] = textData[key];
      }
    }
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGFile_replaceText, args: [this.key, editableTextIndex, textDocument] },
      () => undefined,
    );
  }
  /**
   * Replace the image content of the specified index with a PAGImage object. The index ranges from
   * 0 to PAGFile.numImages - 1. Passing in null for the image parameter will reset it to default
   * image content.
   */
  public replaceImage(editableImageIndex: number, pagImage: WorkerPAGImage) {
    return postMessage(
      this.worker,
      { name: WorkerMessageType.PAGFile_replaceImage, args: [this.key, editableImageIndex, pagImage.key] },
      () => undefined,
    );
  }

  public destroy(): Promise<void> {
    return postMessage(this.worker, { name: WorkerMessageType.PAGFile_destroy, args: [this.key] }, () => {
      this.isDestroyed = true;
    });
  }
}
