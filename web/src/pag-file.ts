import { PAGComposition } from './pag-composition';
import { PAGImage } from './pag-image';
import { LayerType, PAG, PAGTimeStretchMode, TextDocument } from './types';
/* #if _WECHAT
import { isWechatMiniProgram } from './utils/ua';
//#else */
// #endif
import { readFile } from './utils/common';
import { ErrorCode } from './utils/error-map';
import { Log } from './utils/log';

export class PAGFile extends PAGComposition {
  public static module: PAG;
  /**
   * Load pag file from file.
   */
  /* #if _WECHAT
  public static async load(data: ArrayBuffer) {
//#else */
  public static async load(data: File) {
// #endif
    /* #if _WECHAT
    const buffer = data;
    //#else */
    const buffer = (await readFile(data)) as ArrayBuffer;
    // #endif
    if (!buffer || !(buffer.byteLength > 0)) Log.errorByCode(ErrorCode.PagFileDataEmpty);
    const dataUint8Array = new Uint8Array(buffer);
    const numBytes = dataUint8Array.byteLength * dataUint8Array.BYTES_PER_ELEMENT;
    const dataPtr = this.module._malloc(numBytes);
    const dataOnHeap = new Uint8Array(this.module.HEAPU8.buffer, dataPtr, numBytes);
    dataOnHeap.set(dataUint8Array);
    const pagFile = await this.module._PAGFile.Load(dataOnHeap.byteOffset, dataOnHeap.length);
    this.module._free(dataPtr);
    return pagFile;
  }

  public constructor(wasmIns) {
    super(wasmIns);
  }
  /**
   * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
   * Note: It always returns the default text data.
   */
  public async getTextData(editableTextIndex: number): Promise<TextDocument> {
    return (await PAGFile.module.webAssemblyQueue.exec(
      this.wasmIns._getTextData,
      this.wasmIns,
      editableTextIndex,
    )) as TextDocument;
  }
  /**
   * Replace the text data of the specified index. The index ranges from 0 to PAGFile.numTexts - 1.
   * Passing in null for the textData parameter will reset it to default text data.
   */
  public async replaceText(editableTextIndex: number, textData: TextDocument) {
    await PAGFile.module.webAssemblyQueue.exec(this.wasmIns._replaceText, this.wasmIns, editableTextIndex, textData);
  }
  /**
   * Replace the image content of the specified index with a PAGImage object. The index ranges from
   * 0 to PAGFile.numImages - 1. Passing in null for the image parameter will reset it to default
   * image content.
   */
  public async replaceImage(editableImageIndex: number, pagImage: PAGImage) {
    await PAGFile.module.webAssemblyQueue.exec(
      this.wasmIns._replaceImage,
      this.wasmIns,
      editableImageIndex,
      pagImage.pagImageWasm,
    );
  }
  /**
   * The duration of the file in microseconds, indicates the length of the visible range.
   */
  public async duration(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.wasmIns._duration, this.wasmIns)) as number;
  }
  /**
   * The number of replaceable texts.
   */
  public async numTexts(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.wasmIns._numTexts, this.wasmIns)) as number;
  }
  /**
   * The number of replaceable images.
   */
  public async numImages(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.wasmIns._numImages, this.wasmIns)) as number;
  }
  /**
   * The number of video compositions.
   */
  public async numVideos(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.wasmIns._numVideos, this.wasmIns)) as number;
  }
  /**
   * Return an array of layers by specified editable index and layer type.
   */
  public async getLayersByEditableIndex(editableIndex: Number, layerType: LayerType): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(
      this.wasmIns._getLayersByEditableIndex,
      this.wasmIns,
      editableIndex,
      layerType,
    )) as number;
  }
  /**
   * Indicate how to stretch the original duration to fit target duration when file's duration is
   * changed. The default value is PAGTimeStretchMode::Repeat.
   */
  public async timeStretchMode(): Promise<PAGTimeStretchMode> {
    return (await PAGFile.module.webAssemblyQueue.exec(
      this.wasmIns._timeStretchMode,
      this.wasmIns,
    )) as PAGTimeStretchMode;
  }
  /**
   * Set the timeStretchMode of this file.
   */
  public async setTimeStretchMode(value: PAGTimeStretchMode): Promise<void> {
    await PAGFile.module.webAssemblyQueue.exec(this.wasmIns._setTimeStretchMode, this.wasmIns, value);
  }
  /**
   * Set the duration of this PAGFile. Passing a value less than or equal to 0 resets the duration
   * to its default value.
   */
  public async setDuration(duration: number): Promise<void> {
    await PAGFile.module.webAssemblyQueue.exec(this.wasmIns._setDuration, this.wasmIns, duration);
  }

  public async destroy() {
    await PAGImage.module.webAssemblyQueue.exec(this.wasmIns.delete, this.wasmIns);
  }
}
