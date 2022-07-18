import type { ByteData } from '../base/byte-data';
import type { VideoSequence } from '../base/video-sequence';

const CORRECTION_UTC = 2082873600; // 1904-01-01 与 1970-1-1 相差的秒数
const DEFAULT_VOLUME = 1;

export interface MP4Flags {
  isLeading: number;
  isDependedOn: number;
  hasRedundancy: number;
  degradPrio: number;
  isNonSync: number;
  dependsOn: number;
  isKeyFrame: boolean;
}

export interface MP4Sample {
  index: number;
  size: number;
  duration: number;
  cts: number;
  flags: MP4Flags;
}

export interface MP4Track {
  id: number;
  type: string;
  len: number;
  sps: ByteData[];
  pps: ByteData[];
  width: number;
  height: number;
  timescale: number;
  duration: number;
  samples: MP4Sample[];
  pts: number[];
  implicitOffset: number;
}

export interface BoxParam {
  offset: number;
  timescale: number;
  duration: number;
  sequenceNumber: number;
  nalusBytesLen: number;
  baseMediaDecodeTime: number;
  track: MP4Track;
  videoSequence: VideoSequence;
  tracks: MP4Track[];
}

const getCharCode = (name: string) => {
  const res = [];
  for (let index = 0; index < name.length; index++) {
    res.push(name.charCodeAt(index));
  }
  return res;
};

const toHexadecimal = (num: number) => [num >> 24, (num >> 16) & 0xff, (num >> 8) & 0xff, num & 0xff];

const makeBox = (type: number[], ...payload: Uint8Array[]): Uint8Array => {
  let size = 8;
  let i = payload.length;
  const len = i;
  while (i) {
    i -= 1;
    size += payload[i].byteLength;
  }
  const result = new Uint8Array(size);
  result[0] = (size >> 24) & 0xff;
  result[1] = (size >> 16) & 0xff;
  result[2] = (size >> 8) & 0xff;
  result[3] = size & 0xff;
  result.set(type, 4);
  // copy the payload into the result
  for (i = 0, size = 8; i < len; ++i) {
    // copy payload[i] array @ offset size
    result.set(payload[i], size);
    size += payload[i].byteLength;
  }
  return result;
};

export class MP4Generator {
  private param: BoxParam;

  public constructor(boxParam: BoxParam) {
    this.param = boxParam;
  }

  public ftyp() {
    return makeBox(
      getCharCode('ftyp'),
      new Uint8Array(getCharCode('isom')), // major_brand
      new Uint8Array([0, 0, 0, 1]), // minor_version
      new Uint8Array(getCharCode('isom')), // compatible_brands
      new Uint8Array(getCharCode('iso2')),
      new Uint8Array(getCharCode('avc1')),
      new Uint8Array(getCharCode('mp41')),
    );
  }

  public moov() {
    const traks = this.param.tracks.map((track) => this.trak(track)).reverse();
    return makeBox(getCharCode('moov'), this.mvhd(), ...traks, this.mvex());
  }

  public moof() {
    return makeBox(getCharCode('moof'), this.mfhd(), this.traf());
  }

  public mdat() {
    const buffer = new Uint8Array(this.param.track.len);
    let offset = 0;
    this.param.videoSequence.headers.forEach((header) => {
      buffer.set(new Uint8Array(header.data.data()), offset);
      offset += header.length;
    });

    this.param.videoSequence.frames.forEach((frame, index) => {
      buffer.set(new Uint8Array(frame.fileBytes.data.data()), offset);
      offset += frame.fileBytes.length;
    });
    return makeBox(getCharCode('mdat'), buffer);
  }

  private mvhd() {
    return makeBox(
      getCharCode('mvhd'),
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        ...toHexadecimal(CORRECTION_UTC), // creation_time
        ...toHexadecimal(CORRECTION_UTC), // modification_time
        ...toHexadecimal(this.param.timescale), // timescale
        ...toHexadecimal(this.param.duration), // duration
        0x00,
        0x01,
        0x00,
        0x00, // 1.0 rate
        0x01,
        0x00, // 1.0 volume
        0x00,
        0x00, // reserved
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x40,
        0x00,
        0x00,
        0x00, // transformation: unity matrix
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // pre_defined
        0x00,
        0x00,
        0x00,
        0x02, // next_track_ID
      ]),
    );
  }

  private trak(track: MP4Track) {
    return makeBox(getCharCode('trak'), this.tkhd(track), this.edts(track), this.mdia(track));
  }

  private tkhd(track: MP4Track) {
    return makeBox(
      getCharCode('tkhd'),
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x01, // flags
        ...toHexadecimal(CORRECTION_UTC), // creation_time
        ...toHexadecimal(CORRECTION_UTC), // modification_time
        ...toHexadecimal(track.id), // track_ID
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        ...toHexadecimal(track.duration), // duration
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        0x00,
        0x00, // layer
        0x00,
        0x00, // alternate_group
        (DEFAULT_VOLUME >> 0) & 0xff,
        (((DEFAULT_VOLUME % 1) * 10) >> 0) & 0xff, // track volume // FIXME
        0x00,
        0x00, // reserved
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x40,
        0x00,
        0x00,
        0x00, // transformation: unity matrix
        (track.width >> 8) & 0xff,
        track.width & 0xff,
        0x00,
        0x00, // width
        (track.height >> 8) & 0xff,
        track.height & 0xff,
        0x00,
        0x00, // height
      ]),
    );
  }

  private edts(track: MP4Track) {
    return makeBox(getCharCode('edts'), this.elst(track));
  }

  private elst(track: MP4Track) {
    return makeBox(
      getCharCode('elst'),
      new Uint8Array([
        0x00, // version
        0x00,
        0x00,
        0x00, // flags
        ...toHexadecimal(1), // entry_count
        ...toHexadecimal(track.duration),
        ...toHexadecimal(track.implicitOffset * Math.floor(track.duration / track.samples.length)),
        0x00,
        0x01, // media_rate_integer
        0x00,
        0x00, // media_rate_integer
      ]),
    );
  }

  private mdia(track: MP4Track) {
    return makeBox(getCharCode('mdia'), this.mdhd(), this.hdlr(), this.minf(track));
  }

  private mdhd() {
    return makeBox(
      getCharCode('mdhd'),
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        ...toHexadecimal(CORRECTION_UTC), // creation_time
        ...toHexadecimal(CORRECTION_UTC), // modification_time
        ...toHexadecimal(this.param.timescale), // timescale
        ...toHexadecimal(0), // duration
        0x55,
        0xc4, // 'und' language (undetermined)
        0x00,
        0x00,
      ]),
    );
  }

  private hdlr() {
    return makeBox(
      getCharCode('hdlr'),
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        0x00,
        0x00,
        0x00,
        0x00, // pre_defined
        0x76,
        0x69,
        0x64,
        0x65, // handler_type: 'vide'
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        0x56,
        0x69,
        0x64,
        0x65,
        0x6f,
        0x48,
        0x61,
        0x6e,
        0x64,
        0x6c,
        0x65,
        0x72,
        0x00, // name: 'VideoHandler'
      ]),
    );
  }

  private minf(track: MP4Track) {
    return makeBox(getCharCode('minf'), this.vmhd(), this.dinf(), this.stbl(track));
  }

  private vmhd() {
    return makeBox(
      getCharCode('vmhd'),
      new Uint8Array([
        0x00, // version
        0x00,
        0x00,
        0x01, // flags
        0x00,
        0x00, // graphicsmode
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // opcolor
      ]),
    );
  }

  private dinf() {
    return makeBox(getCharCode('dinf'), this.dref());
  }

  private dref() {
    return makeBox(
      getCharCode('dref'),
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        0x00,
        0x00,
        0x00,
        0x01, // entry_count
        0x00,
        0x00,
        0x00,
        0x0c, // entry_size
        0x75,
        0x72,
        0x6c,
        0x20, // 'url' type
        0x00, // version 0
        0x00,
        0x00,
        0x01, // entry_flags
      ]),
    );
  }

  private stbl(track: MP4Track) {
    return makeBox(
      getCharCode('stbl'),
      this.stsd(track),
      this.stts(track),
      this.ctts(track),
      this.stss(track),
      this.stsc(),
      this.stsz(),
      this.stco(),
    );
  }

  private stsd(track: MP4Track) {
    const data = [
      0x00, // version 0
      0x00,
      0x00,
      0x00, // flags
      0x00,
      0x00,
      0x00,
      0x01, // entry_count
    ];
    return makeBox(getCharCode('stsd'), new Uint8Array(data), this.avc1(track));
  }

  private avc1(track: MP4Track) {
    const data = [
      0x00,
      0x00,
      0x00, // reserved
      0x00,
      0x00,
      0x00, // reserved
      0x00,
      0x01, // data_reference_index
      0x00,
      0x00, // pre_defined
      0x00,
      0x00, // reserved
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00, // pre_defined
      (track.width >> 8) & 0xff,
      track.width & 0xff, // width
      (track.height >> 8) & 0xff,
      track.height & 0xff, // height
      0x00,
      0x48,
      0x00,
      0x00, // horizresolution
      0x00,
      0x48,
      0x00,
      0x00, // vertresolution
      0x00,
      0x00,
      0x00,
      0x00, // reserved
      0x00,
      0x01, // frame_count
      0x12,
      0x62,
      0x69,
      0x6e,
      0x65, // binelpro.ru
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00, // compressorname
      0x00,
      0x18, // depth = 24
      0xff,
      0xff,
    ];
    return makeBox(getCharCode('avc1'), new Uint8Array(data), this.avcc(track));
  }

  private avcc(track: MP4Track) {
    let sps: number[] = [];
    let pps: number[] = [];

    track.sps.forEach((byteData) => {
      const len = byteData.length - 4;
      sps.push((len >>> 8) & 0xff);
      sps.push(len & 0xff);
      sps = sps.concat(Array.prototype.slice.call(new Uint8Array(byteData.data.data(), 4)));
    });
    track.pps.forEach((byteData) => {
      const len = byteData.length - 4;
      pps.push((len >>> 8) & 0xff);
      pps.push(len & 0xff);
      pps = pps.concat(Array.prototype.slice.call(new Uint8Array(byteData.data.data(), 4)));
    });

    const data = [
      0x01, // version
      sps[3], // profile
      sps[4], // profile compat
      sps[5], // level
      0xfc | 3, // lengthSizeMinusOne, hard-coded to 4 bytes
      0xe0 | track.sps.length, // 3bit reserved (111) + numOfSequenceParameterSets
    ]
      .concat(sps)
      .concat([track.pps.length]) // numOfPictureParameterSets
      .concat(pps);

    return makeBox(getCharCode('avcC'), new Uint8Array(data));
  }

  private stts(track: MP4Track) {
    return makeBox(
      getCharCode('stts'),
      new Uint8Array([
        0x00, // version
        0x00,
        0x00,
        0x00, // flags
        ...toHexadecimal(1), // entry_count
        ...toHexadecimal(track.samples.length), // sample_count
        ...toHexadecimal(Math.floor(track.duration / track.samples.length)), // sample_offset
      ]),
    );
  }

  private ctts(track: MP4Track) {
    const sampleCount = track.pts.length;
    const sampleDelta = Math.floor(track.duration / sampleCount);
    const data = [
      0x00, // version
      0x00,
      0x00,
      0x00, // flags
      ...toHexadecimal(sampleCount), // entry_count
    ];
    for (let i = 0; i < sampleCount; i++) {
      data.push(...toHexadecimal(1)); // sample_count
      const dts = i * sampleDelta;
      const pts = (track.pts[i] + track.implicitOffset) * sampleDelta;
      data.push(...toHexadecimal(pts - dts)); // sample_offset
    }
    return makeBox(getCharCode('ctts'), new Uint8Array(data));
  }

  private stss(track: MP4Track) {
    const iFrames = track.samples.filter((sample) => sample.flags.isKeyFrame).map((sample) => sample.index + 1);
    const data = [
      0x00, // version
      0x00,
      0x00,
      0x00, // flags
      ...toHexadecimal(iFrames.length),
    ];
    iFrames.forEach((iFrame) => {
      data.push(...toHexadecimal(iFrame));
    });
    return makeBox(getCharCode('stss'), new Uint8Array(data));
  }

  private stsc() {
    return makeBox(
      getCharCode('stsc'),
      new Uint8Array([
        0x00, // version
        0x00,
        0x00,
        0x00, // flags
        0x00,
        0x00,
        0x00,
        0x00, // entry_count
      ]),
    );
  }

  private stsz() {
    return makeBox(
      getCharCode('stsz'),
      new Uint8Array([
        0x00, // version
        0x00,
        0x00,
        0x00, // flags
        0x00,
        0x00,
        0x00,
        0x00, // sample_size
        0x00,
        0x00,
        0x00,
        0x00, // sample_count
      ]),
    );
  }

  private stco() {
    return makeBox(
      getCharCode('stco'),
      new Uint8Array([
        0x00, // version
        0x00,
        0x00,
        0x00, // flags
        0x00,
        0x00,
        0x00,
        0x00, // entry_count
      ]),
    );
  }

  private mvex() {
    const trexs = this.param.tracks.map((track) => this.trex(track)).reverse();
    return makeBox(getCharCode('mvex'), ...trexs);
  }

  private trex(track: MP4Track) {
    return makeBox(
      getCharCode('trex'),
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        track.id >> 24,
        (track.id >> 16) & 0xff,
        (track.id >> 8) & 0xff,
        track.id & 0xff, // track_ID
        0x00,
        0x00,
        0x00,
        0x01, // default_sample_description_index
        0x00,
        0x00,
        0x00,
        0x00, // default_sample_duration
        0x00,
        0x00,
        0x00,
        0x00, // default_sample_size
        0x00,
        0x01,
        0x00,
        0x01, // default_sample_flags
      ]),
    );
  }

  private mfhd() {
    return makeBox(
      getCharCode('mfhd'),
      new Uint8Array([
        0x00,
        0x00,
        0x00,
        0x00, // flags
        this.param.sequenceNumber >> 24,
        (this.param.sequenceNumber >> 16) & 0xff,
        (this.param.sequenceNumber >> 8) & 0xff,
        this.param.sequenceNumber & 0xff, // sequence_number
      ]),
    );
  }

  private traf() {
    const sdtp = this.sdtp();
    this.param.offset = sdtp.length + 72; // tfhd + tfdt + traf header + mfhd + moof header + mdat header
    return makeBox(getCharCode('traf'), this.tfhd(), this.tfdt(), this.trun(), sdtp);
  }

  private tfhd() {
    return makeBox(
      getCharCode('tfhd'),
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        ...toHexadecimal(this.param.track.id), // track_ID
      ]),
    );
  }

  private tfdt() {
    return makeBox(
      getCharCode('tfdt'),
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        ...toHexadecimal(this.param.baseMediaDecodeTime), // baseMediaDecodeTime
      ]),
    );
  }

  private trun() {
    const samples = this.param.track.samples || [];
    const len = samples.length;
    const arraylen = 12 + 16 * len;
    this.param.offset += 8 + arraylen;
    const data = [
      0x00, // version 0
      0x00,
      0x0f,
      0x01, // flags
      ...toHexadecimal(len), // sample_count
      ...toHexadecimal(this.param.offset), // data_offset
    ];
    this.param.track.samples.forEach((sample) => {
      const paddingValue = 0;
      const { duration, size, flags, cts } = sample;
      data.push(...toHexadecimal(duration)); // sample_duration
      data.push(...toHexadecimal(size)); // sample_size
      data.push((flags.isLeading << 2) | flags.dependsOn);
      data.push((flags.isDependedOn << 6) | (flags.hasRedundancy << 4) | (paddingValue << 1) | flags.isNonSync);
      data.push(flags.degradPrio & (0xf0 << 8));
      data.push(flags.degradPrio & 0x0f); // sample_flags
      data.push(...toHexadecimal(cts)); // sample_composition_time_offset
    });
    return makeBox(getCharCode('trun'), new Uint8Array(data));
  }

  private sdtp() {
    const buffer = new Uint8Array(4 + this.param.track.samples.length);
    this.param.track.samples.forEach((sample, index) => {
      buffer[index + 4] = (sample.flags.dependsOn << 4) | (sample.flags.isDependedOn << 2) | sample.flags.hasRedundancy;
    });
    return makeBox(getCharCode('sdtp'), buffer);
  }
}
