import { TagCode } from '../types';

import type { ByteArray } from '../utils/byte-array';

export interface TagHeader {
  code: TagCode;
  length: number;
}

export const readTagHeader = (byteBuffer: ByteArray): TagHeader => {
  const codeAndLength: number = byteBuffer.readUint16();
  let length: number = (codeAndLength & 63) >>> 0;
  const code: number = codeAndLength >> 6;
  if (length === 63) {
    length = byteBuffer.readUint32();
  }
  if (byteBuffer.context.tagLevel < code) {
    byteBuffer.context.tagLevel = code;
  }
  return { code, length };
};

export function readTags<T>(byteArray: ByteArray, parameter: T, reader: Function) {
  let header = readTagHeader(byteArray);
  while (header.code !== TagCode.End) {
    const tagBytes = byteArray.readBytes(header.length);
    reader(tagBytes, header.code, parameter);
    if (byteArray.context.tagLevel < tagBytes.context.tagLevel) {
      byteArray.context.tagLevel = tagBytes.context.tagLevel;
    }
    header = readTagHeader(byteArray);
  }
}
