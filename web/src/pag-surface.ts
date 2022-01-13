import { PAG } from './types';

export class PAGSurface {
  public pagSurfaceWasm;

  private module: PAG;

  public constructor(module, pagSurfaceWasm) {
    this.module = module;
    this.pagSurfaceWasm = pagSurfaceWasm;
  }
  /**
   * The width of surface in pixels.
   */
  public async width(): Promise<number> {
    return (await this.module.webAssemblyQueue.exec(this.pagSurfaceWasm._width, this.pagSurfaceWasm)) as number;
  }
  /**
   * The height of surface in pixels.
   */
  public async height(): Promise<number> {
    return (await this.module.webAssemblyQueue.exec(this.pagSurfaceWasm._height, this.pagSurfaceWasm)) as number;
  }
  /**
   * Update the size of surface, and reset the internal surface.
   */
  public async updateSize() {
    return await this.module.webAssemblyQueue.exec(this.pagSurfaceWasm._updateSize, this.pagSurfaceWasm);
  }

  public async destroy() {
    await this.module.webAssemblyQueue.exec(this.pagSurfaceWasm.delete, this.pagSurfaceWasm);
  }
}
