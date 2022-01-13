import { Mp4Track } from './h264-remuxer';

const CORRECTION_UTC = 2082873600; // 1904-01-01 与 1970-1-1 相差的秒数

const decimal2HexadecimalArray = (payload: number) => [
  payload >> 24,
  (payload >> 16) & 0xff,
  (payload >> 8) & 0xff,
  payload & 0xff,
];

const getCharCode = (name: string) => [name.charCodeAt(0), name.charCodeAt(1), name.charCodeAt(2), name.charCodeAt(3)];

export class Mp4Generator {
  private static hdlrTypes = {
    video: new Uint8Array([
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
    audio: new Uint8Array([
      0x00, // version 0
      0x00,
      0x00,
      0x00, // flags
      0x00,
      0x00,
      0x00,
      0x00, // pre_defined
      0x73,
      0x6f,
      0x75,
      0x6e, // handler_type: 'soun'
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
      0x53,
      0x6f,
      0x75,
      0x6e,
      0x64,
      0x48,
      0x61,
      0x6e,
      0x64,
      0x6c,
      0x65,
      0x72,
      0x00, // name: 'SoundHandler'
    ]),
  };

  private static fullbox = new Uint8Array([
    0x00, // version
    0x00,
    0x00,
    0x00, // flags
    0x00,
    0x00,
    0x00,
    0x00, // entry_count
  ]);

  private static stsc = Mp4Generator.fullbox;

  private static stco = Mp4Generator.fullbox;

  private static stsz = new Uint8Array([
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
  ]);

  private static vmhd = new Uint8Array([
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
  ]);

  private static smhd = new Uint8Array([
    0x00, // version
    0x00,
    0x00,
    0x00, // flags
    0x00,
    0x00, // balance
    0x00,
    0x00, // reserved
  ]);

  private static stsd = new Uint8Array([
    0x00, // version 0
    0x00,
    0x00,
    0x00, // flags
    0x00,
    0x00,
    0x00,
    0x01, // entry_count
  ]);

  private static box(type: any[], ...payload: Uint8Array[]): Uint8Array {
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
  }

  private types = {
    avc1: [],
    avcC: [],
    btrt: [],
    ctts: [],
    dinf: [],
    dref: [],
    edts: [],
    elst: [],
    esds: [],
    ftyp: [],
    hdlr: [],
    mdat: [],
    mdhd: [],
    mdia: [],
    mfhd: [],
    minf: [],
    moof: [],
    moov: [],
    mp4a: [],
    mvex: [],
    mvhd: [],
    sdtp: [],
    stbl: [],
    stco: [],
    stsc: [],
    stsd: [],
    stsz: [],
    stts: [],
    stss: [],
    tfdt: [],
    tfhd: [],
    traf: [],
    trak: [],
    trun: [],
    trex: [],
    tkhd: [],
    vmhd: [],
    smhd: [],
  };

  private dinf: Uint8Array;

  public constructor() {
    Object.keys(this.types).forEach((type) => {
      if (Object.prototype.hasOwnProperty.call(this.types, type)) {
        this.types[type] = getCharCode(type);
      }
    });

    const dref = new Uint8Array([
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
    ]);
    this.dinf = Mp4Generator.box(this.types.dinf, Mp4Generator.box(this.types.dref, dref));
  }

  public ftyp() {
    return Mp4Generator.box(
      this.types.ftyp,
      new Uint8Array(getCharCode('isom')), // major_brand
      new Uint8Array([0, 0, 0, 1]), // minor_version
      new Uint8Array(getCharCode('isom')), // compatible_brands
      new Uint8Array(getCharCode('iso2')),
      new Uint8Array(getCharCode('avc1')),
      new Uint8Array(getCharCode('mp41')),
    );
  }

  public moov(tracks: Mp4Track[], duration: number, timescale: number) {
    let i = tracks.length;
    const boxes = [];

    while (i) {
      i -= 1;
      boxes[i] = this.trak(tracks[i]);
    }

    return Mp4Generator.box.apply(
      null,
      [this.types.moov, this.mvhd(timescale, duration)].concat(boxes).concat(this.mvex(tracks)),
    );
  }

  public moof(sequence_number: number, baseMediaDecodeTime: number, track: Mp4Track) {
    return Mp4Generator.box(this.types.moof, this.mfhd(sequence_number), this.traf(baseMediaDecodeTime, track));
  }

  public mdat(data: Uint8Array) {
    return Mp4Generator.box(this.types.mdat, data);
  }

  private hdlr(type: string) {
    return Mp4Generator.box(this.types.hdlr, Mp4Generator.hdlrTypes[type]);
  }

  private mdhd(timescale: number, duration: number) {
    return Mp4Generator.box(
      this.types.mdhd,
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        ...decimal2HexadecimalArray(Math.floor(Date.now() / 1000 + CORRECTION_UTC)), // creation_time
        ...decimal2HexadecimalArray(Math.floor(Date.now() / 1000 + CORRECTION_UTC)), // modification_time
        ...decimal2HexadecimalArray(timescale), // timescale
        ...decimal2HexadecimalArray(duration), // duration
        0x55,
        0xc4, // 'und' language (undetermined)
        0x00,
        0x00,
      ]),
    );
  }

  private mdia(track: Mp4Track) {
    return Mp4Generator.box(
      this.types.mdia,
      this.mdhd(track.timescale, track.duration),
      this.hdlr(track.type),
      this.minf(track),
    );
  }

  private mfhd(sequenceNumber: number) {
    return Mp4Generator.box(
      this.types.mfhd,
      new Uint8Array([
        0x00,
        0x00,
        0x00,
        0x00, // flags
        sequenceNumber >> 24,
        (sequenceNumber >> 16) & 0xff,
        (sequenceNumber >> 8) & 0xff,
        sequenceNumber & 0xff, // sequence_number
      ]),
    );
  }

  private minf(track: Mp4Track) {
    if (track.type === 'audio') {
      return Mp4Generator.box(
        this.types.minf,
        Mp4Generator.box(this.types.smhd, Mp4Generator.smhd),
        this.dinf,
        this.stbl(track),
      );
    }
    return Mp4Generator.box(
      this.types.minf,
      Mp4Generator.box(this.types.vmhd, Mp4Generator.vmhd),
      this.dinf,
      this.stbl(track),
    );
  }

  private mvex(tracks: Mp4Track[]) {
    let i = tracks.length;
    const boxes = [];

    while (i) {
      i -= 1;
      boxes[i] = this.trex(tracks[i]);
    }
    return Mp4Generator.box.apply(null, [this.types.mvex].concat(boxes));
  }

  private mvhd(timescale: number, duration: number) {
    const bytes = new Uint8Array([
      0x00, // version 0
      0x00,
      0x00,
      0x00, // flags
      ...decimal2HexadecimalArray(Math.floor(Date.now() / 1000 + CORRECTION_UTC)), // creation_time
      ...decimal2HexadecimalArray(Math.floor(Date.now() / 1000 + CORRECTION_UTC)), // modification_time
      ...decimal2HexadecimalArray(timescale), // timescale
      ...decimal2HexadecimalArray(duration), // duration
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
    ]);
    return Mp4Generator.box(this.types.mvhd, bytes);
  }

  private sdtp(track: Mp4Track) {
    const samples = track.samples || [];
    const bytes = new Uint8Array(4 + samples.length);
    let flags;
    let i;
    // leave the full box header (4 bytes) all zero
    // write the sample table
    for (i = 0; i < samples.length; i++) {
      flags = samples[i].flags;
      bytes[i + 4] = (flags.dependsOn << 4) | (flags.isDependedOn << 2) | flags.hasRedundancy;
    }

    return Mp4Generator.box(this.types.sdtp, bytes);
  }

  private stbl(track: Mp4Track) {
    return Mp4Generator.box(
      this.types.stbl,
      this.stsd(track),
      this.stts(track),
      this.ctts(track),
      this.stss(track),
      Mp4Generator.box(this.types.stsc, Mp4Generator.stsc),
      Mp4Generator.box(this.types.stsz, Mp4Generator.stsz),
      Mp4Generator.box(this.types.stco, Mp4Generator.stco),
    );
  }

  private avc1(track: Mp4Track) {
    let sps = [];
    let pps = [];
    let i;
    let data;
    let len;
    // assemble the SPSs

    for (i = 0; i < track.sps.length; i++) {
      data = track.sps[i];
      len = data.byteLength;
      sps.push((len >>> 8) & 0xff);
      sps.push(len & 0xff);
      sps = sps.concat(Array.prototype.slice.call(data)); // SPS
    }

    // assemble the PPSs
    for (i = 0; i < track.pps.length; i++) {
      data = track.pps[i];
      len = data.byteLength;
      pps.push((len >>> 8) & 0xff);
      pps.push(len & 0xff);
      pps = pps.concat(Array.prototype.slice.call(data));
    }

    const avcc = Mp4Generator.box(
      this.types.avcC,
      new Uint8Array(
        [
          0x01, // version
          sps[3], // profile
          sps[4], // profile compat
          sps[5], // level
          0xfc | 3, // lengthSizeMinusOne, hard-coded to 4 bytes
          0xe0 | track.sps.length, // 3bit reserved (111) + numOfSequenceParameterSets
        ]
          .concat(sps)
          .concat([
            track.pps.length, // numOfPictureParameterSets
          ])
          .concat(pps),
      ),
    ); // "PPS"
    const { width } = track;
    const { height } = track;
    return Mp4Generator.box(
      this.types.avc1,
      new Uint8Array([
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
        (width >> 8) & 0xff,
        width & 0xff, // width
        (height >> 8) & 0xff,
        height & 0xff, // height
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
      ]), // pre_defined = -1
      avcc,
    );
  }

  private esds(track) {
    const configlen = track.config.byteLength;
    const data = new Uint8Array(26 + configlen + 3);
    data.set([
      0x00, // version 0
      0x00,
      0x00,
      0x00, // flags
      0x03, // descriptor_type
      0x17 + configlen, // length
      0x00,
      0x01, // es_id
      0x00, // stream_priority
      0x04, // descriptor_type
      0x0f + configlen, // length
      0x40, // codec : mpeg4_audio
      0x15, // stream_type
      0x00,
      0x00,
      0x00, // buffer_size
      0x00,
      0x00,
      0x00,
      0x00, // maxBitrate
      0x00,
      0x00,
      0x00,
      0x00, // avgBitrate
      0x05, // descriptor_type
      configlen,
    ]);
    data.set(track.config, 26);
    data.set([0x06, 0x01, 0x02], 26 + configlen);
    return data;
  }

  private mp4a(track) {
    const { audiosamplerate } = track;
    return Mp4Generator.box(
      this.types.mp4a,
      new Uint8Array([
        0x00,
        0x00,
        0x00, // reserved
        0x00,
        0x00,
        0x00, // reserved
        0x00,
        0x01, // data_reference_index
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        0x00,
        track.channelCount, // channelcount
        0x00,
        0x10, // sampleSize:16bits
        0x00,
        0x00, // pre_defined
        0x00,
        0x00, // reserved2
        (audiosamplerate >> 8) & 0xff,
        audiosamplerate & 0xff, //
        0x00,
        0x00,
      ]),
      Mp4Generator.box(this.types.esds, this.esds(track)),
    );
  }

  private stsd(track: Mp4Track) {
    if (track.type === 'audio') {
      return Mp4Generator.box(this.types.stsd, Mp4Generator.stsd, this.mp4a(track));
    }
    return Mp4Generator.box(this.types.stsd, Mp4Generator.stsd, this.avc1(track));
  }

  private tkhd(track) {
    return Mp4Generator.box(
      this.types.tkhd,
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x01, // flags
        ...decimal2HexadecimalArray(Math.floor(Date.now() / 1000 + CORRECTION_UTC)), // creation_time
        ...decimal2HexadecimalArray(Math.floor(Date.now() / 1000 + CORRECTION_UTC)), // modification_time
        ...decimal2HexadecimalArray(track.id), // track_ID
        0x00,
        0x00,
        0x00,
        0x00, // reserved
        ...decimal2HexadecimalArray(track.duration), // duration
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
        (track.volume >> 0) & 0xff,
        (((track.volume % 1) * 10) >> 0) & 0xff, // track volume // FIXME
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

  private traf(baseMediaDecodeTime: number, track: Mp4Track) {
    const sampleDependencyTable = this.sdtp(track);
    const { id } = track;
    return Mp4Generator.box(
      this.types.traf,
      Mp4Generator.box(
        this.types.tfhd,
        new Uint8Array([
          0x00, // version 0
          0x00,
          0x00,
          0x00, // flags
          id >> 24,
          (id >> 16) & 0xff,
          (id >> 8) & 0xff,
          id & 0xff, // track_ID
        ]),
      ),
      Mp4Generator.box(
        this.types.tfdt,
        new Uint8Array([
          0x00, // version 0
          0x00,
          0x00,
          0x00, // flags
          baseMediaDecodeTime >> 24,
          (baseMediaDecodeTime >> 16) & 0xff,
          (baseMediaDecodeTime >> 8) & 0xff,
          baseMediaDecodeTime & 0xff, // baseMediaDecodeTime
        ]),
      ),
      this.trun(
        track,
        sampleDependencyTable.length +
          16 + // tfhd
          16 + // tfdt
          8 + // traf header
          16 + // mfhd
          8 + // moof header
          8,
      ), // mdat header
      sampleDependencyTable,
    );
  }

  /**
   * Generate a track box.
   * @param track {object} a track definition
   * @return {Uint8Array} the track box
   */
  private trak(track: Mp4Track) {
    track.duration = track.duration || 0xffffffff;
    return Mp4Generator.box(this.types.trak, this.tkhd(track), this.edts(track), this.mdia(track));
  }

  private edts(track: Mp4Track) {
    return Mp4Generator.box(this.types.edts, this.elst(track));
  }

  private elst(track: Mp4Track) {
    const sampleCount = track.samples.length;
    const sampleDelta = track.duration / sampleCount;

    const buffer = [
      0x00, // version
      0x00,
      0x00,
      0x00, // flags
      ...decimal2HexadecimalArray(1), // entry_count
      ...decimal2HexadecimalArray(track.duration),
      ...decimal2HexadecimalArray(track.implicitOffset * sampleDelta),
      0x00,
      0x01, // media_rate_integer
      0x00,
      0x00, // media_rate_integer
    ];
    return Mp4Generator.box(this.types.elst, new Uint8Array(buffer));
  }

  private trex(track: Mp4Track) {
    const { id } = track;
    return Mp4Generator.box(
      this.types.trex,
      new Uint8Array([
        0x00, // version 0
        0x00,
        0x00,
        0x00, // flags
        id >> 24,
        (id >> 16) & 0xff,
        (id >> 8) & 0xff,
        id & 0xff, // track_ID
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

  private trun(track: Mp4Track, offset: number) {
    const samples = track.samples || [];
    const len = samples.length;
    const arraylen = 12 + 16 * len;
    const array = new Uint8Array(arraylen);
    let i;
    let sample;
    let duration;
    let size;
    let flags;
    let compositionTimeOffset;
    const dataOffset = offset + 8 + arraylen;
    array.set(
      [
        0x00, // version 0
        0x00,
        0x0f,
        0x01, // flags
        (len >>> 24) & 0xff,
        (len >>> 16) & 0xff,
        (len >>> 8) & 0xff,
        len & 0xff, // sample_count
        (dataOffset >>> 24) & 0xff,
        (dataOffset >>> 16) & 0xff,
        (dataOffset >>> 8) & 0xff,
        dataOffset & 0xff, // data_offset
      ],
      0,
    );
    for (i = 0; i < len; i++) {
      sample = samples[i];
      duration = sample.duration;
      size = sample.size;
      flags = sample.flags;
      compositionTimeOffset = sample.compositionTimeOffset;
      array.set(
        [
          (duration >>> 24) & 0xff,
          (duration >>> 16) & 0xff,
          (duration >>> 8) & 0xff,
          duration & 0xff, // sample_duration
          (size >>> 24) & 0xff,
          (size >>> 16) & 0xff,
          (size >>> 8) & 0xff,
          size & 0xff, // sample_size
          (flags.isLeading << 2) | flags.dependsOn,
          (flags.isDependedOn << 6) | (flags.hasRedundancy << 4) | (flags.paddingValue << 1) | flags.isNonSyncSample,
          flags.degradationPriority & (0xf0 << 8),
          flags.degradationPriority & 0x0f, // sample_flags
          (compositionTimeOffset >>> 24) & 0xff,
          (compositionTimeOffset >>> 16) & 0xff,
          (compositionTimeOffset >>> 8) & 0xff,
          compositionTimeOffset & 0xff, // sample_composition_time_offset
        ],
        12 + 16 * i,
      );
    }
    return Mp4Generator.box(this.types.trun, array);
  }

  private stts(track: Mp4Track) {
    const sampleCount = track.samples.length;
    const sampleDelta = Math.floor(track.duration / sampleCount);
    const buffer = [
      0x00, // version
      0x00,
      0x00,
      0x00, // flags
      ...decimal2HexadecimalArray(1), // entry_count
      ...decimal2HexadecimalArray(sampleCount), // sample_count
      ...decimal2HexadecimalArray(sampleDelta), // sample_offset
    ];
    return Mp4Generator.box(this.types.stts, new Uint8Array(buffer));
  }

  private ctts(track: Mp4Track) {
    const sampleCount = track.ptsList.length;
    const sampleDelta = Math.floor(track.duration / sampleCount);
    const buffer = [
      0x00, // version
      0x00,
      0x00,
      0x00, // flags
      ...decimal2HexadecimalArray(sampleCount), // entry_count
    ];
    for (let i = 0; i < sampleCount; i++) {
      buffer.push(...decimal2HexadecimalArray(1)); // sample_count
      const dts = i * sampleDelta;
      const pts = (track.ptsList[i] + track.implicitOffset) * sampleDelta;
      buffer.push(...decimal2HexadecimalArray(pts - dts)); // sample_offset
    }
    return Mp4Generator.box(this.types.ctts, new Uint8Array(buffer));
  }

  private stss(track: Mp4Track) {
    const iFrames = track.samples.filter((sample) => sample.flags.isKeyFrame).map((sample) => sample.index + 1);
    const buffer = [
      0x00, // version
      0x00,
      0x00,
      0x00, // flags
      ...decimal2HexadecimalArray(iFrames.length),
    ];
    for (const iFrame of iFrames) {
      buffer.push(...decimal2HexadecimalArray(iFrame));
    }
    return Mp4Generator.box(this.types.stss, new Uint8Array(buffer));
  }
}
