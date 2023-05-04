const getGlobalObject = (): typeof globalThis => {
  if (typeof globalThis !== 'undefined') {
    return globalThis;
  }
  if (typeof window !== 'undefined') {
    return window as unknown as typeof globalThis;
  }
  if (typeof global !== 'undefined') {
    return global as unknown as typeof globalThis;
  }
  if (typeof self !== 'undefined') {
    return self as unknown as typeof globalThis;
  }
  throw new Error('unable to locate global object');
};

const globalObject = getGlobalObject();

if (typeof globalObject.globalThis === 'undefined') {
  Object.defineProperty(globalObject, 'globalThis', {
    get() {
      return globalObject;
    },
  });
}
