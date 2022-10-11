#!/bin/bash -e
echo "-----install emscripten begin-----"
if [ ! -d "/usr/local/Cellar/emsdk" ]; then
  git clone https://github.com/emscripten-core/emsdk.git /usr/local/Cellar/emsdk
  /usr/local/Cellar/emsdk/emsdk install 3.1.20
  /usr/local/Cellar/emsdk/emsdk activate 3.1.20
fi

source /usr/local/Cellar/emsdk/emsdk_env.sh
