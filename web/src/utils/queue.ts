export interface Task {
  fn: () => Promise<any>;
}

export class WebAssemblyQueue {
  private executing = false;
  private queue: Task[] = [];

  public exec<T>(fn: (...args: any[]) => any, scope: any, ...args: any[]): Promise<T> {
    return new Promise((resolve, reject) => {
      const copyFn = async () => {
        if (!fn) {
          reject(new Error('Function is null!'));
          return;
        }
        try {
          const res = await fn.call(scope, ...args);
          resolve(res);
        } catch (e) {
          reject(e);
        }
      };
      this.queue.push({
        fn: copyFn,
      });
      if (this.executing) return;
      this.execNextTask();
    });
  }

  private execNextTask() {
    if (this.queue.length < 1) {
      this.executing = false;
      return;
    }
    this.executing = true;
    const task = this.queue.shift() as Task;
    task.fn().then(() => {
      this.execNextTask();
    });
  }
}
