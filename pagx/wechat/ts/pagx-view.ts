/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import { RenderCanvas, WxCanvas } from './render-canvas';
import { BackendContext } from './backend-context';
import { destroyVerify } from './decorators';
import type {
  PAGX,
  PAGXViewNative,
  ContentTransform,
  ImageBoundsEntry,
  ImageMetadataEntry,
} from './types';
import type { wx } from './interfaces';

declare const wx: wx;

/**
 * Quality tier for an image attached via View.attachNativeImage(). The two tiers map to
 * distinct slots on each Image node and obey different lifecycle rules:
 *
 *   - Thumbnail: low-resolution preview (typically <= 256x256). Acts as a fallback when the
 *     full-resolution texture is missing or has been evicted, so the affected fill area never
 *     goes blank during progressive loading or memory pressure. Thumbnails are not subject to
 *     per-frame LRU eviction; they are dropped only when the thumbnail bucket exceeds its
 *     budget, in which case the oldest entries are silently evicted.
 *
 *   - Full: full-resolution texture. Subject to per-frame LRU eviction when the full bucket
 *     exceeds its budget; evicted paths fire the onTextureEvict callback so the host can drop
 *     its own cached copy. Layers whose full texture has been evicted automatically fall back
 *     to the thumbnail on the next draw.
 *
 * Numeric values must match pagx::ImageQuality on the C++ side (Thumbnail = 0, Full = 1).
 */
export enum ImageQuality {
  Thumbnail = 0,
  Full = 1,
}

/**
 * JavaScript handler shape registered via View.setTextureEventHandler(). Every method is
 * optional; missing methods are silently skipped on the C++ side.
 */
export interface TextureEventHandler {
  /**
   * Called by the renderer once per draw whenever an Image node references a filePath whose
   * full-quality texture is not currently attached and no in-flight attachNativeImage(Full)
   * request has already been queued for the same path. The host is expected to fetch and
   * decode the asset asynchronously and resolve via attachNativeImage(filePath, source, Full);
   * doing so atomically clears the in-flight marker so the same path is not re-requested
   * unless it is evicted again later.
   */
  onTextureRequest?: (filePath: string) => void;

  /**
   * Called once per draw with every full-quality path that was just dropped by the LRU sweep.
   * The host can release the corresponding bytes from its own cache. Thumbnail evictions are
   * silent and never fire this callback.
   */
  onTextureEvict?: (filePaths: string[]) => void;
}


export interface PAGXViewOptions {
  /**
   * Auto start rendering loop. default false.
   * When true, the view will automatically start rendering after initialization.
   */
  autoRender?: boolean;
  /**
   * Custom render callback. Called before each frame is drawn.
   * Can be used for performance monitoring or custom rendering logic.
   */
  onBeforeRender?: () => void;
  /**
   * Custom render callback. Called after each frame is drawn.
   * Can be used for performance monitoring or custom rendering logic.
   */
  onAfterRender?: () => void;
  /**
   * Called once when the first frame is rendered successfully.
   * Useful for hiding loading indicators after content becomes visible.
   */
  onFirstFrame?: () => void;
}

/**
 * PAGXView for WeChat MiniProgram.
 * Manages canvas, WebGL context, and C++ PAGXViewWechat instance.
 *
 * @example Progressive image loading (async decode via host, bypasses wasm libwebp):
 * ```ts
 * const view = await View.init(module, canvas, { autoRender: true });
 * view.parsePAGX(pagxBytes);
 * view.buildLayers();               // Layout settles immediately using orig-image-*
 *                                    // recorded in the PAGX file. Shapes whose fill is an
 *                                    // ImagePattern stay transparent until their image
 *                                    // arrives, everything else paints right away.
 *
 * for (const path of view.getExternalFilePaths()) {
 *   (async () => {
 *     const bytes = await myDownloader(path);               // Custom fetch / caching.
 *     const canvas = await View.decodeImageFromBytes(bytes); // Host-native decode, runs
 *                                                            // concurrently for each path.
 *     view.loadFileDataAsNativeImage(path, canvas);          // Swap in on the next frame.
 *   })();
 * }
 * ```
 */
@destroyVerify
export class View {
  /**
   * Create PAGXView.
   * @param module PAGX module instance
   * @param canvas WeChat canvas object
   * @param options PAGXView options
   */
  public static async init(
    module: PAGX,
    canvas: WxCanvas,
    options: PAGXViewOptions = {}
  ): Promise<View> {
    const view = new View(module, canvas);
    view.pagViewOptions = { ...view.pagViewOptions, ...options };

    // Create RenderCanvas
    view.renderCanvas = RenderCanvas.from(module, canvas);
    view.renderCanvas.retain();
    view.backendContext = view.renderCanvas.backendContext;

    if (!view.backendContext) {
      throw new Error('Failed to create backend context');
    }

    // Make context current
    if (!view.backendContext.makeCurrent(module)) {
      throw new Error('Failed to make context current');
    }

    // Create C++ PAGXViewWechat instance
    view.nativeView = module.PAGXView.MakeFrom(canvas.width, canvas.height);

    view.backendContext.clearCurrent(module);

    if (!view.nativeView) {
      throw new Error('Failed to create PAGXViewWechat');
    }

    if (view.pagViewOptions.autoRender) {
      view.startRendering();
    }

    return view;
  }

  private module: PAGX;
  private nativeView: PAGXViewNative | null = null;
  private renderCanvas: RenderCanvas | null = null;
  private backendContext: BackendContext | null = null;
  private canvas: WxCanvas | null = null;
  private pagViewOptions: PAGXViewOptions = {
    autoRender: false,
  };
  private isDestroyed = false;
  private isRendering = false;
  private firstFrameCallbackFired = false;
  private animationFrameId: number = 0;

  private constructor(module: PAGX, canvas: WxCanvas) {
    this.module = module;
    this.canvas = canvas;
  }

  /**
   * Register fallback fonts for text rendering.
   * Must be called before loadPAGX() to ensure fonts are available during layout.
   * @param fontData Font file data (e.g. NotoSansSC-Regular.otf) as Uint8Array.
   * @param emojiFontData Emoji font file data (e.g. NotoColorEmoji.ttf) as Uint8Array.
   */
  public registerFonts(fontData: Uint8Array, emojiFontData: Uint8Array): void {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    this.backendContext!.makeCurrent(this.module);
    this.nativeView.registerFonts(fontData, emojiFontData);
    this.backendContext!.clearCurrent(this.module);
  }

  /**
   * Load PAGX file data and build the layer tree in one step. Equivalent to calling parsePAGX()
   * followed by buildLayers(). For PAGX files with external image resources, use the three-step
   * loading flow instead: parsePAGX() -> getExternalFilePaths() + loadFileData() -> buildLayers().
   * @param pagxData PAGX file data as Uint8Array.
   */
  public loadPAGX(pagxData: Uint8Array): void {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    this.firstFrameCallbackFired = false;
    this.nativeView.loadPAGX(pagxData);
  }

  /**
   * Parse a PAGX file without building the layer tree. After parsing, call getExternalFilePaths()
   * to determine which external resources need to be fetched, load them with loadFileData(), then
   * call buildLayers() to finalize the rendering content.
   * @param pagxData PAGX file data as Uint8Array.
   */
  public parsePAGX(pagxData: Uint8Array): void {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    this.firstFrameCallbackFired = false;
    this.nativeView.parsePAGX(pagxData);
  }

  /**
   * Returns external file paths referenced by Image nodes that need to be fetched. Call this after
   * parsePAGX() to determine which files to download. Data URIs are excluded from the result.
   * @returns An array of relative file path strings.
   */
  public getExternalFilePaths(): string[] {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    const paths = this.nativeView.getExternalFilePaths();
    const result: string[] = [];
    const count = paths.size();
    for (let i = 0; i < count; i++) {
      result.push(paths.get(i));
    }
    paths.delete();
    return result;
  }

  /**
   * Load external file data for an Image node matching the given file path. The binary data is
   * embedded into the matching node so the renderer can use it directly. Call this after
   * parsePAGX() and before buildLayers() for each path returned by getExternalFilePaths().
   * @param filePath The external file path to match against Image nodes.
   * @param fileData The file content as Uint8Array.
   * @returns True if a matching Image node was found and its data was loaded successfully.
   */
  public loadFileData(filePath: string, fileData: Uint8Array): boolean {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    return this.nativeView.loadFileData(filePath, fileData);
  }

  /**
   * Attach a host-decoded image (e.g. an OffscreenCanvas produced via
   * View.decodeImageToCanvas()) as the source for Image nodes matching the given file path.
   * This is the preferred path on WeChat for external image resources because it moves webp/png
   * decoding off the wasm main thread onto the mini-program's native decoder, and lets multiple
   * images decode concurrently.
   *
   * Unlike loadFileData(), this method may be called AFTER buildLayers(). When called post-
   * build, any VectorLayers whose shapes reference the updated image are rebuilt in place and
   * will reflect the new asset on the next draw() call, so callers can trigger progressive
   * image appearance without reconstructing the layer tree.
   *
   * @param filePath The external file path to match against Image nodes.
   * @param nativeImage A host-decoded image object (typically an OffscreenCanvas).
   * @returns True if a matching Image node was found and attached.
   */
  public loadFileDataAsNativeImage(filePath: string, nativeImage: unknown): boolean {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    return this.nativeView.loadFileDataAsNativeImage(filePath, nativeImage);
  }

  /**
   * Swap in a new host-decoded image for an already-rendered filePath and rebuild every layer
   * that references it in place. Use this when replacing a low-resolution thumbnail with a
   * higher-resolution version during progressive image loading; the next draw() picks up the
   * upgraded asset without any additional calls. Returns false when the filePath is not
   * currently referenced by any layer or buildLayers() has not been invoked yet.
   *
   * Call loadFileDataAsNativeImage() for the first-time attachment (before buildLayers()) and
   * this method for subsequent swaps (after buildLayers()).
   *
   * @param filePath The external file path to match against Image nodes.
   * @param nativeImage A host-decoded image object (typically an OffscreenCanvas).
   */
  public upgradeImageFromNative(filePath: string, nativeImage: unknown): boolean {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    return this.nativeView.upgradeImageFromNative(filePath, nativeImage);
  }

  /**
   * Attaches a host-decoded native image (typically an OffscreenCanvas) to Image nodes
   * matching the given filePath under a specific quality tier. Unlike loadFileDataAsNativeImage
   * + upgradeImageFromNative, the pixels are uploaded as a GPU-resident backend texture on the
   * next draw() and the host-side OffscreenCanvas can be released as soon as this call
   * returns; the renderer no longer holds a reference into the JS heap once the upload runs.
   *
   * Two quality tiers may be attached for the same filePath at different times. Typical
   * progressive flow:
   *   1. Initial: download a small thumbnail variant (e.g. via the imageMogr2 thumbnail
   *      transform), decode to OffscreenCanvas, attachNativeImage(path, canvas, Thumbnail).
   *   2. Later: download the full asset, decode, attachNativeImage(path, canvas, Full).
   * The thumbnail keeps the fill non-blank if the full texture is later evicted under memory
   * pressure (the renderer transparently falls back to the thumbnail until a new full
   * attachment arrives, which the host typically triggers from onTextureRequest).
   *
   * @param filePath The external file path to match against Image nodes.
   * @param nativeImage Host-decoded image source. Standard web TexImageSource or WeChat
   *                    OffscreenCanvas; the latter is detected by the `isOffscreenCanvas`
   *                    discriminator.
   * @param quality Quality tier; selects the Image-node slot the upload writes to.
   * @returns True when the request was queued. The actual GPU upload happens on the next
   *          draw(); a return value of false typically means the thumbnail budget cannot fit
   *          the upload (Thumbnail) or the document/native view is not ready.
   */
  public attachNativeImage(
    filePath: string,
    nativeImage: unknown,
    quality: ImageQuality
  ): boolean {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    return this.nativeView.attachNativeImage(filePath, nativeImage, quality);
  }

  /**
   * Registers a JavaScript handler that receives backend-texture lifecycle events from this
   * view. See TextureEventHandler for the expected method shape; both onTextureRequest and
   * onTextureEvict are optional and missing methods are silently skipped.
   *
   * Pass null/undefined to clear a previously registered handler. The handler survives
   * parsePAGX(), so callers typically register it once after init().
   */
  public setTextureEventHandler(handler: TextureEventHandler | null): void {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    this.nativeView.setTextureEventHandler(handler);
  }

  /**
   * Returns true when the SDK's full-quality cache has crossed its hard cap. Progressive
   * upgrade loops in the host should call this at the top of every upgrade pass and skip
   * new uploads while it returns true. Reactive responses to onTextureRequest remain safe
   * (1:1 replacement of just-evicted entries leaves total bytes net-flat). The flag clears
   * automatically once a viewport change lets the LRU drain enough off-viewport entries.
   *
   * Returns false when the native view is not initialized; callers in that state have no
   * cache to worry about anyway.
   */
  public isFullBudgetSaturated(): boolean {
    if (!this.nativeView) {
      return false;
    }
    return this.nativeView.isFullBudgetSaturated();
  }

  /**
   * Fetch root-space bounds for the given filePaths. Each entry describes the union of every
   * layer that renders the image (for viewport intersection) and the single layer with the
   * biggest display area (for focus-distance scoring used by progressive upgrade). Unknown
   * filePaths map to null. Must be called after buildLayers(); the first call triggers lazy
   * bounds evaluation inside tgfx and is noticeably heavier than later calls, so JS callers
   * typically defer it to the first idle window after the initial frame renders.
   *
   * @param filePaths File paths to look up. Order is preserved in the returned map keys.
   */
  public getImageBounds(filePaths: string[]): Record<string, ImageBoundsEntry | null> {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    return this.nativeView.getImageBounds(filePaths);
  }

  /**
   * Return ImagePattern usage metadata for every externally referenced image in the currently
   * parsed document. Must be called after parsePAGX(). The JS-facing progressive loader uses
   * this to choose thumbnail sizes and compute display scale without re-parsing the PAGX XML.
   */
  public getImageMetadata(): ImageMetadataEntry[] {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    return this.nativeView.getImageMetadata();
  }

  /**
   * Build the layer tree from the previously parsed document. Call this after parsePAGX() and any
   * loadFileData() calls to finalize the rendering content.
   */
  public buildLayers(): void {
    if (!this.nativeView) {
      throw new Error('Native view not initialized');
    }
    this.nativeView.buildLayers();
  }

  /**
   * Update canvas size.
   * @param width New width
   * @param height New height
   */
  public updateSize(width?: number, height?: number): void {
    if (!this.canvas) {
      throw new Error('Canvas element is not found!');
    }

    if (width !== undefined && height !== undefined) {
      this.canvas.width = width;
      this.canvas.height = height;
    }

    if (!this.backendContext) {
      return;
    }

    this.backendContext.makeCurrent(this.module);
    this.nativeView!.updateSize(this.canvas.width, this.canvas.height);
    this.backendContext.clearCurrent(this.module);
  }

  /**
   * Update zoom scale and content offset for display list.
   * @param zoom Zoom scale
   * @param offsetX X offset
   * @param offsetY Y offset
   */
  public updateZoomScaleAndOffset(zoom: number, offsetX: number, offsetY: number): void {
    this.nativeView!.updateZoomScaleAndOffset(zoom, offsetX, offsetY);
  }

  /**
   * Notify that zoom gesture has ended (for adaptive tile refinement).
   * Note: This is now handled automatically by PAGXView internal detection.
   * No need to call manually unless you want to force trigger.
   * @deprecated Automatic detection is enabled, manual call is optional.
   */
  public onZoomEnd(): void {
    this.nativeView!.onZoomEnd();
  }

  /**
   * Sets the bounds origin of the PAGX content relative to the cocraft canvas origin. This
   * overrides any values read from the PAGX file's backGroundColor customData. The values can be
   * negative.
   * @param x The x coordinate of the content bounds origin in cocraft canvas coordinates.
   * @param y The y coordinate of the content bounds origin in cocraft canvas coordinates.
   */
  public setBoundsOrigin(x: number, y: number): void {
    this.nativeView!.setBoundsOrigin(x, y);
  }

  /**
   * Toggles the gesture-freeze fast path. Call with true on pan/zoom start and false on end.
   * On active=true the native side captures the current surface as cachedSnapshot, so we
   * wrap with makeCurrent to ensure the GL context is active for the readback.
   */
  public setGestureActive(active: boolean): void {
    if (active && this.backendContext) {
      this.backendContext.makeCurrent(this.module);
      this.nativeView!.setGestureActive(active);
      this.backendContext.clearCurrent(this.module);
    } else {
      this.nativeView!.setGestureActive(active);
    }
  }

  /**
   * Returns the content transform parameters for mapping cocraft canvas coordinates to canvas
   * pixel positions. Call this once after loading a PAGX file to get the static transform needed
   * for comment overlay positioning.
   *
   * @example
   * ```ts
   * // Once after loadPAGX():
   * const t = view.getContentTransform();
   * const baseX = (cocraftX - t.boundsOriginX) * t.fitScale + t.centerOffsetX;
   * const baseY = (cocraftY - t.boundsOriginY) * t.fitScale + t.centerOffsetY;
   *
   * // On each zoom/pan gesture callback:
   * const screenX = (baseX * zoom + panOffsetX) / dpr;
   * const screenY = (baseY * zoom + panOffsetY) / dpr;
   * ```
   */
  public getContentTransform(): ContentTransform {
    return this.nativeView!.getContentTransform();
  }

  /**
   * Looks up a node by ID and returns its position relative to the canvas.
   * @param nodeId The unique identifier of the node to look up.
   * @returns An object with { found: boolean, x: number, y: number }.
   *          If the node is not found or has no position data, found is false and x/y are 0.
   */
  public getNodePosition(nodeId: string): { found: boolean; x: number; y: number } {
    if (!this.nativeView) {
      return { found: false, x: 0, y: 0 };
    }
    return this.nativeView.getNodePosition(nodeId);
  }

  /**
   * Get content width.
   */
  public contentWidth(): number {
    return this.nativeView!.contentWidth();
  }

  /**
   * Get content height.
   */
  public contentHeight(): number {
    return this.nativeView!.contentHeight();
  }

  /**
   * Returns true if the first frame has been rendered successfully.
   */
  public isFirstFrameRendered(): boolean {
    return this.nativeView!.firstFrameRendered();
  }

  /**
   * Drop any cached/fit snapshots captured between parsePAGX and the gesture-state init,
   * and reset the first-frame flag so isFirstFrameRendered() reflects post-init renders.
   * Call right after applyGestureState() on a freshly loaded document.
   */
  public resetForFreshCapture(): void {
    this.nativeView!.resetForFreshCapture();
  }

  /**
   * Start the rendering loop.
   * This will continuously call draw() using requestAnimationFrame.
   */
  public startRendering(): void {
    if (this.isRendering) {
      return;
    }
    this.isRendering = true;
    this.renderLoop();
  }

  /**
   * Stop the rendering loop.
   */
  public stopRendering(): void {
    if (!this.isRendering) {
      return;
    }
    this.isRendering = false;
    if (this.animationFrameId) {
      if (this.canvas && (this.canvas as any).cancelAnimationFrame) {
        (this.canvas as any).cancelAnimationFrame(this.animationFrameId);
      } else {
        clearTimeout(this.animationFrameId);
      }
      this.animationFrameId = 0;
    }
  }

  /**
   * Check if the view is currently rendering.
   */
  public isCurrentlyRendering(): boolean {
    return this.isRendering;
  }

  /**
   * Set render callback functions for performance monitoring.
   */
  public setRenderCallbacks(onBeforeRender?: () => void, onAfterRender?: () => void): void {
    this.pagViewOptions.onBeforeRender = onBeforeRender;
    this.pagViewOptions.onAfterRender = onAfterRender;
  }

  /**
   * Destroy PAGXView and release all resources.
   */
  public destroy(): void {
    if (this.isDestroyed) {
      return;
    }

    this.stopRendering();

    if (this.nativeView) {
      if (this.backendContext) {
        this.backendContext.makeCurrent(this.module);
      }
      this.nativeView.delete();
      this.nativeView = null;
      if (this.backendContext) {
        this.backendContext.clearCurrent(this.module);
      }
    }

    if (this.renderCanvas) {
      this.renderCanvas.release();
      this.renderCanvas = null;
    }

    this.backendContext = null;
    this.canvas = null;
    this.isDestroyed = true;
  }

  private resetSize(): void {
    if (!this.canvas) {
      throw new Error('Canvas element is not found!');
    }

    // Calculate display size for WeChat MiniProgram
    const displayWidth = (this.canvas as any).displayWidth || this.canvas.width;
    const displayHeight = (this.canvas as any).displayHeight || this.canvas.height;
    const dpr = wx.getSystemInfoSync().pixelRatio;

    this.canvas.width = displayWidth * dpr;
    this.canvas.height = displayHeight * dpr;
  }

  private renderLoop(): void {
    if (!this.isRendering || this.isDestroyed) {
      return;
    }

    if (!this.backendContext) {
      return;
    }

    if (this.pagViewOptions.onBeforeRender) {
      this.pagViewOptions.onBeforeRender();
    }

    this.backendContext.makeCurrent(this.module);
    this.nativeView!.draw();
    this.backendContext.clearCurrent(this.module);

    if (!this.firstFrameCallbackFired && this.nativeView!.firstFrameRendered()) {
      this.firstFrameCallbackFired = true;
      if (this.pagViewOptions.onFirstFrame) {
        this.pagViewOptions.onFirstFrame();
      }
    }

    if (this.pagViewOptions.onAfterRender) {
      this.pagViewOptions.onAfterRender();
    }

    if (this.canvas && (this.canvas as any).requestAnimationFrame) {
      this.animationFrameId = (this.canvas as any).requestAnimationFrame(() => {
        this.renderLoop();
      });
    } else {
      this.animationFrameId = setTimeout(() => {
        this.renderLoop();
      }, 16) as any;
    }
  }

  /**
   * Decode an image from a file path (temp file or URL the mini-program can resolve) into an
   * OffscreenCanvas. The decoded canvas is what loadFileDataAsNativeImage() expects. This
   * utility is deliberately kept as a primitive -- it does not handle downloads or caching -- so
   * callers can plug in their own network/caching logic (which is commonly required for CDN
   * signing, retry policy, and so on).
   *
   * The decode itself runs on the mini-program's native decoder (webp/png/jpeg), which is
   * multi-threaded off the JS thread, so concurrent decodeImageFromPath() calls can overlap.
   *
   * @param filePath The local temp path or remote URL.
   * @returns A promise that resolves with an OffscreenCanvas containing the decoded pixels.
   *          Rejects on decode failure.
   */
  public static decodeImageFromPath(filePath: string): Promise<OffscreenCanvas> {
    return new Promise((resolve, reject) => {
      const canvas = wx.createOffscreenCanvas({ type: '2d' }) as OffscreenCanvas & {
        createImage: () => HTMLImageElement;
      };
      const img = canvas.createImage();
      let settled = false;
      img.onload = () => {
        if (settled) return;
        settled = true;
        canvas.width = img.width;
        canvas.height = img.height;
        const ctx = canvas.getContext('2d') as OffscreenCanvasRenderingContext2D | null;
        if (!ctx) {
          reject(new Error('decodeImageFromPath: 2d context unavailable'));
          return;
        }
        ctx.drawImage(img as any, 0, 0);
        // Force the OffscreenCanvas's GPU contents to flush before handing it to the renderer.
        // On iOS WeChat the drawImage above is GPU-accelerated and may be deferred until the
        // canvas is read; calling getImageData synchronously waits for the pipeline to finish,
        // so the subsequent texImage2D upload sees actual pixels instead of an undefined
        // intermediate state that manifested as black tiles after first-time zoom-in.
        try {
          ctx.getImageData(0, 0, 1, 1);
        } catch (_) {
          // Best-effort flush. A failure here only loses the synchronization guarantee, the
          // subsequent upload still attempts to consume the canvas as before.
        }
        resolve(canvas);
      };
      img.onerror = (err: any) => {
        if (settled) return;
        settled = true;
        const msg = (err && (err.errMsg || err.message || err.type)) || 'onerror';
        reject(new Error('decodeImageFromPath: ' + msg));
      };
      img.src = filePath;
    });
  }

  /**
   * Decode an image from its encoded bytes into an OffscreenCanvas. This writes the bytes to a
   * short-lived temp file under wx.env.USER_DATA_PATH, runs decodeImageFromPath() on it, then
   * deletes the file. Prefer decodeImageFromPath() directly when the caller already has the
   * asset on disk or can pass a URL the mini-program can fetch.
   *
   * @param bytes The encoded image bytes (webp, png, jpeg, ...).
   * @param hint An optional filename hint (with extension) used only to build the temp path;
   *             does not affect decoding.
   * @returns A promise that resolves with an OffscreenCanvas containing the decoded pixels.
   */
  public static async decodeImageFromBytes(
    bytes: ArrayBuffer | Uint8Array,
    hint: string = 'pagx_decode'
  ): Promise<OffscreenCanvas> {
    const fs = wx.getFileSystemManager();
    const base = (wx as any).env?.USER_DATA_PATH ?? '';
    const unique = `${hint.replace(/[^a-zA-Z0-9._-]/g, '_')}_${Date.now()}_${Math.random()
      .toString(36)
      .slice(2, 8)}`;
    const tempPath = `${base}/${unique}`;
    const buffer = bytes instanceof Uint8Array ? bytes.buffer : bytes;
    (fs as any).writeFileSync(tempPath, buffer);
    try {
      return await View.decodeImageFromPath(tempPath);
    } finally {
      try {
        fs.unlinkSync(tempPath);
      } catch (_) {
        // Best-effort cleanup. Leaving a stray temp file is not fatal; the OS/app cleanup will
        // eventually reclaim it.
      }
    }
  }
}
