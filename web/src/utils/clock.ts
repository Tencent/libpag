let getTime: () => number;
try {
  getTime = performance.now.bind(performance);
} catch (e) {
  getTime = Date.now.bind(Date);
}

export class Clock {
  private startTime: number;
  private markers: { [key: string]: number };

  public constructor() {
    this.startTime = getTime();
    this.markers = {};
  }

  public reset() {
    this.startTime = getTime();
    this.markers = {};
  }

  public mark(key: string) {
    if (!key) {
      console.log('Clock.mark(): An empty marker name was specified!');
      return;
    }
    if (Object.keys(this.markers).find((markerKey) => markerKey === key)) {
      console.log(`Clock.mark(): The specified marker name '${key}' already exists!`);
      return;
    }
    this.markers[key] = getTime();
  }

  public measure(makerFrom: string, makerTo: string) {
    let start;
    let end;
    if (!makerFrom) {
      start = this.startTime;
    } else {
      if (!Object.keys(this.markers).find((markerKey) => markerKey === makerFrom)) {
        console.log(`Clock.measure(): The specified makerFrom '${makerFrom}' does not exist!`);
        return 0;
      }
      start = this.markers[makerFrom];
    }
    if (!makerTo) {
      end = getTime();
    } else {
      if (!Object.keys(this.markers).find((markerKey) => markerKey === makerTo)) {
        console.log(`Clock.measure(): The specified makerTo '${makerTo}' does not exist!`);
        return 0;
      }
      end = this.markers[makerTo];
    }
    return end - start;
  }
}
