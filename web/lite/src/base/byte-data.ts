import { ByteArray } from '../codec/utils/byte-array';

export class ByteData {
  public data: ByteArray;
  public length = 0;

  public constructor(data: ByteArray, length: number) {
    this.data = data;
    this.length = length;
  }
}
