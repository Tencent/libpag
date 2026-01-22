/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
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

import type { PAGX } from './types';

const WEBGL_CONTEXT_ATTRIBUTES = {
  depth: false,
  stencil: false,
  antialias: false,
};

/**
 * BackendContext wraps Emscripten GL context handle and manages context switching.
 */
export class BackendContext {
  public handle: number;
  private externallyOwned: boolean;
  private oldHandle: number = 0;

  /**
   * Create BackendContext from WebGL2RenderingContext.
   * Registers the context to Emscripten if not already registered.
   */
  public static from(module: PAGX, gl: WebGL2RenderingContext): BackendContext {
    const { GL } = module;
    let id = 0;

    // Check if context is already registered
    if (GL.contexts.length > 0) {
      id = GL.contexts.findIndex((context: any) => context?.GLctx === gl);
    }

    if (id < 1) {
      // Register new context to Emscripten (WebGL 2 only)
      id = GL.registerContext(gl, {
        majorVersion: 2,
        minorVersion: 0,
        ...WEBGL_CONTEXT_ATTRIBUTES,
      });
      return new BackendContext(id, false);
    }
    return new BackendContext(id, true);
  }

  private constructor(handle: number, externallyOwned: boolean = false) {
    this.handle = handle;
    this.externallyOwned = externallyOwned;
  }

  /**
   * Make this context current for the calling thread.
   * Returns true if successful.
   */
  public makeCurrent(module: PAGX): boolean {
    this.oldHandle = module.GL.currentContext?.handle || 0;
    if (this.oldHandle === this.handle) {
      return true;
    }
    return module.GL.makeContextCurrent(this.handle);
  }

  /**
   * Restore the previous context.
   */
  public clearCurrent(module: PAGX): void {
    if (this.oldHandle === this.handle) {
      return;
    }
    module.GL.makeContextCurrent(0);
    if (this.oldHandle) {
      module.GL.makeContextCurrent(this.oldHandle);
    }
  }

  /**
   * Destroy the context if it's not externally owned.
   */
  public destroy(module: PAGX): void {
    if (this.externallyOwned) {
      return;
    }
    module.GL.deleteContext(this.handle);
  }
}
