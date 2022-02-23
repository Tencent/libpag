import { ErrorCode, ErrorMap } from './error-map';

export class Log {
  public static log(...args: any[]) {
    console.log(...args);
  }

  public static warn(...args: any[]) {
    console.warn(...args);
  }

  public static error(error: string) {
    throw new Error(error);
  }

  public static errorByCode(errorCode: ErrorCode) {
    throw new Error(ErrorMap[errorCode]);
  }
}
