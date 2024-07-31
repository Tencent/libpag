import resourceManager from "@ohos.resourceManager";

export declare class JsPAGLayer {
}

export declare class JsPAGComposition extends JsPAGLayer {
  static Make(width: number, height: number): JsPAGComposition | null;

  width(): number;

  height(): number;

  setContentSize(width: number, height: number): void;
}

export declare class JsPAGFile extends JsPAGComposition {
  static LoadFromPath(filePath: string): JsPAGFile | null;

  static LoadFromBytes(data: Int8Array, filePath?: string): JsPAGFile | null;

  static LoadFromAssets(manager: resourceManager.ResourceManager, name: string): JsPAGFile | null;
}

export declare class JsPAGPlayer {
  setComposition(composition: JsPAGComposition | null): void;

  getComposition(): JsPAGComposition | null;

  getSurface(): JsPAGSurface | null;

  setSurface(surface: JsPAGSurface | null): void;

  setProgress(progress: number): void;

  videoEnabled(): boolean;

  flush(): void;
}

export declare class JsPAGSurface {
  static SetupOffscreen(width: number, height: number): JsPAGSurface | null;

  static GetSurface(xComponentId: string): JsPAGSurface | null;
}

export declare class JsPAGView {
  uniqueID(): string;

  flush(): void;

  setProgress(progress: number): void;

  setComposition(composition: JsPAGComposition | null): void;

  play(): void;

  pause(): void;

  setPlayingChangeCallback(callback: (boolean) => void): void;

  setProgressCallback(callback: (double) => void): void;
}