export enum ErrorCode {
  PagFileDataEmpty,
  PagFontDataEmpty,
  PagImageDataEmpty,
  VideoReaderUndefined,
  VideoReaderHeadersExisted,
  VideoReaderH264HeaderError,
  NaluEmpty,
  FrameEmpty,
  ImageCodecUndefined,
  FontNamesUnloaded,
  UnsupportCanvas2D,
}

export const ErrorMap = {
  [ErrorCode.PagFileDataEmpty]: 'Initialize PAGFile data not be empty!',
  [ErrorCode.PagFontDataEmpty]: 'Initialize PAGFont data not be empty!',
  [ErrorCode.PagImageDataEmpty]: 'Initialize PAGImage data not be empty!',
  [ErrorCode.VideoReaderUndefined]: 'Video reader instance undefined!',
  [ErrorCode.VideoReaderHeadersExisted]: 'Video reader headers is existed!',
  [ErrorCode.VideoReaderH264HeaderError]: 'Video reader headers need has sps and pps!',
  [ErrorCode.NaluEmpty]: 'Nal units not be empty！',
  [ErrorCode.FrameEmpty]: 'Frames not be empty！',
  [ErrorCode.ImageCodecUndefined]: 'Image codec instance undefined!',
  [ErrorCode.FontNamesUnloaded]: 'Target fontNames unloaded!',
  [ErrorCode.UnsupportCanvas2D]: 'Unsupport Canvas2D!',
};
