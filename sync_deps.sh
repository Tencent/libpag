#!/bin/bash -e
cd $(dirname $0)

./install_tools.sh

if [[ `uname` == 'Darwin' ]]; then
  if [ ! $(which emcc) ]; then
      echo "emscripten not found. Trying to install..."
      ./web/script/install-emscripten.sh  || exit 1
  fi
  if [ ! $(which gcovr) ]; then
      echo "gcovr not found. Trying to install..."
      brew install gcovr || exit 1
  fi
fi

depsync

git lfs prune