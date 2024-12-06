/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

export const readBufferFromWasm = (
    module: EmscriptenModule,
    data: ArrayLike<number> | ArrayBufferLike,
    handle: (byteOffset: number, length: number) => boolean,
) => {
    const uint8Array = new Uint8Array(data);
    const dataPtr = module._malloc(uint8Array.byteLength);
    if (!handle(dataPtr, uint8Array.byteLength)) return {
        data: null,
        free: () => module._free(dataPtr)
    };
    const dataOnHeap = new Uint8Array(module.HEAPU8.buffer, dataPtr, uint8Array.byteLength);
    uint8Array.set(dataOnHeap);
    return {data: uint8Array, free: () => module._free(dataPtr)};
};
