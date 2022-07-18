import { Black, Color } from './color';
import { Layer, LayerType } from './layer';
import { TimeRange } from './time-range';
import { verifyAndReturn, verifyFailed } from './utils/verify';

export class SolidLayer extends Layer {
  public solidColor: Color = Black;
  public width = 0;
  public height = 0;

  public type(): LayerType {
    return LayerType.Solid;
  }

  public excludeVaryingRanges(timeRanges: Array<TimeRange>) {
    super.excludeVaryingRanges(timeRanges);
  }

  public gotoFrame(frame: number) {
    super.gotoFrame(frame);
  }

  public verify(): boolean {
    if (!super.verify()) {
      verifyFailed();
      return false;
    }
    return verifyAndReturn(this.width > 0 && this.height > 0);
  }
}
