import { PAGModule } from './binding';
import { PAGFont } from './pag-font';
import { PAGLayer } from './pag-layer';
import { destroyVerify, wasmAwaitRewind } from './utils/decorators';

import type { Color, TextDocument } from './types';

@destroyVerify
@wasmAwaitRewind
export class PAGTextLayer extends PAGLayer {
  public static make(
    duration: number,
    text: string,
    fontSize: number,
    fontFamily: string,
    fontStyle: string,
  ): PAGTextLayer;
  public static make(duration: number, textDocumentHandle: TextDocument): PAGTextLayer;
  public static make(
    duration: number,
    text: string | TextDocument,
    fontSize = 0,
    fontFamily = '',
    fontStyle = '',
  ): PAGTextLayer {
    if (typeof text === 'string') {
      return new PAGTextLayer(PAGModule._PAGTextLayer._Make(duration, text, fontSize, fontFamily, fontStyle));
    } else {
      return new PAGTextLayer(PAGModule._PAGTextLayer._Make(duration, text));
    }
  }

  /**
   * Returns the text layer’s fill color.
   */
  public fillColor(): Color {
    return this.wasmIns._fillColor() as Color;
  }
  /**
   * Set the text layer’s fill color.
   */
  public setFillColor(value: Color) {
    this.wasmIns._setFillColor(value);
  }
  /**
   * Returns the text layer's font.
   */
  public font(): PAGFont {
    return new PAGFont(this.wasmIns._font());
  }
  /**
   * Set the text layer's font.
   */
  public setFont(pagFont: PAGFont) {
    this.wasmIns._setFont(pagFont.wasmIns);
  }
  /**
   * Returns the text layer's font size.
   */
  public fontSize(): number {
    return this.wasmIns._fontSize() as number;
  }
  /**
   * Set the text layer's font size.
   */
  public setFontSize(size: number) {
    this.wasmIns._setFontSize(size);
  }
  /**
   * Returns the text layer's stroke color.
   */
  public strokeColor(): Color {
    return this.wasmIns._strokeColor() as Color;
  }
  /**
   * Set the text layer's stroke color.
   */
  public setStrokeColor(value: Color) {
    this.wasmIns._setStrokeColor(value);
  }
  /**
   * Returns the text layer's text.
   */
  public text(): string {
    return this.wasmIns._text() as string;
  }
  /**
   * Set the text layer's text.
   */
  public setText(text: string) {
    this.wasmIns._setText(text);
  }
  /**
   * Reset the text layer to its default text data.
   */
  public reset() {
    this.wasmIns._reset();
  }
}
