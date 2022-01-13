import { WebAssemblyQueue } from '../src/utils/queue';

describe('测试队列', () => {
  const webAssemblyQueue = new WebAssemblyQueue();
  webAssemblyQueue.start();

  test('顺序执行', async () => {
    const list = [];
    const fn = (arg) => {
      list.push(arg);
    };
    webAssemblyQueue.exec(fn, this, 1);
    webAssemblyQueue.exec(fn, this, 2);
    webAssemblyQueue.exec(fn, this, 3).then(() => {
      expect(list).toEqual([1, 2, 3]);
    });
  });

  test('await 执行', async () => {
    const list = [];
    const fn = (arg) => {
      list.push(arg);
      return arg;
    };
    webAssemblyQueue.exec(fn, this, 1);
    const res = await webAssemblyQueue.exec(fn, this, 2);
    expect(res).toBe(2);
    expect(list).toEqual([1, 2]);
    webAssemblyQueue.exec(fn, this, 3).then(() => {
      expect(list).toEqual([1, 2, 3]);
    });
  });

  test('Promist 执行', async () => {
    const list = [];
    const fn = (arg) => {
      return new Promise((resolve) => {
        list.push(arg);
        setTimeout(() => {
          resolve(arg);
        }, 100);
      });
    };
    webAssemblyQueue.exec(fn, this, 1);
    const res = await webAssemblyQueue.exec(fn, this, 2);
    expect(res).toBe(2);
    expect(list).toEqual([1, 2]);
    webAssemblyQueue.exec(fn, this, 3).then(() => {
      expect(list).toEqual([1, 2, 3]);
    });
  });
});
