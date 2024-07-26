import pag from 'liblibpag.so'
import { PAGComposition } from '../PAGComposition';
import { PAGPlayer } from '../PAGPlayer';
import { PAGSurface } from '../PAGSurface';

function generateUUID() {
  let d = new Date().getTime();
  let uuid = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
    let r = (d + Math.random() * 16) % 16 | 0;
    d = Math.floor(d / 16);
    return (c == 'x' ? r : (r & 0x3 | 0x8)).toString(16);
  });
  return uuid;
};

@Component
export struct PAGView {
  @Link composition: PAGComposition | null;

  aboutToAppear(): void {
    if (this.composition) {
      this.player.setComposition(this.composition);
      this.flush();
    }
  }


  build() {
    XComponent({
      id: this.xId,
      type: "surface",
      libraryname: "libpag",
    })
      .onLoad((context) => {
        this.player.setSurface(PAGSurface.GetSurface(this.xId));
        this.player.setComposition(this.composition);
        this.flush()
      })
      .onDestroy(() => {
        this.player.setSurface(null);
      })
  }

  // setComposition(composition: PAGComposition | null): PAGView {
  //   this.composition = composition;
  //   this.player.setComposition(composition);
  //   this.flush();
  //   return this;
  // }

  flush(): PAGView {
    this.player.setComposition(this.composition);
    this.player.flush();
    return this;
  }

  private player: PAGPlayer = new PAGPlayer();
  private xId: string = generateUUID();
}

declare class PAGViewAttribute extends CommonMethod<XComponentAttribute> {
  composition(): PAGViewAttribute;
}