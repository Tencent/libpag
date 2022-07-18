import { Context } from '../context';
import { ErrorMessage } from '../../base/utils/error-map';

const LENGTH_FOR_STORE_NUM_BITS = 5;

export class ByteArray {
  public context: Context;

  private readonly littleEndian: boolean;
  private dataView: DataView;
  private _position = 0;
  private bitPosition = 0;

  public constructor(buffer: ArrayBuffer, littleEndian?: boolean) {
    this.dataView = new DataView(buffer);
    this.littleEndian = !!littleEndian;
    this.context = new Context();
  }

  public get length(): number {
    return this.dataView.byteLength;
  }

  public get bytesAvailable(): number {
    return this.dataView.byteLength - this._position;
  }

  public data(): ArrayBuffer {
    return this.dataView.buffer;
  }

  public get position(): number {
    return this._position;
  }

  public alignWithBytes() {
    this.bitPosition = this._position * 8;
  }

  public readBoolean(): boolean {
    const value = this.dataView.getInt8(this._position);
    this._position += 1;
    this.positonChanged();
    return Boolean(value);
  }

  public readChar(): string {
    if (this._position >= this.length) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getInt8(this._position);
    this._position += 1;
    this.positonChanged();
    return String.fromCharCode(value);
  }

  public readUint8(): number {
    if (this._position >= this.length) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getUint8(this._position);
    this._position += 1;
    this.positonChanged();
    return value;
  }

  public readInt8(): number {
    if (this._position >= this.length) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getInt8(this._position);
    this._position += 1;
    this.positonChanged();
    return value;
  }

  public readInt16(): number {
    if (this._position >= this.length - 1) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getInt16(this._position, this.littleEndian);
    this._position += 2;
    this.positonChanged();
    return value;
  }

  public readUint16(): number {
    if (this._position >= this.length - 1) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getUint16(this._position, this.littleEndian);
    this._position += 2;
    this.positonChanged();
    return value;
  }

  public readInt24(): number {
    if (this._position >= this.length - 2) throw new Error(ErrorMessage.PAGDecodeError);
    const left = this.dataView.getInt16(this._position, this.littleEndian);
    const right = this.dataView.getInt8(this._position + 2);
    this._position += 3;
    this.positonChanged();
    return this.littleEndian ? left + 2 ** 16 * right : 2 ** 16 * left + right;
  }

  public readUint24(): number {
    if (this._position >= this.length - 2) throw new Error(ErrorMessage.PAGDecodeError);
    const left = this.dataView.getUint16(this._position, this.littleEndian);
    const right = this.dataView.getUint8(this._position + 2);
    this._position += 3;
    this.positonChanged();
    return this.littleEndian ? left + 2 ** 16 * right : 2 ** 16 * left + right;
  }

  public readInt32(): number {
    if (this._position >= this.length - 3) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getInt32(this._position, this.littleEndian);
    this._position += 4;
    this.positonChanged();
    return value;
  }

  public readUint32(): number {
    if (this._position >= this.length - 3) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getUint32(this._position, this.littleEndian);
    this._position += 4;
    this.positonChanged();
    return value;
  }

  public readInt64(): number {
    if (this._position >= this.length - 7) throw new Error(ErrorMessage.PAGDecodeError);
    const left = this.dataView.getInt32(this._position, this.littleEndian);
    const right = this.dataView.getInt32(this._position + 4, this.littleEndian);
    this._position += 8;
    this.positonChanged();
    return this.littleEndian ? left + 2 ** 32 * right : 2 ** 32 * left + right;
  }

  public readUint64(): number {
    if (this._position >= this.length - 7) throw new Error(ErrorMessage.PAGDecodeError);
    const left = this.dataView.getUint32(this._position, this.littleEndian);
    const right = this.dataView.getUint32(this._position + 4, this.littleEndian);
    this._position += 8;
    this.positonChanged();
    return this.littleEndian ? left + 2 ** 32 * right : 2 ** 32 * left + right;
  }

  public readFloat32(): number {
    if (this._position >= this.length - 3) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getFloat32(this._position, this.littleEndian);
    this._position += 4;
    this.positonChanged();
    return value;
  }

  public readDouble(): number {
    if (this._position >= this.length - 7) throw new Error(ErrorMessage.PAGDecodeError);
    const value = this.dataView.getFloat64(this._position, this.littleEndian);
    this._position += 8;
    this.positonChanged();
    return value;
  }

  public readUTF8String(): string {
    if (this._position >= this.length) throw new Error(ErrorMessage.PAGDecodeError);
    let encoded = '';
    let dataLength = 0;
    for (let i = this._position; i < this.length; i++) {
      if (this.dataView.getUint8(i) === 0) {
        break;
      }
      encoded += `%${this.dataView.getUint8(i).toString(16)}`;
      dataLength += 1;
    }
    this._position += dataLength;
    this.positonChanged();
    return decodeURIComponent(encoded);
  }

  public readEncodedUint32(): number {
    const valueMask = 127;
    const hasNext = 128;
    let value = 0;
    let byte = 0;
    for (let i = 0; i < 32; i += 7) {
      if (this._position >= this.length) {
        throw Error('readEncodedUint32 End of file was encountered.');
      }
      byte = this.dataView.getUint8(this._position);
      this._position += 1;
      value |= (byte & valueMask) << i;
      if ((byte & hasNext) === 0) {
        break;
      }
    }
    this.positonChanged();
    return value;
  }

  public readEncodeInt32(): number {
    const data = this.readEncodedUint32();
    const value = data >> 1;
    return (data & 1) > 0 ? -value : value;
  }

  public readEncodedUint64(): number {
    const valueMask = 127;
    const hasNext = 128;
    let value = 0;
    let byte = 0;
    for (let i = 0; i < 64; i += 7) {
      if (this._position >= this.length) {
        throw Error('readEncodedUint64 End of file was encountered.');
      }
      byte = this.dataView.getUint8(this._position);
      this._position += 1;
      value |= (byte & valueMask) << i;
      if ((byte & hasNext) === 0) {
        break;
      }
    }
    this.positonChanged();
    return value;
  }

  public readEncodeInt64(): number {
    const data = this.readEncodedUint64();
    const value = data << 0;
    return (data & 1) > 0 ? -value : value;
  }

  public readBytes(length?: number): ByteArray {
    const len = length || this.length - this._position;
    if (this._position > this.length - len) throw new Error(ErrorMessage.PAGDecodeError);
    const newBuffer = this.dataView.buffer.slice(this._position, this._position + len);
    this._position += len;
    this.positonChanged();
    return new ByteArray(newBuffer, this.littleEndian);
  }

  public readUBits(numBits: number): number {
    const bitMasks: number[] = [0, 1, 3, 7, 15, 31, 63, 127, 255];
    let value = 0;
    if (this.bitPosition > this.length * 8 - numBits) throw new Error(ErrorMessage.PAGDecodeError);
    let pos = 0;
    while (pos < numBits) {
      const bytePosition = Math.floor(this.bitPosition * 0.125);
      const bitPosition = this.bitPosition % 8;
      let byte = this.dataView.getUint8(bytePosition) >> bitPosition;
      const bitLength = Math.min(8 - bitPosition, numBits - pos);
      byte &= bitMasks[bitLength];
      value |= byte << pos;
      pos += bitLength;
      this.bitPosition += bitLength;
    }
    this.bitPositionChanged();
    return value;
  }

  public readBits(numBits: number): number {
    let value = this.readUBits(numBits);
    value <<= 32 - numBits;
    const data = value << 0;
    return data >> (32 - numBits);
  }

  public readNumBits(): number {
    return this.readUBits(LENGTH_FOR_STORE_NUM_BITS) + 1;
  }

  public readInt32List(count: number): number[] {
    const numBits = this.readNumBits();
    const value = new Array(count);
    for (let i = 0; i < count; i++) {
      value[i] = this.readBits(numBits);
    }
    return value;
  }

  public readUint32List(count: number): number[] {
    const numBits = this.readNumBits();
    const value = new Array(count);
    for (let i = 0; i < count; i++) {
      value[i] = this.readUBits(numBits);
    }
    return value;
  }

  public readBitBoolean() {
    return this.readUBits(1) !== 0;
  }

  public readFloatList(count: number, precision: number): number[] {
    const numBits = this.readNumBits();
    const value = new Array(count);
    for (let i = 0; i < count; i++) {
      value[i] = this.readBits(numBits) * precision;
    }
    return value;
  }

  private bitPositionChanged() {
    this._position = Math.ceil(this.bitPosition * 0.125);
  }

  private positonChanged() {
    this.bitPosition = this._position * 8;
  }
}
