import { PAGImage } from './pag-image';
import { PAG, TextDocument } from './types';
import { readFile } from './utils/common';
import { ErrorCode } from './utils/error-map';
import { Log } from './utils/log';

export class PAGFile {
  public static module: PAG;
  /**
   * Load pag file from file.
   */
  public static async load(data: File) {
    const buffer = (await readFile(data)) as ArrayBuffer;
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

  public pagFileWasm;

  public constructor(pagFileWasm) {
    this.pagFileWasm = pagFileWasm;
  }
  /**
   * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
   * Note: It always returns the default text data.
   */
  public async getTextData(editableTextIndex: number): Promise<TextDocument> {
    return (await PAGFile.module.webAssemblyQueue.exec(
      this.pagFileWasm._getTextData,
      this.pagFileWasm,
      editableTextIndex
    )) as TextDocument;
  }
  /**
   * Replace the text data of the specified index. The index ranges from 0 to PAGFile.numTexts - 1.
   * Passing in null for the textData parameter will reset it to default text data.
   */
  public async replaceText(editableTextIndex: number, textData: TextDocument) {
    await PAGFile.module.webAssemblyQueue.exec(
      this.pagFileWasm._replaceText,
      this.pagFileWasm,
      editableTextIndex,
      textData,
    );
  }
  /**
   * Replace the image content of the specified index with a PAGImage object. The index ranges from
   * 0 to PAGFile.numImages - 1. Passing in null for the image parameter will reset it to default
   * image content.
   */
  public async replaceImage(editableImageIndex: number, pagImage: PAGImage) {
    await PAGFile.module.webAssemblyQueue.exec(
      this.pagFileWasm._replaceImage,
      this.pagFileWasm,
      editableImageIndex,
      pagImage.pagImageWasm,
    );
  }
  /**
   * Returns the width of the File.
   */
  public async width(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.pagFileWasm._width, this.pagFileWasm)) as number;
  }
  /**
   * Returns the height of the File.
   */
  public async height(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.pagFileWasm._height, this.pagFileWasm)) as number;
  }
  /**
   * The duration of the file in microseconds, indicates the length of the visible range.
   */
  public async duration(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.pagFileWasm._duration, this.pagFileWasm)) as number;
  }
  /**
   * The number of replaceable texts.
   */
  public async numTexts(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.pagFileWasm._numTexts, this.pagFileWasm)) as number;
  }
  /**
   * The number of replaceable images.
   */
  public async numImages(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.pagFileWasm._numImages, this.pagFileWasm)) as number;
  }
  /**
   * The number of video compositions.
   */
  public async numVideos(): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(this.pagFileWasm._numVideos, this.pagFileWasm)) as number;
  }
  /**
   * Return an array of layers by specified editable index and layer type.
   */
  public async getLayersByEditableIndex(editableIndex, layerType): Promise<number> {
    return (await PAGFile.module.webAssemblyQueue.exec(
      this.pagFileWasm._getLayersByEditableIndex,
      this.pagFileWasm,
      editableIndex,
      layerType,
    )) as number;
  }

  public async destroy() {
    await PAGImage.module.webAssemblyQueue.exec(this.pagFileWasm.delete, this.pagFileWasm);

  }
}
