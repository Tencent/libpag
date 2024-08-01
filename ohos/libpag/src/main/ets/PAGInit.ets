import { font } from '@kit.ArkUI';

function PAGFontInit() {
  let fontConfig = font.getUIFontConfig();
  for (let index = 0; index < fontConfig.fallbackGroups.length; index++) {
    const element = fontConfig.fallbackGroups[index];
    for (let j = 0; j < element.fallback.length; j++) {
      let fontInfo = font.getFontByName(element.fallback[j].family)
      if (fontInfo == null) {
        fontInfo = font.getFontByName(element.fallback[j].family + " Regular")
      }
      if (fontInfo != null) {

      }
    }
  }
}

export class PAGInit {
  static Init() {
    if (PAGInit.hasInit) {
      return;
    }
    PAGFontInit();
    PAGInit.hasInit = true;
  }

  static hasInit: boolean = false;
}