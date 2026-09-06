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

/* global globalThis */
// Polyfills for WeChat Mini Program environment: registers WXWebAssembly as WebAssembly
// and ensures globalThis/window are available for Emscripten module initialization.

declare const WXWebAssembly: typeof WebAssembly;
declare const globalThis: any;

globalThis.WebAssembly = WXWebAssembly;
globalThis.isWxWebAssembly = true;
// eslint-disable-next-line no-global-assign
window = globalThis;

// WeChat Mini Program has no global crypto, so Expat aborts while requesting entropy during
// PAGX parsing. Fall back to Math.random(): the hash salt only needs to be unpredictable, not
// cryptographically secure.
if (!globalThis.crypto || typeof globalThis.crypto.getRandomValues !== 'function') {
  const getRandomValues = (array: any) => {
    if (array && ArrayBuffer.isView(array)) {
      const bytes = new Uint8Array(array.buffer, array.byteOffset, array.byteLength);
      for (let i = 0; i < bytes.length; i++) {
        bytes[i] = Math.floor(Math.random() * 256);
      }
    }
    return array;
  };
  if (globalThis.crypto) {
    globalThis.crypto.getRandomValues = getRandomValues;
  } else {
    globalThis.crypto = { getRandomValues };
  }
}

// Keep this file a module so the ambient `declare const globalThis` above stays module-scoped;
// without an import/export it becomes a global script and clashes with the built-in globalThis.
export {};
