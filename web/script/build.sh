#!/bin/bash -e

SOURCE_DIR=../..

echo "-----begin-----"

if [ ! -d "../src/wasm" ]; then
  mkdir ../src/wasm
fi

RELEASE_CONF="-Oz -s"
PAG_BUILD_OPTION=""
PAG_BUILD_TYPE="release"

if [[ $@ == *debug* ]]; then
  RELEASE_CONF="-O0 -g3 -s SAFE_HEAP=1"
  PAG_BUILD_OPTION="-d"
  PAG_BUILD_TYPE="debug"
fi

../../build_pag -p web $PAG_BUILD_OPTION

emcc $RELEASE_CONF -std=c++17 \
  -I$SOURCE_DIR/include/ \
  -I$SOURCE_DIR/src/ \
  -I$SOURCE_DIR/third_party/tgfx/include/ \
  -DPAG_BUILD_FOR_WEB \
  -Wl,--whole-archive ../../out/$PAG_BUILD_TYPE/web/wasm/libpag.a \
  --no-entry \
  --bind \
  -s WASM=1 \
  -s ASYNCIFY \
  -s USE_WEBGL2=1 \
  -s "EXPORTED_RUNTIME_METHODS=['GL','Asyncify']" \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORT_NAME="PAGInit" \
  -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
  -s MODULARIZE=1 \
  -s NO_EXIT_RUNTIME=1 \
  -s ENVIRONMENT="web,worker" \
  -s EXPORT_ES6=1 \
  -s USE_ES6_IMPORT_META=0 \
  -o ../src/wasm/libpag.js

if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~wasm build success~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~wasm build failed~~~~~~~~~~~~~~~~~~~"
  exit 1
fi

if [ ! -d "../lib" ]; then
  mkdir ../lib
fi

if [ ! -d "../wechat/lib" ]; then
  mkdir ../wechat/lib
fi

cp -f ../src/wasm/libpag.wasm ../lib
cp -f ../src/wasm/libpag.wasm ../wechat/lib

npm install --silent

if [[ ! $@ == *debug* ]]; then
  npm run build
  npm run build:wx
fi

echo "-----end-----"
