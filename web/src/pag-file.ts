import { PAGModule } from './binding';
import { PAGComposition } from './pag-composition';
import { PAGImage } from './pag-image';
import { PAGImageLayer } from './pag-image-layer';
import { PAGLayer } from './pag-layer';
import { PAGSolidLayer } from './pag-solid-layer';
import { PAGTextLayer } from './pag-text-layer';
import { LayerType, PAG, PAGTimeStretchMode, TextDocument, Vector } from './types';
import { readFile } from './utils/common';
import { wasmAwaitRewind, wasmAsyncMethod, destroyVerify } from './utils/decorators';
import { ErrorCode } from './utils/error-map';
import { Log } from './utils/log';
import { proxyVector } from './utils/type-utils';

@destroyVerify
@wasmAwaitRewind
export class PAGFile extends PAGComposition {
  /**
   * Load pag file from file.
   */
  @wasmAsyncMethod
  public static async load(data: File | Blob | ArrayBuffer) {
    let buffer: ArrayBuffer | null = null;
    if (data instanceof File) {
      buffer = (await readFile(data)) as ArrayBuffer;
    } else if (data instanceof Blob) {
      buffer = (await readFile(new File([data], ''))) as ArrayBuffer;
    } else if (data instanceof ArrayBuffer) {
      buffer = data;
    }
    if (buffer === null) {
      Log.errorByCode(ErrorCode.PagFileDataError);
    } else {
      return PAGFile.loadFromBuffer(buffer);
    }
  }
  /**
   * Load pag file from arrayBuffer
   */
  public static loadFromBuffer(buffer: ArrayBuffer) {
    if (!buffer || !(buffer.byteLength > 0)) Log.errorByCode(ErrorCode.PagFileDataEmpty);
    const dataUint8Array = new Uint8Array(buffer);
    const numBytes = dataUint8Array.byteLength;
    const dataPtr = PAGModule._malloc(numBytes);
    const dataOnHeap = new Uint8Array(PAGModule.HEAPU8.buffer, dataPtr, numBytes);
    dataOnHeap.set(dataUint8Array);
    const wasmIns = PAGModule._PAGFile._Load(dataOnHeap.byteOffset, dataOnHeap.length);
    const pagFile = new PAGFile(wasmIns);
    PAGModule._free(dataPtr);
    return pagFile;
  }
  /**
   * The maximum tag level current SDK supports.
   */
  public static maxSupportedTagLevel(): number {
    return PAGModule._PAGFile._MaxSupportedTagLevel() as number;
  }

  /**
   * The tag level this pag file requires.
   */
  public tagLevel(): number {
    return this.wasmIns._tagLevel() as number;
  }
  /**
   * The number of replaceable texts.
   */
  public numTexts(): number {
    return this.wasmIns._numTexts() as number;
  }
  /**
   * The number of replaceable images.
   */
  public numImages(): number {
    return this.wasmIns._numImages() as number;
  }
  /**
   * The number of video compositions.
   */
  public numVideos(): number {
    return this.wasmIns._numVideos() as number;
  }
  /**
   * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
   * Note: It always returns the default text data.
   */
  public getTextData(editableTextIndex: number): TextDocument {
    return this.wasmIns._getTextData(editableTextIndex) as TextDocument;
  }
  /**
   * Replace the text data of the specified index. The index ranges from 0 to PAGFile.numTexts - 1.
   * Passing in null for the textData parameter will reset it to default text data.
   */
  public replaceText(editableTextIndex: number, textData: TextDocument) {
    return this.wasmIns._replaceText(editableTextIndex, textData);
  }
  /**
   * Replace the image content of the specified index with a PAGImage object. The index ranges from
   * 0 to PAGFile.numImages - 1. Passing in null for the image parameter will reset it to default
   * image content.
   */
  public replaceImage(editableImageIndex: number, pagImage: PAGImage) {
    this.wasmIns._replaceImage(editableImageIndex, pagImage.wasmIns);
  }
  /**
   * Return an array of layers by specified editable index and layer type.
   */
  public getLayersByEditableIndex(editableIndex: Number, layerType: LayerType) {
    const vector = this.wasmIns._getLayersByEditableIndex(editableIndex, layerType) as Vector<any>;
    switch (layerType) {
      case LayerType.Solid:
        return proxyVector(vector, PAGSolidLayer);
      case LayerType.Text:
        return proxyVector(vector, PAGTextLayer);
      case LayerType.Image:
        return proxyVector(vector, PAGImageLayer);
      default:
        return proxyVector(vector, PAGLayer);
    }
  }
  /**
   * Returns the indices of the editable layers in this PAGFile.
   * If the editableIndex of a PAGLayer is not present in the returned indices, the PAGLayer should
   * not be treated as editable.
   */
  public getEditableIndices(layerType: LayerType) {
    return this.wasmIns._getEditableIndices(layerType) as Vector<number>;
  }
  /**
   * Indicate how to stretch the original duration to fit target duration when file's duration is
   * changed. The default value is PAGTimeStretchMode::Repeat.
   */
  public timeStretchMode(): PAGTimeStretchMode {
    return this.wasmIns._timeStretchMode() as PAGTimeStretchMode;
  }
  /**
   * Set the timeStretchMode of this file.
   */
  public setTimeStretchMode(value: PAGTimeStretchMode): void {
    this.wasmIns._setTimeStretchMode(value);
  }
  /**
   * Set the duration of this PAGFile. Passing a value less than or equal to 0 resets the duration
   * to its default value.
   */
  public setDuration(duration: number): void {
    this.wasmIns._setDuration(duration);
  }
  /**
   * Set the duration of this PAGFile. Passing a value less than or equal to 0 resets the duration
   * to its default value.
   */
  public copyOriginal(): PAGFile {
    return new PAGFile(this.wasmIns._copyOriginal());
  }
}
