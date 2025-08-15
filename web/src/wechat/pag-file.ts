import { PAGFile as NativePAGFile } from '../pag-file';
import { destroyVerify } from '../utils/decorators';

@destroyVerify
export class PAGFile extends NativePAGFile {
  public static async load(data: ArrayBuffer) {
    return PAGFile.loadFromBuffer(data);
  }
}
