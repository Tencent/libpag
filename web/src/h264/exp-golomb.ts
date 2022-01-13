export class ExpGolomb {
  private data: Uint8Array;
  private index: number;
  private bitLength: number;

  public constructor(data: Uint8Array) {
    this.data = data;
    this.index = 0;
    this.bitLength = data.byteLength * 8;
  }

  public get bitsAvailable() {
    return this.bitLength - this.index;
  }

  public readBits(size: number, moveIndex = true) {
    const result = this.getBits(size, this.index, moveIndex);
    return result;
  }

  public skipLZ() {
    let leadingZeroCount;
    for (leadingZeroCount = 0; leadingZeroCount < this.bitLength - this.index; ++leadingZeroCount) {
      if (this.getBits(1, this.index + leadingZeroCount, false) !== 0) {
        this.index += leadingZeroCount;
        return leadingZeroCount;
      }
    }
    return leadingZeroCount;
  }

  public readUEG() {
    const prefix = this.skipLZ();
    return this.readBits(prefix + 1) - 1;
  }

  public readUByte(numberOfBytes = 1) {
    return this.readBits(numberOfBytes * 8);
  }

  private getBits(size: number, offsetBits: number, moveIndex = true) {
    if (this.bitsAvailable < size) {
      return 0;
    }
    const offset = offsetBits % 8;
    const byte = this.data[(offsetBits / 8) | 0] & (0xff >>> offset);
    const bits = 8 - offset;
    if (bits >= size) {
      if (moveIndex) {
        this.index += size;
      }
      return byte >> (bits - size);
    }
    if (moveIndex) {
      this.index += bits;
    }
    const nextSize = size - bits;
    return (byte << nextSize) | this.getBits(nextSize, offsetBits + bits, moveIndex);
  }
}
