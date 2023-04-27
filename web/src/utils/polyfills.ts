const getGlobalObject = (): typeof globalThis => {
  if (typeof window !== 'undefined') {
    return window as unknown as typeof globalThis;
  } else if (typeof global !== 'undefined') {
    return global as unknown as typeof globalThis;
  } else if (typeof self !== 'undefined') {
    return self as unknown as typeof globalThis;
  } else {
    throw new Error('unable to locate global object');
  }
};

const globalObject = getGlobalObject();

if (typeof globalObject.globalThis === 'undefined') {
  Object.defineProperty(globalObject, 'globalThis', {
    get() {
      return globalObject;
    },
  });
}
