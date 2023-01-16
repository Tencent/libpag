import type { WorkerMessage } from './worker';

let messageCount = 0;
const generateMessageName = (name: string) => `${name}_${messageCount++}`;

export interface WorkerInterface {
  postMessage: (message: any, transfer: Transferable[]) => void;
  addEventListener: (type: string, listener: (event: MessageEvent) => void) => void;
  removeEventListener: (type: string, listener: (event: MessageEvent) => void) => void;
}

export const postMessage = <T>(
  worker: WorkerInterface,
  message: WorkerMessage,
  callback: (...args: any[]) => T,
  transfer: (OffscreenCanvas | Transferable)[] = [],
): Promise<T> => {
  return new Promise((resolve) => {
    const name = generateMessageName(message.name);
    const handle = (event: MessageEvent<WorkerMessage>) => {
      if (event.data.name === name) {
        worker.removeEventListener('message', handle);
        resolve(callback(...event.data.args));
      }
    };
    worker.addEventListener('message', handle);
    worker.postMessage({ name, args: message.args }, transfer);
  });
};
