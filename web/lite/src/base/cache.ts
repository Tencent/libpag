export class Cache {
  private static cacheID = 1;
  public static generateID(): number {
    this.cacheID += 1;
    return this.cacheID;
  }
}
