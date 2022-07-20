type K = keyof HTMLVideoElementEventMap;

let eventHandlers: {
  [key in K]?: {
    node: HTMLVideoElement;
    handler: (this: HTMLVideoElement, ev: HTMLVideoElementEventMap[K]) => any;
    capture: boolean;
  }[];
} = {};

export const addListener = (
  node: HTMLVideoElement,
  event: K,
  handler: (this: HTMLVideoElement, ev: HTMLVideoElementEventMap[K]) => any,
  capture = false,
) => {
  if (!(event in eventHandlers)) {
    eventHandlers[event] = [];
  }
  eventHandlers[event]?.push({ node: node, handler: handler, capture: capture });
  node.addEventListener(event, handler, capture);
};

export const removeListener = (
  targetNode: HTMLElement,
  event: K,
  targetHandler?: (this: HTMLVideoElement, ev: HTMLVideoElementEventMap[K]) => any,
) => {
  if (!(event in eventHandlers)) return;
  if (targetHandler) {
    eventHandlers[event]
      ?.filter(({ node, handler }) => node === targetNode && handler === targetHandler)
      .forEach(({ node, handler, capture }) => node.removeEventListener(event, handler, capture));
    eventHandlers[event] = eventHandlers[event]?.filter(
      ({ node, handler }) => !(node === targetNode && handler === targetHandler),
    );
  } else {
    eventHandlers[event]
      ?.filter(({ node }) => node === targetNode)
      .forEach(({ node, handler, capture }) => node.removeEventListener(event, handler, capture));
    eventHandlers[event] = eventHandlers[event]?.filter(({ node }) => node !== targetNode);
  }
};

export const removeAllListeners = (targetNode: HTMLElement) => {
  Object.keys(eventHandlers).forEach((event) => {
    const videoEvent = event as K;
    eventHandlers[videoEvent]
      ?.filter(({ node }) => node === targetNode)
      .forEach(({ node, handler, capture }) => node.removeEventListener(videoEvent, handler, capture));

    eventHandlers[videoEvent] = eventHandlers[videoEvent]?.filter(({ node }) => node !== targetNode);
  });
};
