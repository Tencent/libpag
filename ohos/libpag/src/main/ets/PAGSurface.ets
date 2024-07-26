import { JsPAGSurface } from 'liblibpag.so'

export class PAGSurface {
  constructor(nativeSurface: JsPAGSurface) {
    this.nativeSurface = nativeSurface;
  }

  static SetupOffscreen(width: number, height: number): PAGSurface | null {
    let nativeSurface = JsPAGSurface.SetupOffscreen(width, height);
    if (nativeSurface == null) {
      return null;
    } else {
      return new PAGSurface(nativeSurface);
    }
  }

  static GetSurface(xComponentId: string): PAGSurface | null {
    let nativeSurface = JsPAGSurface.GetSurface(xComponentId);
    if (nativeSurface == null) {
      return null;
    } else {
      return new PAGSurface(nativeSurface);
    }
  }

  nativeSurface: JsPAGSurface;
}