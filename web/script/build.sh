#!/bin/bash -e

SOURCE_DIR=../..
BUILD_DIR=../../build

echo "-----begin-----"

chmod +x ./get-emscripten-version.sh
./get-emscripten-version.sh
USER_EM_VERSION="$(cat emscripten_version.txt | grep "emcc" | grep -Eo "[0-9]+\.[0-9]+\.[0-9]+")"
rm -rf emscripten_version.txt

if [ -d "../../build" ]; then
  if [ -f "../../build/emscripten_version.txt" ]; then
    CACHE_EM_VERSION="$(cat ../../build/emscripten_version.txt)"
    if [ ! "$CACHE_EM_VERSION" == "$USER_EM_VERSION" ]; then
      rm -rf "../../build"
      echo "-----emscripten version mismatch-----"
    fi
  else
    rm -rf "../../build"
    echo "-----emscripten_version.txt does not exist-----"
  fi
fi

if [ ! -d "../src/wasm" ]; then
  mkdir ../src/wasm
fi

RELEASE_CONF="-Oz -s"
CMAKE_BUILD_TYPE=Relese

if [[ $@ == *debug* ]]; then
  CMAKE_BUILD_TYPE=Debug
  RELEASE_CONF="-O0 -g3 -s SAFE_HEAP=1"
fi

emcmake cmake -S $SOURCE_DIR -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"

cmake --build $BUILD_DIR --target pag

emcc $RELEASE_CONF -std=c++17 \
  -I$SOURCE_DIR/include/ \
  -I$SOURCE_DIR/src/ \
  -I$SOURCE_DIR/tgfx/include/ \
  -I$SOURCE_DIR/tgfx/src/ \
  -DPAG_BUILD_FOR_WEB \
  -Wl,--whole-archive $BUILD_DIR/libpag.a \
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

echo $USER_EM_VERSION >../../build/emscripten_version.txt
echo "-----add emscripten_version.txt-----"

if [ ! -d "../lib" ]; then
  mkdir ../lib
fi

if [ ! -d "../wechat/lib" ]; then
  mkdir ../wechat/lib
fi

cp -f ../src/wasm/libpag.wasm ../lib
cp -f ../src/wasm/libpag.wasm ../wechat/lib

if [ ! -d "../node_modules" ]; then
  npm install
fi

if [[ ! $@ == *debug* ]]; then
  npm run build
  npm run build:wx
fi

echo "-----end-----"
