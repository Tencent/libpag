import { H264Frame } from './h264-parser';
import { NALU } from './nalu';
import { Mp4Generator } from './mp4-generator';
import { NALU_HEADER_LENGTH } from '../constant';
import { concatUint8Arrays } from '../utils/common';
import { ErrorCode } from '../utils/error-map';
import { Log } from '../utils/log';

export interface Sample {
  units: NALU[];
  size: number;
  keyFrame: boolean;
}

export interface Mp4Sample {
  index: number;
  size: number;
  duration: number;
  compositionTimeOffset: number;
  flags: {
    isLeading: number;
    isDependedOn: number;
    hasRedundancy: number;
    degradationPriority: number;
    isKeyFrame: boolean;
    isNonSyncSample: number;
    dependsOn: number;
  };
}

export interface Mp4Track {
  id: number;
  type: string;
  len: number;
  fragmented: boolean;
  sps: Uint8Array[];
  pps: Uint8Array[];
  width: number;
  height: number;
  timescale: number;
  duration: number;
  samples: Mp4Sample[];
  ptsList: number[];
  fps: number;
  implicitOffset: number;
}

const NALU_BASE_TYPES = [NALU.sps, NALU.pps, NALU.idr, NALU.ndr]; // 只解析基础 NALU
const SEQUENCE_NUMBER = 1;
const BASE_MEDIA_DECODE_TIME = 0;
const BASE_MEDIA_TIME_SCALE = 1000;

const getImplicitOffset = (ptsList: number[]) => {
  const offsetList = ptsList.map((pts, index) => pts - index).filter((offset) => offset < 0);
  if (offsetList.length < 1) return 0;
  return Math.abs(Math.min(...offsetList));
};

let trackId = 1;

export class H264Remuxer {
  public static getTrackID() {
    const id = trackId;
    trackId += 1;
    return id;
  }

  public static remux(frames: H264Frame[], headers: Uint8Array[], width, height, frameRate, ptsList) {
    if (frames.length < 1) Log.errorByCode(ErrorCode.FrameEmpty);
    if (headers.length < 2) Log.errorByCode(ErrorCode.VideoReaderH264HeaderError);
    const remuxer = new H264Remuxer();
    remuxer.mp4track.timescale = BASE_MEDIA_TIME_SCALE;
    remuxer.mp4track.duration = (frames.length / frameRate) * BASE_MEDIA_TIME_SCALE;
    remuxer.mp4track.fps = frameRate;
    remuxer.mp4track.ptsList = ptsList;
    remuxer.mp4track.width = width;
    remuxer.mp4track.height = height;
    remuxer.mp4track.sps = [headers[0].subarray(NALU_HEADER_LENGTH)];
    remuxer.mp4track.pps = [headers[1].subarray(NALU_HEADER_LENGTH)];
    remuxer.mp4track.implicitOffset = getImplicitOffset(ptsList);

    for (const frame of frames) {
      const units = frame.units.filter((unit) => NALU_BASE_TYPES.includes(unit.nalUnitType));
      if (units.length < 1) continue;
      const size = units.reduce((pre, cur) => pre + cur.getSize(), 0);
      remuxer.mp4track.len += size;
      remuxer.samples.push({
        units,
        size,
        keyFrame: frame.isKeyFrame,
      });
    }
    return remuxer;
  }

  public mp4track: Mp4Track = {
    id: H264Remuxer.getTrackID(),
    type: 'video',
    len: 0,
    fragmented: true,
    sps: null,
    pps: null,
    width: 0,
    height: 0,
    timescale: 0,
    duration: 0,
    samples: [],
    ptsList: [],
    fps: 0,
    implicitOffset: 0,
  };
  public samples: Sample[] = [];

  public convertMp4() {
    const payload = this.getPayload();
    if (!payload) return;
    const mp4Generator = new Mp4Generator();
    const ftyp = mp4Generator.ftyp();
    const moov = mp4Generator.moov([this.mp4track], this.mp4track.duration, this.mp4track.timescale);
    const moof = mp4Generator.moof(SEQUENCE_NUMBER, BASE_MEDIA_DECODE_TIME, this.mp4track);
    const mdat = mp4Generator.mdat(payload);

    return concatUint8Arrays([ftyp, moov, moof, mdat]);
  }

  private getPayload() {
    const payload = new Uint8Array(this.mp4track.len);
    const sampleDelta = this.mp4track.duration / this.samples.length;
    let offset = 0;
    let count = 0;

    for (const sample of this.samples) {
      const { units } = sample;
      const mp4Sample: Mp4Sample = {
        index: count,
        size: sample.size,
        duration: sampleDelta,
        compositionTimeOffset: (this.mp4track.ptsList[count] + this.mp4track.implicitOffset - count) * sampleDelta,
        flags: {
          isLeading: 0,
          isDependedOn: 0,
          hasRedundancy: 0,
          degradationPriority: 0,
          isKeyFrame: sample.keyFrame,
          isNonSyncSample: sample.keyFrame ? 0 : 1,
          dependsOn: sample.keyFrame ? 2 : 1,
        },
      };

      for (const unit of units) {
        payload.set(unit.getData(), offset);
        offset += unit.getSize();
      }
      this.mp4track.samples.push(mp4Sample);
      count += 1;
    }

    if (!this.mp4track.samples.length) return;
    return payload;
  }
}
