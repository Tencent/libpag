export function interpolateFloat(a: number, b: number, t: number): number {
  return a + (b - a) * t;
}
