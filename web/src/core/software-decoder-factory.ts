import { SoftwareDecoder } from './software-decoder';

export class SoftwareDecoderFactory {
  public createSoftwareDecoder(): SoftwareDecoder {
    throw new Error('Virtual functions.');
  }
}
