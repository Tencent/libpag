import { IPHONE, MACOS } from './ua';

const windows = [
  'Microsoft YaHei',
  'Microsoft JhengHei',
  'Hiragino Sans GB',
  'SimSun',
  'FangSong',
  'KaiTi',
  'NSimSun',
  'SimHei',
  'DengXian',
  'Adobe Song Std',
  'Adobe Fangsong Std',
  'Adobe Heiti Std',
  'Adobe Kaiti Std',
  'Times New Roman',
  'Comic Sans MS',
  'Courier New',
  'Calibri',
  'Impact',
  'Microsoft Sans Serif',
  'Symbol',
  'Tahoma',
  'Trebuchet MS',
  'Verdana',
];
const cocoa = [
  'PingFang SC',
  'Apple SD Gothic Neo',
  'Helvetica',
  'Myanmar Sangam MN',
  'Thonburi',
  'Mishafi',
  'Menlo',
  'Kailasa',
  'Kefa',
  'Kohinoor Telugu',
  'Hiragino Maru Gothic ProN',
];

export const defaultFontNames = (() => {
  if (MACOS || IPHONE) {
    return ['emoji'].concat(...cocoa);
  } else {
    return ['emoji'].concat(...windows);
  }
})();
