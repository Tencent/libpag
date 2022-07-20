import { Composition } from './composition';
import { verifyAndReturn } from './utils/verify';

export class Sequence {
  public composition: Composition | undefined = undefined;
  public id = 0;
  public width = 0;
  public height = 0;
  public frameRate = 0;
  public frameCount = 0;
  public isKeyFrameFlags: Array<boolean> = [];

  public verify() {
    return verifyAndReturn(this.composition !== undefined && this.width > 0 && this.height > 0 && this.frameRate > 0);
  }
}
