export function wasmAwaitRewind(constructor: any) {
  const ignoreStaticFunctions = ['length', 'name', 'prototype', 'wasmAsyncMethods'];

  let staticFunctions = Object.getOwnPropertyNames(constructor).filter(
    (name) => ignoreStaticFunctions.indexOf(name) === -1,
  );
  let functions = Object.getOwnPropertyNames(constructor.prototype).filter(
    (name) => name !== 'constructor' && typeof constructor.prototype[name] === 'function',
  );
  functions = functions.concat(staticFunctions);
  const wasmAsyncMethods = constructor.prototype.wasmAsyncMethods;
  if (wasmAsyncMethods && wasmAsyncMethods.length > 0) {
    functions = functions.filter((name) => wasmAsyncMethods.indexOf(name) === -1);
  }
  functions.forEach((name) => {
    const fn = constructor.prototype[name];
    constructor.prototype[name] = function (...args) {
      if (constructor.module.Asyncify.currData !== null) {
        const currData = constructor.module.Asyncify.currData;
        constructor.module.Asyncify.currData = null;
        const ret = fn.call(this, ...args);
        constructor.module.Asyncify.currData = currData;
        return ret;
      } else {
        return fn.call(this, ...args);
      }
    };
  });
}

export function wasmAsyncMethod(isStatic = false) {
  return (target: any, propertyKey: string, descriptor: PropertyDescriptor) => {
    const constructor = isStatic ? target.prototype : target;
    if (!constructor.wasmAsyncMethods) {
      constructor.wasmAsyncMethods = [];
    }
    constructor.wasmAsyncMethods.push(propertyKey);
  };
}
