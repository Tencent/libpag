import { JPAG } from 'libpag.so';

export class PAG {
  /**
   * Get SDK version information.
   */
  static SDKVersion(): string {
    return JPAG.SDKVersion();
  }
}