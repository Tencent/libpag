import { Layer, LayerType } from './layer';

export class UnDefinedLayer extends Layer {
  public type(): LayerType {
    return LayerType.undefined;
  }
}
