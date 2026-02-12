/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Class decorator that wraps all prototype methods with a destroy check.
 * Methods called on a destroyed instance will log an error and return early.
 */
export function destroyVerify(constructor: any) {
  const functions = Object.getOwnPropertyNames(constructor.prototype).filter(
    (name) => name !== 'constructor' && typeof constructor.prototype[name] === 'function',
  );

  const proxyFn = (target: { [prop: string]: any }, methodName: string) => {
    const fn = target[methodName];
    target[methodName] = function (...args: any[]) {
      if (this['isDestroyed']) {
        console.error(`Don't call ${methodName} of the ${constructor.name} that is destroyed.`);
        return;
      }
      return fn.call(this, ...args);
    };
  };
  functions.forEach((name) => proxyFn(constructor.prototype, name));
}
