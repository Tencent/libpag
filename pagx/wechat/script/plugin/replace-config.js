/* eslint-disable */
export const replaceFunctionConfig = [
  {
    name: 'replace __emval_get_method_caller',
    start: 'function __emval_get_method_caller(argCount, argTypes)',
    end: 'function __emval_get_module_property(name)',
    type: 'function',
    replaceStr: function __emval_get_method_caller(argCount, argTypes) {
      var types;
      try {
        types = __emval_lookupTypes(argCount, argTypes);
      } catch (e) {
        types = emval_lookupTypes(argCount, argTypes);
      }
      var retType = types[0];
      var signatureName =
        retType.name +
        '_$' +
        types
          .slice(1)
          .map(function (t) {
            return t.name;
          })
          .join('_') +
        '$';
      var returnId = emval_registeredMethods[signatureName];
      if (returnId !== void 0) {
        return returnId;
      }
      var params = ['retType'];
      var args = [retType];
      var argsList = '';
      for (var i2 = 0; i2 < argCount - 1; ++i2) {
        argsList += (i2 !== 0 ? ', ' : '') + 'arg' + i2;
        params.push('argType' + i2);
        args.push(types[1 + i2]);
      }
      var functionName = makeLegalFunctionName('methodCaller_' + signatureName);
      var functionBody = 'return function ' + functionName + '(handle, name, destructors, args) {\n';
      var offset = 0;
      for (var i2 = 0; i2 < argCount - 1; ++i2) {
        functionBody +=
          '    var arg' + i2 + ' = argType' + i2 + '.readValueFromPointer(args' + (offset ? '+' + offset : '') + ');\n';
        offset += types[i2 + 1]['argPackAdvance'];
      }
      functionBody += '    var rv = handle[name](' + argsList + ');\n';
      for (var i2 = 0; i2 < argCount - 1; ++i2) {
        if (types[i2 + 1]['deleteObject']) {
          functionBody += '    argType' + i2 + '.deleteObject(arg' + i2 + ');\n';
        }
      }
      if (!retType.isVoid) {
        functionBody += '    return retType.toWireType(destructors, rv);\n';
      }
      functionBody += '};\n';
      params.push(functionBody);
      var anonymous = function (retType) {
        var parentargs = Array.from(arguments);
        parentargs.shift();
        return (this[makeLegalFunctionName('methodCaller_' + signatureName)] = function (
          handle,
          name,
          destructors,
          args,
        ) {
          var paramList = [];
          var offset = 0;
          for (var i = 0; i < parentargs.length; i++) {
            paramList.push(parentargs[i].readValueFromPointer(args + offset));
            offset += types[i + 1]['argPackAdvance'];
          }
          var rv = handle[name](...paramList);
          if (!retType.isVoid) {
            return retType.toWireType(destructors, rv);
          }
        });
      };
      var invokerFunction = anonymous.apply({}, args);
      try {
        returnId = __emval_addMethodCaller(invokerFunction);
      } catch (e) {
        returnId = emval_addMethodCaller(invokerFunction);
      }
      emval_registeredMethods[signatureName] = returnId;
      return returnId;
    }.toString(),
  },
  {
    name: 'replace craftEmvalAllocator',
    start: 'function craftEmvalAllocator(argCount)',
    end: 'var emval_newers = {};',
    type: 'function',
    replaceStr: function craftEmvalAllocator(argCount) {
      var argsList = '';
      for (var i2 = 0; i2 < argCount; ++i2) {
        argsList += (i2 !== 0 ? ', ' : '') + 'arg' + i2;
      }
      var functionBody = 'return function emval_allocator_' + argCount + '(constructor, argTypes, args) {\n';
      for (var i2 = 0; i2 < argCount; ++i2) {
        functionBody +=
          'var argType' +
          i2 +
          " = requireRegisteredType(Module['HEAP32'][(argTypes >>> 2) + " +
          i2 +
          '], "parameter ' +
          i2 +
          '");\nvar arg' +
          i2 +
          ' = argType' +
          i2 +
          '.readValueFromPointer(args);\nargs += argType' +
          i2 +
          "['argPackAdvance'];\n";
      }
      functionBody += 'var obj = new constructor(' + argsList + ');\nreturn valueToHandle(obj);\n}\n';
      function anonymous(requireRegisteredType, Module, valueToHandle) {
        return function (constructor, argTypes, args) {
          var resultList = [];
          for (var i2 = 0; i2 < argCount; ++i2) {
            var currentArg = requireRegisteredType(Module['HEAP32'][(argTypes >>> 2) + i2], `parameter ${i2}`);
            var res = currentArg.readValueFromPointer(args);
            resultList.push(res);
            args += currentArg['argPackAdvance'];
          }
          var obj = new constructor(...resultList);
          return valueToHandle(obj);
        };
      }
      var invokerFunction = anonymous.apply({}, [requireRegisteredType, Module, Emval.toHandle]);
      return invokerFunction;
    }.toString(),
  },
  {
    name: 'replace craftInvokerFunction',
    start: 'function craftInvokerFunction(humanName, argTypes, classType, cppInvokerFunc, cppTargetFunc)',
    end: 'function heap32VectorToArray(count, firstElement)',
    type: 'funtcion',
    replaceStr: function craftInvokerFunction(humanName, argTypes, classType, cppInvokerFunc, cppTargetFunc) {
      var createOption = {};
      createOption.argCount = argTypes.length;
      var argCount = argTypes.length;
      if (argCount < 2) {
        throwBindingError("argTypes array size mismatch! Must at least get return value and 'this' types!");
      }
      createOption.isClassMethodFunc = argTypes[1] !== null && classType !== null;
      var isClassMethodFunc = argTypes[1] !== null && classType !== null;
      var needsDestructorStack = false;
      for (var i2 = 1; i2 < argTypes.length; ++i2) {
        if (argTypes[i2] !== null && argTypes[i2].destructorFunction === void 0) {
          needsDestructorStack = true;
          break;
        }
      }
      createOption.needsDestructorStack = needsDestructorStack;
      var returns = argTypes[0].name !== 'void';
      createOption.returns = argTypes[0].name !== 'void';
      createOption.childArgs = [];
      createOption.childDtorFunc = [];
      for (var i2 = 0; i2 < argCount - 2; ++i2) {
        createOption.childArgs.push(i2);
      }
      var args1 = ['throwBindingError', 'invoker', 'fn', 'runDestructors', 'retType', 'classParam'];
      var args2 = [throwBindingError, cppInvokerFunc, cppTargetFunc, runDestructors, argTypes[0], argTypes[1]];
      for (var i2 = 0; i2 < argCount - 2; ++i2) {
        args1.push('argType' + i2);
        args2.push(argTypes[i2 + 2]);
      }
      if (!needsDestructorStack) {
        for (var i2 = isClassMethodFunc ? 1 : 2; i2 < argTypes.length; ++i2) {
          var paramName = i2 === 1 ? 'thisWired' : 'arg' + (i2 - 2) + 'Wired';
          if (argTypes[i2].destructorFunction !== null) {
            createOption.childDtorFunc.push({
              paramName,
              func: argTypes[i2].destructorFunction,
              index: i2,
            });
          }
        }
      }
      function anonymous(throwBindingError, invoker, fn, runDestructors, retType, classParam) {
        const anonymousArg = Array.from(arguments);

        return (this[makeLegalFunctionName(humanName)] = function () {
          var parentargs = Array.from(arguments);
          var argumentLen = createOption.childArgs.length;
          if (parentargs.length !== argCount - 2) {
            throwBindingError(
              'function _PAGPlayer._getComposition called with ' + parentargs.length + ' arguments, expected 0 args!',
            );
          }
          const argArr = anonymousArg.slice(6, 6 + argumentLen);
          var destructors = [];
          var dtorStack = needsDestructorStack ? destructors : null;
          var thisWired;
          var argWiredList = [];
          var rv;
          if (isClassMethodFunc) {
            thisWired = classParam.toWireType(dtorStack, this);
          }
          for (var i = 0; i < createOption.childArgs.length; i++) {
            argWiredList.push(argArr[i].toWireType(dtorStack, parentargs[i]));
          }
          if (isClassMethodFunc) {
            rv = invoker(fn, thisWired, ...argWiredList);
          } else {
            rv = invoker(fn, ...argWiredList);
          }
          function onDone(crv) {
            if (needsDestructorStack) {
              runDestructors(destructors);
            } else {
              const funcOption = createOption.childDtorFunc;
              for (var i = 0; i < funcOption.length; i++) {
                const currentOption = funcOption[i];
                if (currentOption.index === 1) {
                  currentOption.func(thisWired);
                } else {
                  const ci = createOption.childArgs.indexOf(currentOption.index);
                  if (ci >= 0) {
                    currentOption.func(argWiredList[ci]);
                  }
                }
              }
            }
            if (returns) {
              var ret = retType.fromWireType(crv);
              return ret;
            }
          }
          return onDone(rv);
        });
      }
      var invokerFunction = anonymous.apply({}, args2);
      return invokerFunction;
    }.toString(),
  },
  {
    name: 'replace createNamedFunction',
    start: 'function createNamedFunction(name, body)',
    end: 'function extendError(baseErrorType, errorName)',
    replaceStr: function createNamedFunction(name, body) {
      name = makeLegalFunctionName(name);
      return function () {
        return body.apply(this, arguments);
      };
    }.toString(),
  },
  {
    name: 'replace getBinaryPromise',
    start: 'function getBinaryPromise()',
    end: 'function createWasm()',
    replaceStr: function getBinaryPromise() {
      return new Promise((resolve, reject) => {
        if (globalThis.isWxWebAssembly) {
          resolve(wasmBinaryFile);
        } else {
          const fs = wx.getFileSystemManager();
          fs.readFile({
            filePath: wasmBinaryFile,
            position: 0,
            success(res) {
              resolve(res.data);
            },
            fail(res) {
              reject(res);
            },
          });
        }
      });
    }.toString(),
  },
  {
    name: 'replace WebAssembly Runtime error',
    type: 'string',
    start: 'var e = new WebAssembly.RuntimeError(what);',
    replaceStr: 'var e = "run time error";',
  },
  {
    name: 'replace performance',
    type: 'string',
    start: 'performance.now();',
    replaceStr: 'Date.now();',
  },
  {
    name: 'replace pagx-viewer.wasm name',
    start: 'var wasmBinaryFile;',
    end: 'function getBinary(file)',
    replaceStr:`
    var wasmBinaryFile;
    wasmBinaryFile = "pagx-viewer.wasm.br";
    if (!isDataURI(wasmBinaryFile)) {
      wasmBinaryFile = locateFile(wasmBinaryFile);
    }
    `,
  },
  {
    name: 'fix get gl get framebuffer',
    type: 'string',
    start: 'var result = GLctx.getParameter(name_);',
    replaceStr: `var result = GLctx.getParameter(name_);
    if(result === null || result === undefined) {
      return 0;
    }
    `,
  },
  {
    name: 'fix gl initExtensions',
    type: 'string',
    start: `if (!ext.includes("lose_context") && !ext.includes("debug"))`,
    replaceStr: `if (!ext.includes("lose_context") && !ext.includes("debug") && !ext.includes("WEBGL_webcodecs_video_frame"))`,
  },
  {
    name: 'replace _scriptDir',
    type: 'string',
    start: `var _scriptDir = import_meta.url;`,
    replaceStr: `var _scriptDir = typeof document !== "undefined" && document.currentScript ? document.currentScript.src : void 0;`,
  },
];
