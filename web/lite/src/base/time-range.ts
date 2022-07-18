export interface TimeRange {
  start: number;
  end: number;
}

export function subtractFromTimeRanges(timeRanges: Array<TimeRange>, startTime: number, endTime: number) {
  if (endTime < startTime) {
    return;
  }
  const size = timeRanges.length;
  for (let i = size - 1; i >= 0; i--) {
    const timeRange = timeRanges[i];
    if (timeRange.end < startTime || timeRange.start > endTime) {
      continue;
    }
    if (timeRange.start < startTime && timeRange.end > endTime) {
      const range = { start: endTime + 1, end: timeRange.end };
      timeRange.end = startTime - 1;
      if (range.end > range.start) {
        timeRanges.splice(i + 1, 0, range);
      }
      if (timeRange.end <= timeRange.start) {
        timeRanges.splice(i, 1);
      }
      break;
    }
    if (timeRange.start >= startTime && timeRange.end <= endTime) {
      timeRanges.splice(i, 1);
    } else if (timeRange.start < startTime) {
      timeRange.end = startTime - 1;
      if (timeRange.end <= timeRange.start) {
        timeRanges.splice(i, 1);
      }
    } else {
      timeRange.start = endTime + 1;
      if (timeRange.end <= timeRange.start) {
        timeRanges.splice(i, 1);
      }
    }
  }
}

export function splitTimeRangesAt(timeRanges: Array<TimeRange>, startTime: number) {
  const size = timeRanges.length;
  for (let i = size - 1; i >= 0; i--) {
    const timeRange = timeRanges[i];
    if (timeRange.start === startTime || timeRange.end <= startTime) {
      break;
    }
    if (timeRange.start < startTime && timeRange.end > startTime) {
      const range = { start: startTime, end: timeRange.end };
      timeRange.end = startTime - 1;
      if (range.end > range.start) {
        timeRanges.splice(i + 1, 0, range);
      }
      if (timeRange.end <= timeRange.start) {
        timeRanges.splice(i, 1);
      }
      break;
    }
  }
}

function findTimeRangeAt(timeRanges: Array<TimeRange>, position: number, start: number, end: number): number {
  if (start > end) {
    return -1;
  }
  const index = Math.ceil((start + end) * 0.5);
  const timeRange = timeRanges[index];
  if (timeRange.start > position) {
    return findTimeRangeAt(timeRanges, position, start, index - 1);
  }
  if (timeRange.end < position) {
    return findTimeRangeAt(timeRanges, position, index + 1, end);
  }
  return index;
}

export function convertFrameByStaticTimeRanges(timeRanges: Array<TimeRange>, frame: number): number {
  const index = findTimeRangeAt(timeRanges, frame, 0, timeRanges.length - 1);
  return index !== -1 ? timeRanges[index].start : frame;
}
