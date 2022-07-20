import { ByteData } from '../base/byte-data';
import { ByteArray } from './utils/byte-array';
import { memcpy } from './utils/byte-utils';

export const readByteDataWithStartCode = (byteArray: ByteArray) => {
  const length = byteArray.readEncodedUint32();
  const bytes = byteArray.readBytes(length);
  if (length === 0) throw new Error('Read start code with length 0!');
  const data = new ArrayBuffer(length + 4);
  const dataView = new DataView(data);
  dataView.setUint32(0, length);
  memcpy(data, 4, bytes.data(), 0, length);
  return new ByteData(new ByteArray(data), length + 4);
};
