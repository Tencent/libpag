import { PathVerb } from '../constant';
import { Point } from './point';

export class Path {
  public verbs: Array<PathVerb> = [];
  public points: Array<Point> = [];

  public close(): void {}

  public reverse(): void {}

  public clear(): void {}

  public interpolate(_path: Path, _result: Path, _t: number) {}
}
