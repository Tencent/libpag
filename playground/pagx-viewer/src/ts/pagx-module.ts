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

import type { PAGXModule } from './types';

let pagxModule: PAGXModule | null = null;

export function setPAGXModule(module: PAGXModule): void {
  if (pagxModule !== null && pagxModule !== module) {
    console.warn(
      'setPAGXModule: overwriting an existing PAGX module. Existing PAGXView instances bound to the previous module may behave unexpectedly.',
    );
  }
  pagxModule = module;
}

export function getPAGXModule(): PAGXModule | null {
  return pagxModule;
}
