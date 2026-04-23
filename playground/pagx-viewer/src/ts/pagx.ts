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

import { PAGXBind } from './binding';
import * as types from './types';
import createPAGX from '../../wasm-mt/pagx-viewer';

export { PAGXView } from './pagx-view';

export interface ModuleOption {
  /**
   * Custom function to locate the WASM file.
   * @param file The filename ('pagx-viewer.wasm' or 'pagx-viewer.js')
   * @returns The URL or path to the file
   */
  locateFile?: (file: string) => string;

  /**
   * URL or Blob for the main script file. Required for multi-threaded builds
   * when the code is bundled, so workers can be spawned correctly.
   */
  mainScriptUrlOrBlob?: string | Blob;

  /**
   * Pre-fetched WASM binary data. If provided, the module will use this
   * instead of fetching the WASM file.
   */
  wasmBinary?: ArrayBuffer;
}

/**
 * Initializes the PAGX Viewer WebAssembly module.
 *
 * @example
 * ```typescript
 * import { PAGXInit } from 'pagx-viewer';
 *
 * // Initialize the WASM module
 * const PAGX = await PAGXInit({ locateFile: (file) => '/wasm/' + file });
 *
 * // Create a view
 * const view = PAGX.PAGXView.init('#canvas');
 *
 * // Load resources
 * view.registerFonts(fontData, emojiFontData);
 * view.parsePAGX(pagxData);
 * view.buildLayers();
 * view.updateSize();
 *
 * // Start render loop
 * view.start();
 *
 * // Update zoom/pan
 * view.updateZoomScaleAndOffset(2.0, 100, 50);
 *
 * // Cleanup
 * view.stop();
 * view.destroy();
 * ```
 */
export function PAGXInit(moduleOption: ModuleOption = {}): Promise<types.PAGXModule> {
  return createPAGX(moduleOption)
    .then((module: types.PAGXModule) => {
      PAGXBind(module);
      return module;
    })
    .catch((error: unknown) => {
      console.error('PAGXInit failed:', error);
      throw new Error('PAGXInit failed. Please check the .wasm file path.');
    });
}

export { types };
