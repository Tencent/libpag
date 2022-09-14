import { PAGFont as NativePAGFont } from '../pag-font';

export class PAGFont extends NativePAGFont {
  public static async registerFont(family: string, data: File) {
    throw new Error('please use wx.loadFontFace to register font on wechat miniprogram');
  }
}
