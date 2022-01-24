import { PAG } from './types';
import { PAGLayer } from './pag-layer';

export class PAGComposition extends PAGLayer {
  public static module: PAG;

  public constructor(wasmIns) {
    super(wasmIns);
  }

  public async width(): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._width, this.wasmIns)) as number;
  }
  public async height(): Promise<number> {
    return (await PAGComposition.module.webAssemblyQueue.exec(this.wasmIns._height, this.wasmIns)) as number;
  }
}
