export interface Task {
  fn: () => Promise<any>;
}

export class WebAssemblyQueue {
  private executing = false;
  private queue: Task[] = [];

  public exec(fn, scope, ...args) {
    return new Promise((resolve) => {
      const copyFn = async () => {
        if (!fn) {
          resolve(null);
          return;
        }
        const res = await fn.call(scope, ...args);
        resolve(res);
      };
      this.queue.push({
        fn: copyFn,
      });
    });
  }

  public start() {
    setInterval(() => {
      if (this.executing) return;
      this.execNextTask();
    }, 1);
  }

  private execNextTask() {
    if (this.queue.length < 1) {
      this.executing = false;
      return;
    }
    this.executing = true;
    const task = this.queue.shift();
    task
      .fn()
      .then(() => {
        this.execNextTask();
      })
      .catch(() => {
        this.execNextTask();
      });
  }
}
