import { Color } from '../base/color';
import { Composition } from '../base/composition';
import { Ratio } from '../base/ratio';
import { ByteArray } from './utils/byte-array';
import { Point } from '../base/point';
import { Layer } from '../base/layer';
import { Mask } from '../base/mask';

export const SPATIAL_PRECISION = 0.05;
export const BEZIER_PRECISION = 0.005;

export const readRatio = (byteArray: ByteArray): Ratio => {
  const numeratorValue: number = byteArray.readEncodeInt32();
  const denominatorValue: number = byteArray.readEncodedUint32();
  const ration: Ratio = new Ratio(numeratorValue, denominatorValue);
  return ration;
};

export const readColor = (byteArray: ByteArray): Color => {
  const redNum: number = byteArray.readUint8();
  const greenNum: number = byteArray.readUint8();
  const blueNum: number = byteArray.readUint8();
  const color: Color = { red: redNum, green: greenNum, blue: blueNum };
  return color;
};

export const readTime = (byteArray: ByteArray): number => byteArray.readEncodedUint64();

export const readFloat = (byteArray: ByteArray): number => byteArray.readFloat32();

export const readBoolean = (byteArray: ByteArray): boolean => byteArray.readBitBoolean();

export const readEnum = (byteArray: ByteArray): number => byteArray.readUint8();

export const readID = (byteArray: ByteArray): number => byteArray.readEncodedUint32();

export const readLayerID = (byteArray: ByteArray): Layer => {
  const id = byteArray.readEncodedUint32();
  if (id === 0) throw new Error('Layer ID is 0');
  const layer: Layer = new Layer();
  layer.id = id;
  return layer;
};

export const readMaskID = (byteArray: ByteArray): Mask => {
  const id: number = byteArray.readEncodedUint32();
  if (id === 0) throw new Error('Mask ID is 0');
  const mask: Mask = new Mask();
  mask.id = id;
  return mask;
};

export const readCompositionID = (byteArray: ByteArray): Composition => {
  const id: number = byteArray.readEncodedUint32();
  if (id === 0) throw new Error('Composition ID is 0');
  const composition: Composition = new Composition();
  composition.id = id;
  return composition;
};

export const readString = (byteArray: ByteArray): string => byteArray.readUTF8String();

export function ReadOpacity(byteArray: ByteArray): number {
  return byteArray.readUint8();
}

export const readPoint = (byteArray: ByteArray): Point => {
  const x: number = byteArray.readFloat32();
  const y: number = byteArray.readFloat32();
  return new Point(x, y);
};
