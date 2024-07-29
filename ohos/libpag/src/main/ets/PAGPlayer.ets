import { JsPAGPlayer } from 'liblibpag.so'
import { PAGComposition } from './PAGComposition';
import { PAGSurface } from './PAGSurface';


export class PAGPlayer {
  constructor(nativePlayer: JsPAGPlayer | null = null) {
    if (nativePlayer == null) {
      this.nativePlayer = new JsPAGPlayer();
    } else {
      this.nativePlayer = nativePlayer;
    }
  }

  flush() {
    this.nativePlayer.flush();
  }

  setComposition(composition: PAGComposition | null) {
    if (composition == null) {
      this.nativePlayer.setComposition(composition);
    } else {
      this.nativePlayer.setComposition(composition.nativeComposition);
    }
  }

  setSurface(surface: PAGSurface | null): void {
    if (surface == null) {
      this.nativePlayer.setSurface(null);
    } else {
      this.nativePlayer.setSurface(surface.nativeSurface);
    }
  }

  setProgress(progress: number): void {
    this.nativePlayer.setProgress(progress);
  }

  private nativePlayer: JsPAGPlayer;
}

//
// function PAGFontInit() {
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