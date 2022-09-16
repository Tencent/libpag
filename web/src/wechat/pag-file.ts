import { PAGFile as NativePAGFile } from '../pag-file';
import { destroyVerify, wasmAwaitRewind } from '../utils/decorators';

@destroyVerify
@wasmAwaitRewind
export class PAGFile extends NativePAGFile {
  public static async load(data: ArrayBuffer) {
    return PAGFile.loadFromBuffer(data);
  }
}
