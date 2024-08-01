import resourceManager from "@ohos.resourceManager";

export declare class JPAGImage {
}

export declare class JPAGLayer {
  layerType(): number;

  layerName(): string;

  isPAGFile(): boolean;

  matrix(): Float32Array;

  setMatrix(matrix: Float32Array);

  resetMatrix();

  getTotalMatrix(): Float32Array;

  visible(): boolean;

  setVisible(value: boolean);

  editableIndex(): number;

  parent(): JPAGComposition;

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

  trackMatteLayer(): JPAGLayer;

  getBounds(): Float32Array;

  excludedFromTimeline(): boolean;

  setExcludedFromTimeline(value: boolean);
}

export declare class JPAGSolidLayer extends JPAGLayer {
}

export declare class JPAGImageLayer extends JPAGLayer {
}

export declare class JPAGTextLayer extends JPAGLayer {
}

export declare class JPAGShapeLayer extends JPAGLayer {
}

export declare class JPAGComposition extends JPAGLayer {
  static Make(width: number, height: number): JPAGComposition | null;

  width(): number;

  height(): number;

  setContentSize(width: number, height: number): void;

  numChildren(): number;

  getLayerAt(index: number): JPAGLayer;

  getLayerIndex(layer: JPAGLayer): number;

  setLayerIndex(layer: JPAGLayer, index: number): void;

  addLayer(pagLayer: JPAGLayer): void;

  addLayerAt(pagLayer: JPAGLayer, index: number): void;

  contains(pagLayer: JPAGLayer): boolean;

  removeLayer(pagLayer: JPAGLayer): JPAGLayer | null;

  removeLayerAt(index: number): JPAGLayer | null;

  removeAllLayers(): void;

  swapLayer(pagLayer1: JPAGLayer, pagLayer2: JPAGLayer): void;

  swapLayerAt(index1: number, index2: number);

  audioBytes(): ArrayBuffer | null;

  audioStartTime(): number;

  audioMarkers(): Array<object>;

  getLayersByName(layerName: string): Array<JPAGLayer>;

  getLayersUnderPoint(localX: number, localY: number): Array<JPAGLayer>
}

export declare class JPAGFile extends JPAGComposition {
  static MaxSupportedTagLevel(): number;

  static LoadFromPath(filePath: string): JPAGFile | null;

  static LoadFromBytes(data: Int8Array, filePath?: string): JPAGFile | null;

  static LoadFromAssets(manager: resourceManager.ResourceManager, name: string): JPAGFile | null;

  tagLevel(): number;

  numTexts(): number;

  numImages(): number;

  numVideos(): number;

  path(): string;

  getTextData(index: number): object;

  replaceText(editableTextIndex: number, textData: object);

  replaceImage(editableImageIndex: number, image: JPAGImage);

  replaceImageByName(layerName: string, image: JPAGImage);

  getLayersByEditableIndex(editableIndex: number, layerType: number): Array<JPAGLayer>;

  getEditableIndices(layerType: number): Array<number>;

  timeStretchMode(): number;

  setTimeStretchMode(value: number);

  setDuration(duration: number);

  copyOriginal(): JPAGFile;
}

export declare class JPAGPlayer {
  setComposition(composition: JPAGComposition | null): void;

  getComposition(): JPAGComposition | null;

  getSurface(): JPAGSurface | null;

  setSurface(surface: JPAGSurface | null): void;

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

  getBounds(pagLayer: JPAGLayer): Float32Array;

  getLayersUnderPoint(surfaceX: number, surfaceY: number): Array<JPAGLayer>;

  hitTestPoint(pagLayer: JPAGLayer, surfaceX: number,
    surfaceY: number, pixelHitTest: boolean): boolean;
}

export declare class JPAGSurface {
  static MakeOffscreen(width: number, height: number): JPAGSurface | null;

  width(): number;

  height(): number;

  clearAll(): boolean;

  freeCache(): void;
}

export declare class JPAGView {
  uniqueID(): string;

  flush(): void;

  setProgress(progress: number): void;

  setComposition(composition: JPAGComposition | null): void;

  setRepeatCount(repeatCount: number): void;

  play(): void;

  pause(): void;

  setStateChangeCallback(callback: (number) => void): void;

  setProgressUpdateCallback(callback: (double) => void): void;
}