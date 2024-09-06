import resourceManager from "@ohos.resourceManager";
import { image } from "@kit.ImageKit";

export declare class JPAGImage {
  static FromPath(path: string): JPAGImage | null;

  static FromBytes(data: Int8Array): JPAGImage | null;

  static FromPixelMap(pixelMap: image.PixelMap): JPAGImage | null;

  static LoadFromAssets(manager: resourceManager.ResourceManager, name: string): JPAGImage | null;

  width(): number;

  height(): number;

  matrix(): Array<number>;

  setMatrix(matrix: Array<number>);

  scaleMode(): number;

  setScaleMode(mode: number)
}

export declare class JPAGLayer {
  layerType(): number;

  layerName(): string;

  isPAGFile(): boolean;

  matrix(): Array<number>;

  setMatrix(matrix: Array<number>);

  resetMatrix();

  getTotalMatrix(): Array<number>;

  visible(): boolean;

  setVisible(value: boolean);

  editableIndex(): number;

  parent(): JPAGComposition | null;

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

  trackMatteLayer(): JPAGLayer | null;

  getBounds(): Array<number>;

  excludedFromTimeline(): boolean;

  setExcludedFromTimeline(value: boolean);
}

export declare class JPAGSolidLayer extends JPAGLayer {
  solidColor(): number;

  setSolidColor(solidColor: number);
}

export declare class JPAGImageLayer extends JPAGLayer {
  static Make(width: number, height: number, duration: number): JPAGImageLayer | null;

  contentDuration(): number;

  getVideoRanges(): Array<object>;

  setImage(image: JPAGImage);

  imageBytes(): ArrayBuffer | null;
}

export declare class JPAGTextLayer extends JPAGLayer {
  fillColor(): number;

  setFillColor(color: number): void;

  font(): JPAGFont;

  setFont(font: JPAGFont);

  fontSize(): number;

  setFontSize(fontSize: number): void;

  strokeColor(): number;

  setStrokeColor(color: number): void;

  text(): string;

  setText(text: string): void;

  reset(): void;
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

  getTextData(index: number): JPAGText | null;

  replaceText(editableTextIndex: number, textData: JPAGText | null);

  replaceImage(editableImageIndex: number, image: JPAGImage | null);

  replaceImageByName(layerName: string, image: JPAGImage | null);

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

  matrix(): Array<number>;

  setMatrix(matrix: Array<number>);

  duration(): number;

  getProgress(): number;

  setProgress(progress: number): void;

  currentFrame(): number;

  prepare(): void;

  flush(): void;

  getBounds(pagLayer: JPAGLayer): Array<number>;

  getLayersUnderPoint(surfaceX: number, surfaceY: number): Array<JPAGLayer>;

  hitTestPoint(pagLayer: JPAGLayer, surfaceX: number,
    surfaceY: number, pixelHitTest: boolean): boolean;
}

export declare class JPAGSurface {
  static MakeOffscreen(width: number, height: number): JPAGSurface | null;

  static FromSurfaceID(surfaceId: number): JPAGSurface | null;

  width(): number;

  height(): number;

  clearAll(): boolean;

  freeCache(): void;

  updateSize(): void;

  makeSnapshot(): image.PixelMap | null;
}

export declare class JPAGView {
  flush(): void;

  setProgress(progress: number): void;

  setComposition(composition: JPAGComposition | null): void;

  setRepeatCount(repeatCount: number): void;

  play(): void;

  pause(): void;

  setStateChangeCallback(callback: (number) => void): void;

  setProgressUpdateCallback(callback: (double) => void): void;

  uniqueID(): string;

  setSync(isSync: boolean): void;

  setVideoEnabled(videoEnabled: boolean): void;

  setCacheEnabled(cacheEnabled: boolean): void;

  setCacheScale(cacheScale: number): void;

  setMaxFrameRate(maxFrameRate: number): void;

  setScaleMode(scaleMode: number): void;

  setMatrix(matrix: Array<number>);

  currentFrame(): number;

  getLayersUnderPoint(x: number, y: number): Array<JPAGLayer>;

  freeCache();

  makeSnapshot(): image.PixelMap | null;

  release();
}

export declare class JPAGFont {
  static RegisterFontFromPath(fontPath: string, ttcIndex?: number,
    fontFamily?: string, fontStyle?: string): JPAGFont;

  static RegisterFontFromAsset(manager: resourceManager.ResourceManager, fileName: string, ttcIndex?: number,
    fontFamily?: string, fontStyle?: string): JPAGFont;

  static UnregisterFont(font: JPAGFont);

  static SetFallbackFontPaths(fontPath: Array<string>): void;

  constructor(fontFamily?: string, fontStyle?: string);

  fontFamily: string;

  fontStyle: string;
}

export declare class JPAGText {
  applyFill: boolean;

  applyStroke: boolean;

  baselineShift: number;

  boxText: boolean;

  boxTextRect: Array<number>;

  firstBaseLine: number;

  fauxBold: boolean;

  fauxItalic: boolean;

  fillColor: number;

  fontFamily: string;

  fontStyle: string;

  fontSize: number;

  strokeColor: number;

  strokeOverFill: boolean;

  strokeWidth: number;

  text: string;

  justification: number;

  leading: number;

  tracking: number;

  backgroundColor: number;

  backgroundAlpha: number;
}

export declare class JPAG {
  static SDKVersion(): string;
}

export declare class JPAGDiskCache {
  static MaxDiskSize(): number;

  static SetMaxDiskSize(size: number): void;

  static RemoveAll(): void;

  static SetCacheDir(path: string): void;
}