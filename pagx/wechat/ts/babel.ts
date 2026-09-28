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
// Any new entry point that loads the wasm module must import this file first, otherwise the
// Expat crash described below will resurface.

declare const WXWebAssembly: typeof WebAssembly;
declare const globalThis: any;

globalThis.WebAssembly = WXWebAssembly;
globalThis.isWxWebAssembly = true;
// eslint-disable-next-line no-global-assign
window = globalThis;

// WeChat Mini Program has no global crypto, so Expat aborts while requesting entropy during
// PAGX parsing. The seed salts Expat's internal hash table to mitigate hash-flooding DoS.
// Math.random() is not cryptographically secure, but that is acceptable under the mini program
// threat model: a malicious PAGX can at worst deny service to the user's own mini program
// instance, and this fallback is the remedy Emscripten itself suggests in its abort message.
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
