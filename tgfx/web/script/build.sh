#!/bin/bash -e

SOURCE_DIR=../..
BUILD_DIR=../../build

echo "-----begin-----"

if [ ! -d "../src/wasm" ]; then
  mkdir ../src/wasm
fi

RELEASE_CONF="-Oz -s"
CMAKE_BUILD_TYPE=Relese

if [[ $@ == *debug* ]]; then
  CMAKE_BUILD_TYPE=Debug
  RELEASE_CONF="-O0 -g3 -s SAFE_HEAP=1"
fi

emcmake cmake -S $SOURCE_DIR -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"  -DTGFX_USE_BIND_FOR_WEB="ON"

cmake --build $BUILD_DIR --target tgfx

emcc $RELEASE_CONF -std=c++17 \
  -I$SOURCE_DIR/include/ \
  -I$SOURCE_DIR/src/ \
  -Wl,--whole-archive $BUILD_DIR/libtgfx.a \
  --no-entry \
  --bind \
  -s WASM=1 \
  -s ASYNCIFY \
  -s USE_WEBGL2=1 \
  -s "EXPORTED_RUNTIME_METHODS=['GL','Asyncify']" \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORT_NAME="TGFXInit" \
  -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
  -s MODULARIZE=1 \
  -s NO_EXIT_RUNTIME=1 \
  -s ENVIRONMENT="web,worker" \
  -s EXPORT_ES6=1 \
  -s USE_ES6_IMPORT_META=0 \
  -o ../src/wasm/tgfx.js

if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~wasm build success~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~wasm build failed~~~~~~~~~~~~~~~~~~~"
  exit 1
fi

echo $USER_EM_VERSION >../../build/emscripten_version.txt
echo "-----add emscripten_version.txt-----"

if [ ! -d "../lib" ]; then
  mkdir ../lib
fi

cp -f ../src/wasm/tgfx.wasm ../lib

if [ ! -d "../node_modules" ]; then
  npm install
fi

if [[ ! $@ == *debug* ]]; then
  npm run build
fi

echo "-----end-----"
