export class Ratio {
  public numerator = 1;
  public denominator = 1;

  public constructor(numerator: number, denominator: number) {
    this.numerator = numerator;
    this.denominator = denominator;
  }

  public value(): number {
    return this.numerator / this.denominator;
  }
}

export const DefaultRatio = new Ratio(1, 1);
