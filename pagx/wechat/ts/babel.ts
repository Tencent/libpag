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
import type { wx } from './interfaces';

declare const WXWebAssembly: typeof WebAssembly;
declare const wx: wx;
declare const globalThis: any;

globalThis.WebAssembly = WXWebAssembly;
globalThis.isWxWebAssembly = true;
// eslint-disable-next-line no-global-assign
window = globalThis;
