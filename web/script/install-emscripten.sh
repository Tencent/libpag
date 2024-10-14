#!/bin/bash -e

INSTALL_PATH="/usr/local/Cellar/emsdk"

echo "-----install emscripten begin-----"
if [ ! -d $INSTALL_PATH ]; then
  mkdir -p $INSTALL_PATH
  git clone https://github.com/emscripten-core/emsdk.git $INSTALL_PATH
fi

"$INSTALL_PATH/emsdk" install 3.1.20
"$INSTALL_PATH/emsdk" activate 3.1.20

LINK_PATH=$(ls $INSTALL_PATH/upstream/emscripten/ | grep ^em)

for i in $LINK_PATH; do
   ln -sf "$INSTALL_PATH/upstream/emscripten/$i" "/usr/local/bin/$i"
done

echo "-----install emscripten end-----"
