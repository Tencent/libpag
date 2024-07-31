import pag from 'liblibpag.so'
import { PAGComposition } from './PAGComposition';

@Component
export struct PAGView {
  @Link @Watch("onCompositionChange") composition: PAGComposition | null;
  @Link @Watch("onProgressChange") progress: number;
  @Link @Watch("onPlayStateChange") isPlaying: boolean;

  getProgress() {
    return;
  }

  build() {
    XComponent({
      id: this.viewController.uniqueID(),
      type: "surface",
      libraryname: "libpag",
    })
      .onLoad((context) => {
        this.viewController.setComposition(this.composition?.nativeComposition);
        this.viewController.setProgress(this.progress);
        if (this.isPlaying) {
          this.viewController.play();
        }
      })
      .onDestroy(() => {
      })
      .backgroundColor(Color.Transparent)
  }

  aboutToAppear(): void {
    this.viewController.setPlayingChangeCallback(this.onPlayStateChange.bind(this));
    this.viewController.setProgressCallback(this.onProgressUpdate.bind(this));
  }

  onPageShow(): void {
    this.flush();
  }

  onCompositionChange() {
    this.viewController.setComposition(this.composition?.nativeComposition);
    if (!this.isPlaying) {
      this.flush()
    }
  }

  onProgressChange() {
    if (this.nativeProgress == this.progress) {
      return;
    }
    this.viewController.setProgress(this.progress);
    if (!this.isPlaying) {
      this.flush()
    }
  }

  flush(): void {
    this.viewController.flush();
  }

  onPlayStateChange() {
    if (this.isPlaying) {
      this.viewController.play();
    } else {
      this.viewController.pause();
    }
  }

  onPlayingStateChange(isPlaying: boolean): void {
    this.isPlaying = isPlaying;
  }

  onProgressUpdate(progress: number): void {
    this.nativeProgress = progress;
    this.progress = progress;
  }

  private viewController: pag.JsPAGView = new pag.JsPAGView();
  private nativeProgress: number = 0.0;
}