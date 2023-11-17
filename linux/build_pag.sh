#!/bin/bash -e
cd $(dirname $0)

function make_dir() {
  rm -rf $1
  mkdir -p $1
}

../install_tools.sh

if [[ $(uname) == 'Darwin' ]]; then
  PLATFORM="mac"
elif [[ $(uname) == 'Linux' ]]; then
  PLATFORM="linux"
fi

node ../build_pag -p $PLATFORM -DPAG_USE_SWIFTSHADER=ON -DPAG_BUILD_SHARED=OFF -DPAG_BUILD_FRAMEWORK=OFF -o vendor/libpag/$PLATFORM

make_dir vendor/libpag/include
cp -a ../include/* vendor/libpag/include

make_dir vendor/swiftshader/$PLATFORM
cp -a ../third_party/tgfx/vendor/swiftshader/$PLATFORM/* vendor/swiftshader/$PLATFORM

