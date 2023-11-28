#!/bin/bash -e

SOURCE_DIR=../..

echo "-----begin-----"

RELEASE_CONF="-Oz -s"
PAG_BUILD_OPTION=""
PAG_BUILD_TYPE="release"

if [[ $@ == *debug* ]]; then
  RELEASE_CONF="-O0 -g3 -s SAFE_HEAP=1"
  PAG_BUILD_OPTION="-d"
  PAG_BUILD_TYPE="debug"
fi

../../build_pag -p web $PAG_BUILD_OPTION
if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~wasm build success~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~wasm build failed~~~~~~~~~~~~~~~~~~~"
  exit 1
fi

if [ ! -d "../src/wasm" ]; then
  mkdir ../src/wasm
fi
cp -f ../../out/$PAG_BUILD_TYPE/web/wasm/libpag.wasm ../src/wasm
cp -f ../../out/$PAG_BUILD_TYPE/web/wasm/libpag.js ../src/wasm

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
