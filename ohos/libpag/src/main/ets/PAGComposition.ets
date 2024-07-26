import { PAGLayer } from "./PAGLayer"
import { JsPAGLayer, JsPAGComposition } from "liblibpag.so"

export class PAGComposition extends PAGLayer {
  constructor(nativeComposition: JsPAGComposition) {
    super(nativeComposition);
    this.nativeComposition = nativeComposition;
  }

  static Make(width: number, height: number): PAGComposition | null {
    let native = JsPAGComposition.Make(width, height);
    if (native == null) {
      return null;
    }
    return new PAGComposition(native);
  }

  width(): number {
    return this.nativeComposition.width();
  }

  height(): number {
    return this.nativeComposition.height();
  }

  setContentSize(width: number, height: number): void {
    this.nativeComposition.setContentSize(width, height);
  }

  readonly nativeComposition: JsPAGComposition;
}