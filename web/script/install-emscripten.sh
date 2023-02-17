#!/bin/bash -e
echo "-----install emscripten begin-----"
if [ ! -d "/usr/local/Cellar/emsdk" ]; then
  git clone https://github.com/emscripten-core/emsdk.git /usr/local/Cellar/emsdk || exit 1
fi

/usr/local/Cellar/emsdk/emsdk install 3.1.20 || exit 1
/usr/local/Cellar/emsdk/emsdk activate 3.1.20 || exit 1

LINK_PATH="em++ em-config emar embuilder emcc emcmake emconfigure emdump emdwp emmake emnm emrun emprofile emscons emsize emstrip emsymbolizer emcc.py emcmake.py emar.py"

for i in $LINK_PATH; do
  if [ ! -f "/usr/local/bin/$i" ]; then
    ln -s "/usr/local/Cellar/emsdk/upstream/emscripten/$i" "/usr/local/bin/$i"
  fi
done
echo "-----install emscripten end-----"
