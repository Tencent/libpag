import { ByteArray } from '../codec/utils/byte-array';
import { ByteData } from './byte-data';

export class VideoFrame {
  public isKeyframe = false;
  public frame = 0;
  public fileBytes: ByteData = new ByteData(new ByteArray(new ArrayBuffer(0)), 0);
}
