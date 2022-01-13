export class NALU {
  public static ndr = 1;
  public static idr = 5;
  public static sei = 6;
  public static sps = 7;
  public static pps = 8;
  public static aud = 9;

  public static get TYPES() {
    return {
      [NALU.ndr]: 'NDR',
      [NALU.idr]: 'IDR',
      [NALU.sei]: 'SEI',
      [NALU.sps]: 'SPS',
      [NALU.pps]: 'PPS',
      [NALU.aud]: 'AUD',
    };
  }

  public static getNaluType(nalu: NALU) {
    if (nalu.nalUnitType in NALU.TYPES) {
      return NALU.TYPES[nalu.nalUnitType];
    }
    return 'UNKNOWN';
  }

  public payload: Uint8Array;
  public nalRefIdc: number;
  public nalUnitType: number;
  public isVcl: boolean;
  public sliceType: number;
  public firstMbInSlice: boolean;

  public constructor(data: Uint8Array) {
    this.payload = data;
    this.nalRefIdc = (this.payload[0] & 0x60) >> 5; // nal_ref_idc
    this.nalUnitType = this.payload[0] & 0x1f;
    this.isVcl = this.nalUnitType === NALU.ndr || this.nalUnitType === NALU.idr;
    this.sliceType = 0; // slice_type
    this.firstMbInSlice = false; // first_mb_in_slice
  }

  public toString() {
    return `${NALU.TYPES[this.nalUnitType]}: NRI: ${this.nalRefIdc}`;
  }

  public isKeyframe() {
    return this.nalUnitType === NALU.idr;
  }

  public getPayloadSize() {
    return this.payload.byteLength;
  }

  public getSize() {
    return 4 + this.getPayloadSize();
  }

  public getData() {
    const result = new Uint8Array(this.getSize());
    const view = new DataView(result.buffer);
    view.setUint32(0, this.getSize() - 4);
    result.set(this.payload, 4);
    return result;
  }
}
