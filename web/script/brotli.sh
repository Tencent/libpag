#!/bin/bash -e

npm run brotli
cp -f ./lib/libpag.wasm.br ./demo/wechat-miniprogram/utils/libpag.wasm.br
rm ./lib/libpag.wasm.br
