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

import './babel';
import * as types from './types';
import createPAG from '../wasm/pagx-viewer';
import { PAGX } from './types';
import { binding } from './binding';

export interface moduleOption {
  /**
   * Link to wasm file.
   */
  locateFile?: (file: string) => string;
}

/**
 * Initialize PAGX webassembly module.
 */
const PAGXInit = (moduleOption: moduleOption = {}): Promise<types.PAGX> =>
  createPAG(moduleOption)
    .then((module: types.PAGX) => {
      binding(module);
      return module;
    })
    .catch((error: any) => {
      console.error(error);
      throw new Error('PAGXInit fail! Please check .wasm file path valid.');
    });

export { PAGXInit, types };