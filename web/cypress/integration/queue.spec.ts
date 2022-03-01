import { WebAssemblyQueue } from '../../src/utils/queue';

describe('Test queue', () => {
  const webAssemblyQueue = new WebAssemblyQueue();
  webAssemblyQueue.start();

  it('Sequence exec', async () => {
    const list = [];
    const fn = (arg) => {
      list.push(arg);
    };
    webAssemblyQueue.exec(fn, this, 1);
    webAssemblyQueue.exec(fn, this, 2);
    webAssemblyQueue.exec(fn, this, 3).then(() => {
      expect(list).to.eql([1, 2, 3]);
    });
  });

  it('Await exec', async () => {
    const list = [];
    const fn = (arg) => {
      list.push(arg);
      return arg;
    };
    webAssemblyQueue.exec(fn, this, 1);
    const res = await webAssemblyQueue.exec(fn, this, 2);
    expect(res).to.eq(2);
    expect(list).to.eql([1, 2]);
    webAssemblyQueue.exec(fn, this, 3).then(() => {
      expect(list).to.eql([1, 2, 3]);
    });
  });

  it('Promise exec', async () => {
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
    expect(res).to.eq(2);
    expect(list).to.eql([1, 2]);
    webAssemblyQueue.exec(fn, this, 3).then(() => {
      expect(list).to.eql([1, 2, 3]);
    });
  });
});
