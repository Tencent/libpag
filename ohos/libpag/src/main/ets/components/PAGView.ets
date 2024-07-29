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
  @Link @Watch("onCompositionChange") composition: PAGComposition | null;
  @Link @Watch("onProgressChange") progress: number;
  @Link @Watch("onPlayStateChange") isPlaying: boolean;
  @Prop frameRate: number = 60.0;
  @Prop repeatCount: number = 0;

  aboutToAppear(): void {
    this.flush();
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
        this.player.setProgress(this.progress);
        if (this.isPlaying) {
          this.play();
        }
      })
      .onDestroy(() => {
        this.player.setSurface(null);
      })
      .backgroundColor(Color.Transparent)
  }

  onCompositionChange() {
    this.player.setComposition(this.composition);
    this.flush();
  }

  play(): void {
    this.timerId = setTimeout(PAGView.TickFlush, 1000.0 / this.frameRate, this);
  }

  pause(): void {
    clearTimeout(this.timerId);
    this.timerId = -1;
  }

  onProgressChange() {
    this.player.setProgress(this.progress);
  }

  flush(): void {
    this.player.flush();
  }

  onPlayStateChange() {
    if (this.isPlaying) {
      this.play();
    } else {
      this.pause();
    }
  }

  static TickFlush(view: PAGView): void {
    view.progress += 1.0 / view.frameRate;
    if (view.progress > 1) {
      view.progress = 0;
    }
    view.flush();
    if (view.isPlaying) {
      view.timerId = setTimeout(PAGView.TickFlush, 1000.0 / view.frameRate, view);
    }
  }

  private player: PAGPlayer = new PAGPlayer();
  private xId: string = generateUUID();
  private timerId: number = -1;
  private startTime: number = 0;
}