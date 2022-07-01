import { readFile } from './utils/common';
import { ErrorCode } from './utils/error-map';
import { Log } from './utils/log';
import { defaultFontNames } from './utils/font-family';
import { wasmAwaitRewind, wasmAsyncMethod, destroyVerify } from './utils/decorators';
import { PAGModule } from './binding';

@destroyVerify
@wasmAwaitRewind
export class PAGFont {
  /**
   * Create PAGFont instance.
   */
  public static create(fontFamily: string, fontStyle: string) {
    const wasmIns = PAGModule._PAGFont._create(fontFamily, fontStyle);
    if (!wasmIns) throw new Error('Create PAGFont fail!');
    return new PAGFont(wasmIns);
  }

  /**
   * Register custom font family in the browser.
   */
  @wasmAsyncMethod
  public static async registerFont(family: string, data: File) {
    const buffer = (await readFile(data)) as ArrayBuffer;
    if (!buffer || !(buffer.byteLength > 0)) Log.errorByCode(ErrorCode.PagFontDataEmpty);
    const dataUint8Array = new Uint8Array(buffer);
    const fontFace = new FontFace(family, dataUint8Array);
    document.fonts.add(fontFace);
    await fontFace.load();
  }

  /**
   * Register fallback font names from pag.
   */
  public static registerFallbackFontNames(fontNames: String[] = []) {
    /**
     * Cannot determine whether a certain word exists in the font on the web environment.
     * Because the canvas has font fallback when drawing.
     * The fonts registered here are mainly used to put words in a list in order, and the list can put up to UINT16_MAX words.
     * The emoji font family also has emoji words.
     */
    const vectorNames = new PAGModule.VectorString();
    const names = fontNames.concat(defaultFontNames);
    for (const name of names) {
      vectorNames.push_back(name);
    }
    PAGModule._PAGFont._SetFallbackFontNames(vectorNames);
    vectorNames.delete();
  }

  public wasmIns: any;
  public isDestroyed = false;

  public readonly fontFamily: string;
  public readonly fontStyle: string;

  public constructor(wasmIns: any) {
    this.wasmIns = wasmIns;
    this.fontFamily = this.wasmIns.fontFamily;
    this.fontStyle = this.wasmIns.fontStyle;
  }

  public destroy() {
    this.wasmIns.delete();
    this.isDestroyed = true;
  }
}
