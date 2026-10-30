/* eslint prefer-arrow-callback: ["error", { "allowNamedFunctions": true }] */
/* global describe it expect */
import { WebAssemblyQueue } from '../../src/utils/queue';

describe('Queue', () => {
  it('Sequence exec', async () => {
    const webAssemblyQueue = new WebAssemblyQueue();
    const list: number[] = [];
    function append(arg: number) {
      list.push(arg);
    }
    await Promise.all([
      webAssemblyQueue.exec(append, null, 1),
      webAssemblyQueue.exec(append, null, 2),
      webAssemblyQueue.exec(append, null, 3),
    ]);
    expect(list).to.eql([1, 2, 3]);
  });

  it('Await exec', async () => {
    const webAssemblyQueue = new WebAssemblyQueue();
    const list: number[] = [];
    function append(arg: number) {
      list.push(arg);
      return arg;
    }
    const first = webAssemblyQueue.exec(append, null, 1);
    const second = webAssemblyQueue.exec(append, null, 2);
    expect(await Promise.all([first, second])).to.eql([1, 2]);
    expect(list).to.eql([1, 2]);
    expect(await webAssemblyQueue.exec(append, null, 3)).to.eq(3);
    expect(list).to.eql([1, 2, 3]);
  });

  it('Promise exec', async () => {
    const webAssemblyQueue = new WebAssemblyQueue();
    const list: number[] = [];
    function append(arg: number) {
      return new Promise<number>(function completeTask(resolve) {
        setTimeout(function finishTask() {
          list.push(arg);
          resolve(arg);
        }, (4 - arg) * 10);
      });
    }
    const first = webAssemblyQueue.exec(append, null, 1);
    const second = webAssemblyQueue.exec(append, null, 2);
    expect(await Promise.all([first, second])).to.eql([1, 2]);
    expect(list).to.eql([1, 2]);
    expect(await webAssemblyQueue.exec(append, null, 3)).to.eq(3);
    expect(list).to.eql([1, 2, 3]);
  });
});
