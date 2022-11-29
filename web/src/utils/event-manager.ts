export type Listener<K> = (event: K) => void;

export class EventManager<K, V> {
  private listenerMap: Map<keyof K, Array<(event: V) => void>>;

  public constructor() {
    this.listenerMap = new Map();
  }

  public on(eventName: keyof K, listener: Listener<V>) {
    let listenerList: Listener<V>[] = [];
    if (this.listenerMap.has(eventName)) {
      listenerList = this.listenerMap.get(eventName) as Array<(event: V) => void>;
    }
    listenerList.push(listener);
    this.listenerMap.set(eventName, listenerList);
  }

  public off(eventName: keyof K, listener?: Listener<V>): boolean {
    if (!this.listenerMap.has(eventName)) return false;
    const listenerList: Listener<V>[] = this.listenerMap.get(eventName) as Listener<V>[];
    if (listenerList.length === 0) return false;
    if (!listener) {
      this.listenerMap.delete(eventName);
      return true;
    }
    const index = listenerList.indexOf(listener);
    if (index === -1) return false;
    listenerList.splice(index, 1);
    return true;
  }

  public emit(eventName: keyof K, event: V): boolean {
    if (!this.listenerMap.has(eventName)) return false;
    const listenerList: Listener<V>[] = this.listenerMap.get(eventName) as Listener<V>[];
    if (listenerList.length === 0) return false;
    listenerList.forEach((listener: Listener<V>) => listener(event));
    return true;
  }
}
