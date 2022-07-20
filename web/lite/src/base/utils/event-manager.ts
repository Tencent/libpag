export type Listener = (...payload: any) => void;

interface ListenersMap {
  [propName: string]: Listener[];
}

export class EventManager {
  private listenersMap: ListenersMap;

  public constructor() {
    this.listenersMap = {};
  }

  public on(eventName: string, listener: Listener) {
    if (this.listenersMap[eventName] === undefined) {
      this.listenersMap[eventName] = [];
    }
    this.listenersMap[eventName].push(listener);
    return;
  }

  public off(eventName: string, listener?: Listener) {
    const listenerList: Listener[] = this.listenersMap[eventName];
    if (listenerList === undefined) return;
    if (listener === undefined) {
      delete this.listenersMap[eventName];
      return;
    }
    const index = listenerList.findIndex((fn: Listener) => fn === listener);
    listenerList.splice(index, 1);
    return;
  }

  public emit(eventName: string, ...payload: any): boolean {
    const listenerList: Listener[] = this.listenersMap[eventName];
    if (listenerList === undefined || listenerList.length < 1) return false;
    for (const listener of listenerList) {
      listener(...payload);
    }
    return true;
  }
}
