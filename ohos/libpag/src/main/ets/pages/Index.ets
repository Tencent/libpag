import font from '@ohos.font'

@Entry
@Component
struct Index {
  @State message: string = 'Hello World';

  build() {
    Row() {
      Column() {
        Text(this.message)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .onClick(() => {
            let list = font.getSystemFontList();
          })
      }
      .width('100%')
    }
    .height('100%')
  }
}



function PAGFontInit() {
  // if (globalThis.$system.platform === '') {
  //   // Android-specific code
  //   let fontConfig = font.getUIFontConfig();
  //   for (let index = 0; index < fontConfig.fallbackGroups.length; index++) {
  //     const element = fontConfig.fallbackGroups[index];
  //     for (let j = 0; j < element.fallback.length; j++) {
  //       let fontInfo = font.getFontByName(element.fallback[j].family)
  //       if (fontInfo == null) {
  //         fontInfo = font.getFontByName(element.fallback[j].family + " Regular")
  //       }
  //       if (fontInfo != null) {
  //         console.log("getFontByName(): path = " + fontInfo.path)
  //         console.log("getFontByName(): postScriptName = " + fontInfo.postScriptName)
  //         console.log("getFontByName(): fullName = " + fontInfo.fullName)
  //         console.log("getFontByName(): Family = " + fontInfo.family)
  //         console.log("getFontByName(): Subfamily = " + fontInfo.subfamily)
  //         console.log("getFontByName(): weight = " + fontInfo.weight)
  //         console.log("getFontByName(): width = " + fontInfo.width)
  //         console.log("getFontByName(): italic = " + fontInfo.italic)
  //         console.log("getFontByName(): monoSpace = " + fontInfo.monoSpace)
  //         console.log("getFontByName(): symbolic = " + fontInfo.symbolic)
  //       }
  //     }
  //   }
  // }
}

PAGFontInit();