import resourceManager from "@ohos.resourceManager";

export declare class JsPAGImage {
}

export declare class JsPAGLayer {
  layerType(): number;

  layerName(): string;

  matrix(): Float32Array;

  setMatrix(matrix: Float32Array);

  resetMatrix();

  getTotalMatrix(): Float32Array;

  visible(): boolean;

  setVisible(value: boolean);

  editableIndex(): number;

  parent(): JsPAGComposition;

  markers(): Array<object>;

  localTimeToGlobal(localTime: number): number;

  globalToLocalTime(globalTime: number): number;

  duration(): number;

  frameRate(): number;

  startTime(): number;

  setStartTime(time: number): void;

  currentTime(): number;

  setCurrentTime(time: number): void;

  getProgress(): number;

  setProgress(value: number): void;

  trackMatteLayer(): JsPAGLayer;

  getBounds(): Float32Array;

  excludedFromTimeline(): boolean;

  setExcludedFromTimeline(value: boolean);
}

export declare class JsPAGSolidLayer extends JsPAGLayer {
}

export declare class JsPAGImageLayer extends JsPAGLayer {
}

export declare class JsPAGTextLayer extends JsPAGLayer {
}

export declare class JsPAGShapeLayer extends JsPAGLayer {
}

export declare class JsPAGComposition extends JsPAGLayer {
  static Make(width: number, height: number): JsPAGComposition | null;
  width(): number;
  height(): number;
  setContentSize(width: number, height: number): void;
  numChildren(): number;
  getLayerAt(index: number): JsPAGLayer;
  getLayerIndex(layer: JsPAGLayer): number;
  setLayerIndex(layer: JsPAGLayer, index: number): void;
  addLayer(pagLayer: JsPAGLayer): void;
  addLayerAt(pagLayer: JsPAGLayer, index: number): void;
  contains(pagLayer: JsPAGLayer): boolean;
  removeLayer(pagLayer: JsPAGLayer): JsPAGLayer | null;
  removeLayerAt(index: number): JsPAGLayer | null;
  removeAllLayers(): void;
  swapLayer(pagLayer1: JsPAGLayer, pagLayer2: JsPAGLayer): void;
  swapLayerAt(index1: number, index2: number);
  audioBytes(): ArrayBuffer | null;
  audioStartTime(): number;
  audioMarkers(): Array<object>;
  getLayersByName(layerName: string): Array<JsPAGLayer>;
  getLayersUnderPoint(localX: number, localY: number): Array<JsPAGLayer>
}

export declare class JsPAGFile extends JsPAGComposition {
  static MaxSupportedTagLevel(): number;

  static LoadFromPath(filePath: string): JsPAGFile | null;

  static LoadFromBytes(data: Int8Array, filePath?: string): JsPAGFile | null;

  static LoadFromAssets(manager: resourceManager.ResourceManager, name: string): JsPAGFile | null;

  tagLevel(): number;

  numTexts(): number;

  numImages(): number;

  numVideos(): number;

  path(): string;

  getTextData(index: number): object;

  replaceText(editableTextIndex: number, textData: object);

  replaceImage(editableImageIndex: number, image: JsPAGImage);

  replaceImageByName(layerName: string, image: JsPAGImage);

  getLayersByEditableIndex(editableIndex: number, layerType: number): Array<JsPAGLayer>;

  getEditableIndices(layerType: number): Array<number>;

  timeStretchMode(): number;

  setTimeStretchMode(value: number);

  setDuration(duration: number);

  copyOriginal(): JsPAGFile;
}

export declare class JsPAGPlayer {
  setComposition(composition: JsPAGComposition | null): void;

  getComposition(): JsPAGComposition | null;

  getSurface(): JsPAGSurface | null;

  setSurface(surface: JsPAGSurface | null): void;

  setProgress(progress: number): void;

  videoEnabled(): boolean;

  setVideoEnabled(value: boolean): void;

  cacheEnabled(): boolean;

  setCacheEnabled(value: boolean);

  useDiskCache(): boolean;

  setUseDiskCache(value: boolean);

  cacheScale(): number;

  setCacheScale(value: number);

  maxFrameRate(): number;

  setMaxFrameRate(value: number);

  scaleMode(): number;

  setScaleMode(mode: number)

  matrix(): Float32Array;

  setMatrix(matrix: Float32Array);

  duration(): number;

  getProgress(): number;

  setProgress(progress: number): void;

  currentFrame(): number;

  prepare(): void;

  flush(): void;

  getBounds(pagLayer: JsPAGLayer): Float32Array;

  getLayersUnderPoint(surfaceX: number, surfaceY: number): Array<JsPAGLayer>;

  hitTestPoint(pagLayer: JsPAGLayer, surfaceX: number,
    surfaceY: number, pixelHitTest: boolean): boolean;
}

export declare class JsPAGSurface {
  static MakeOffscreen(width: number, height: number): JsPAGSurface | null;

  width(): number;

  height(): number;

  clearAll(): boolean;

  freeCache(): void;
}

export declare class JsPAGView {
  uniqueID(): string;

  flush(): void;

  setProgress(progress: number): void;

  setComposition(composition: JsPAGComposition | null): void;

  setRepeatCount(repeatCount: number): void;

  play(): void;

  pause(): void;

  setStateChangeCallback(callback: (number) => void): void;

  setProgressUpdateCallback(callback: (double) => void): void;
}