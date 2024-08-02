import { resourceManager } from '@kit.LocalizationKit';
import { JPAGFont } from 'libpag.so';
import { PAGUtils } from './private/PAGUtils';

export class PAGFont {
  static RegisterFontFromPath(fontPath: string, ttcIndex?: number,
    fontFamily?: string, fontStyle?: string): PAGFont {
    let jFont = JPAGFont.RegisterFontFromPath(fontPath, ttcIndex, fontFamily, fontStyle);
    return PAGUtils.ToPAGFont(jFont);
  }

  static RegisterFontFromAsset(manager: resourceManager.ResourceManager, fileName: string, ttcIndex?: number,
    fontFamily?: string, fontStyle?: string): PAGFont {
    let jFont = JPAGFont.RegisterFontFromAsset(manager, fileName, ttcIndex, fontFamily, fontStyle);
    return PAGUtils.ToPAGFont(jFont);
  }

  static UnregisterFont(font: PAGFont): void {
    JPAGFont.UnregisterFont(new JPAGFont(font.fontFamily, font.fontStyle));
  }

  constructor(fontFamily?: string, fontStyle?: string) {
    this.fontFamily = fontFamily ? fontFamily : "";
    this.fontStyle = fontStyle ? fontStyle : "";
  }

  /**
   * A string with the name of the font family.
   **/
  fontFamily: string;
  /**
   * A string with the style information — e.g., “bold”, “italic”.
   **/
  fontStyle: string;
}
