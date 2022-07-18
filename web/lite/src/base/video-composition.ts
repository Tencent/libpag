import { Composition } from './composition';
import { CompositionType } from '../codec/types';
import { TimeRange } from './time-range';
import { VideoSequence } from './video-sequence';
import { verifyFailed } from './utils/verify';

export class VideoComposition extends Composition {
  public hasAlpha = false;
  public sequences: Array<VideoSequence> = [];

  private staticTimeRanges: Array<TimeRange> = [];
  private staticTimeRangeUpdated = false;

  public type(): CompositionType {
    return CompositionType.Video;
  }

  public getStaticTimeRanges(): Array<TimeRange> {
    if (!this.staticTimeRangeUpdated) {
      this.staticTimeRangeUpdated = true;
      this.updateStaticTimeRanges();
    }
    return this.staticTimeRanges;
  }

  public updateStaticTimeRanges(): void {
    if (this.duration <= 1) return;
    if (this.sequences.length > 0) {
      let sequence = this.sequences[0];
      for (let i = 1; i < this.sequences.length; i++) {
        const item = this.sequences[i];
        if (item.frameRate > sequence.frameRate) sequence = item;
      }
      const timeScale = this.frameRate / sequence.frameRate;
      for (const timeRange of sequence.staticTimeRanges) {
        timeRange.start = Math.round(timeRange.start * timeScale);
        timeRange.end = Math.round(timeRange.end * timeScale);
        this.staticTimeRanges.push(timeRange);
      }
    } else {
      const range: TimeRange = { start: 0, end: this.duration - 1 };
      this.staticTimeRanges.push(range);
    }
  }

  public hasImageContent(): boolean {
    return true;
  }

  public verify(): boolean {
    if (!super.verify() || this.sequences.length <= 0) {
      verifyFailed();
      return false;
    }
    for (const sequence of this.sequences) {
      if (!sequence || !sequence.verify()) {
        verifyFailed();
        return false;
      }
    }
    return true;
  }
}
