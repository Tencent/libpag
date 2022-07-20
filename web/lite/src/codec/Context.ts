import { Composition } from '../base/composition';

export class Context {
  public tagLevel = 0;
  public compositions: Array<Composition> = [];

  private errorMessages: string[] = [];

  public throwException(message: string) {
    this.errorMessages.push(message);
  }

  public releaseCompositions(): Array<Composition> {
    const compositions = this.compositions.slice();
    this.compositions = [];
    return compositions;
  }
}
