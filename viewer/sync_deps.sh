#!/bin/bash
cd $(dirname $0)

../install_tools.sh

if [[ `uname` == 'Darwin' ]]; then
  if [ ! $(which emcc) ]; then
      echo "emscripten not found. Trying to install..."
      brew install emscripten
  fi
fi

if [ ! $(which depsync) ]; then
  echo "depsync not found. Trying to install..."
  npm install -g depsync > /dev/null
else
  npm update -g depsync > /dev/null
fi

depsync || exit 1
