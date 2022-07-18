import { Layer, LayerType } from './layer';
import { Property } from './property';
import { TextDocument } from './text-document';
import { TextMoreOptions } from './text/text-more-options';
import { TextPathOptions } from './text/text-path-options';
import { TimeRange } from './time-range';

export class TextLayer extends Layer {
  public sourceText: Property<TextDocument> | undefined;
  public pathOption: TextPathOptions | undefined;
  public moreOption: TextMoreOptions | undefined;

  public type(): LayerType {
    return LayerType.Text;
  }

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_time: number): void {}

  public verify(): boolean {
    return false;
  }
}
