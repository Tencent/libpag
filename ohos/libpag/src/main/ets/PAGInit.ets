import { font } from '@kit.ArkUI';
import { JPAGFont } from 'libpag.so';

export class PAGInit {
  static Init() {
    if (PAGInit.hasInit) {
      return;
    }
    PAGInit.InitFontFallback();
    PAGInit.hasInit = true;
  }

  private static hasInit: boolean = false;

  private static InitFontFallback() {
    let fontConfig = font.getUIFontConfig();
    for (let index = 0; index < fontConfig.fallbackGroups.length; index++) {
      const element = fontConfig.fallbackGroups[index];
      let paths = new Array<string>();
      for (let j = 0; j < element.fallback.length; j++) {
        let fontInfo = font.getFontByName(element.fallback[j].family)
        if (fontInfo == null) {
          fontInfo = font.getFontByName(element.fallback[j].family + " Regular")
        }
        if (fontInfo != null) {
          paths.push(fontInfo.path);
        }
      }
      JPAGFont.SetFallbackFontPaths(paths);
    }
  }
}
