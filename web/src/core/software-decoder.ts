export const enum DecoderResult {
  /**
   * The calling is successful.
   */
  Success = 0,
  /**
   * Output is not available in this state, need more input buffers.
   */
  TryAgainLater = -1,
  /**
   * The calling fails.
   */
  Error = -2,
}

export declare class YUVBuffer {
  public data: Uint8Array[];
  public lineSize: number[];
}

export class SoftwareDecoder {
  public onConfigure(headers: Uint8Array[], mimeType: string, width: number, height: number): boolean {
    throw new Error('Virtual functions.');
  }

  public onSendBytes(bytes: Uint8Array, timestamp: number): DecoderResult {
    throw new Error('Virtual functions.');
  }

  public onDecodeFrame(): DecoderResult {
    throw new Error('Virtual functions.');
  }

  public onEndOfStream(): DecoderResult {
    throw new Error('Virtual functions.');
  }

  public onFlush() {
    throw new Error('Virtual functions.');
  }

  public onRenderFrame(): YUVBuffer {
    throw new Error('Virtual functions.');
  }
}
