import { ZERO_ID, ZERO_TIME } from '../constant';
import { Color, White } from './color';
import { CompositionType } from '../codec/types';
import { TimeRange } from './time-range';
import { verifyAndReturn } from './utils/verify';

export class Composition {
  private static cacheIDCount = 1;

  /**
   * A unique identifier for this item.
   */
  public id: number = ZERO_ID;
  /**
   * The width of the Composition.
   */
  public width = 0;
  /**
   * The height of the item.
   */
  public height = 0;
  /**
   * The total duration of the item.
   */
  public duration: number = ZERO_TIME;
  /**
   * The frame rate of the Composition.
   */
  public frameRate = 30;
  /**
   * The background color of the composition.
   */
  public backgroundColor: Color = White;
  public cacheID = 0;

  public constructor() {
    this.cacheID = Composition.cacheIDCount;
    Composition.cacheIDCount += 1;
  }

  /**
   * The type of the Composition.
   */
  public type(): CompositionType {
    return CompositionType.Unknown;
  }

  /**
   * Returns the static time ranges of this composition.
   */
  public getStaticTimeRanges(): Array<TimeRange> {
    return [];
  }

  public verify(): boolean {
    return verifyAndReturn(this.width > 0 && this.height > 0 && this.duration > 0 && this.frameRate > 0);
  }
}
