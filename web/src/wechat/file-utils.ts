import type { wx } from './interfaces';

declare const wx: wx;

const fs = wx.getFileSystemManager();

export const touchDirectory = (path: string) => {
  try {
    fs.accessSync(path);
  } catch (e) {
    try {
      fs.mkdirSync(path);
    } catch (err) {
      console.error(e);
    }
  }
};

export const writeFile = (path: string, data: string | ArrayBuffer) => {
  try {
    fs.writeFileSync(path, data, 'utf8');
  } catch (e: any) {
    throw new Error(e);
  }
};

export const removeFile = (path: string) => {
  try {
    fs.accessSync(path);
    fs.unlinkSync(path);
  } catch (e) {
    console.error(e);
  }
};

export const clearDirectory = (path: string) => {
  try {
    const res = fs.readdirSync(path);
    if (res.length > 0) {
      res.forEach((file) => {
        fs.unlinkSync(`${path}${file}`);
      });
    }
    return true;
  } catch (e) {
    console.error(e);
    return false;
  }
};

export const checkFileExist = (path: string) =>
  new Promise((resolve) => {
    wx.getFileInfo({
      filePath: path,
      success() {
        resolve(true);
      },
      fail() {
        resolve(false);
      },
    });
  });
