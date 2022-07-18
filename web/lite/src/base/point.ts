export class Point {
  public x: number;
  public y: number;
  public constructor(x: number, y: number) {
    this.x = x;
    this.y = y;
  }
}

export const ZERO_POINT = new Point(0, 0);
