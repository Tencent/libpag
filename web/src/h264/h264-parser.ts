import { ExpGolomb } from './exp-golomb';
import { NALU } from './nalu';
import { NALU_HEADER_LENGTH } from '../constant';
import { ErrorCode } from '../utils/error-map';
import { Log } from '../utils/log';

export interface H264Frame {
  units: Array<NALU>;
  isKeyFrame: boolean;
}

export const getH264Frames = (nalus: Uint8Array[]): H264Frame[] => {
  if (nalus.length < 1) Log.errorByCode(ErrorCode.NaluEmpty);
  const frames: H264Frame[] = [];
  let units: Array<NALU> = [];
  let isKeyFrame = false;
  let isVcl = false;
  for (const nalu of nalus) {
    const unit = new NALU(nalu.subarray(NALU_HEADER_LENGTH));
    if (unit.nalUnitType === NALU.idr || unit.nalUnitType === NALU.ndr) {
      parseHeader(unit);
    }
    if (units.length && isVcl && (unit.firstMbInSlice || !unit.isVcl)) {
      frames.push({ units, isKeyFrame });
      units = [];
      isKeyFrame = false;
      isVcl = false;
    }
    units.push(unit);
    isKeyFrame = isKeyFrame || unit.isKeyframe();
    isVcl = isVcl || unit.isVcl;
  }
  if (units.length > 0) {
    if (isVcl) {
      frames.push({ units, isKeyFrame });
    } else {
      const last = frames.length - 1;
      frames[last].units = frames[last].units.concat(units);
    }
  }
  return frames;
};

const parseHeader = (nalu: NALU) => {
  const decoder = new ExpGolomb(nalu.payload);
  decoder.readUByte(); // skip NALu type
  nalu.firstMbInSlice = decoder.readUEG() === 0;
  nalu.sliceType = decoder.readUEG();
};
