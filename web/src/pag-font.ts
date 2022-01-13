import { PAG } from './types';
import { readFile } from './utils/common';
import { ErrorCode } from './utils/error-map';
import { Log } from './utils/log';
import { defaultFontList } from './utils/font-family';
import { IPHONE, MACOS } from './utils/ua';

export class PAGFont {
  public static module: PAG;
  /**
   * Register custom font family in the browser.
   */
  public static async registerFont(family: string, data: File) {
    const buffer = (await readFile(data)) as ArrayBuffer;
    if (!buffer || !(buffer.byteLength > 0)) Log.errorByCode(ErrorCode.PagFontDataEmpty);
    const dataUint8Array = new Uint8Array(buffer);
    const fontFace = new FontFace(family, dataUint8Array);
    await fontFace.load();
  }
  /**
   * Register fallback font names from pag.
   */
  public static registerFallbackFontNames() {
    /**
     * Cannot determine whether a certain word exists in the font on the web environment.
     * Because the canvas has font fallback when drawing. 
     * The fonts registered here are mainly used to put words in a list in order, and the list can put up to UINT16_MAX words.
     * The emoji font family also has emoji words.
     */
    const fontNames = ['emoji'];
    if (MACOS || IPHONE) {
      fontNames.concat(defaultFontList.cocoa);
    } else {
      fontNames.concat(defaultFontList.windows);
    }

    const names = new this.module.VectorString();
    for (const name of fontNames) {
      names.push_back(name);
    }
    this.module.setFallbackFontNames(names);
    names.delete();
  }

  public static async loadFont(fontFamily: string, BinaryData: string | BinaryData) {
    const font = new FontFace(fontFamily, BinaryData);
    await font.load();
    document.fonts.add(font);
    return fontFamily;
  }
}
