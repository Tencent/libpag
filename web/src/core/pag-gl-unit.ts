import { PAGGlContext } from './pag-gl-context';
import type { PAG } from '../types';

export class PAGGlUnit {
  public static module: PAG;

  public id: number;
  public pagGlContext: PAGGlContext;

  public constructor(pagGlContext: PAGGlContext, id: number) {
    this.pagGlContext = pagGlContext;
    this.id = id;
  }

  public makeContextCurrent() {
    this.pagGlContext.makeContextCurrent();
  }
}
