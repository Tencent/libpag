import { PAG } from './types';

export class PAGLayer {
  public static module: PAG;

  private pagLayerWasm;

  public constructor(pagLayerWasm) {
    this.pagLayerWasm = pagLayerWasm;
  }
  /**
   * The duration of the layer in microseconds, indicates the length of the visible range.
   */
  public async duration(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._duration, this.pagLayerWasm)) as number;
  }
  /**
   * The start time of the layer in microseconds, indicates the start position of the visible range
   * in parent composition. It could be negative value.
   */
  public async startTime(): Promise<number> {
    return (await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm._startTime, this.pagLayerWasm)) as number;
  }
  public async destroy() {
    await PAGLayer.module.webAssemblyQueue.exec(this.pagLayerWasm.delete, this.pagLayerWasm);
  }
}
