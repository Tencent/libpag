#!/bin/bash -e

INSTALL_PATH="/usr/local/Cellar/emsdk"

echo "-----install emscripten begin-----"
if [ ! -d $INSTALL_PATH ]; then
 git clone https://github.com/emscripten-core/emsdk.git $INSTALL_PATH || exit 1
fi

"$INSTALL_PATH/emsdk" install 3.1.20 || exit 1
"$INSTALL_PATH/emsdk" activate 3.1.20 || exit 1

LINK_PATH=$(ls $INSTALL_PATH/upstream/emscripten/ | grep ^em)

for i in $LINK_PATH; do
 if [ ! -f "/usr/local/bin/$i" ]; then
   ln -s "$INSTALL_PATH/upstream/emscripten/$i" "/usr/local/bin/$i"
 fi
done

echo "-----install emscripten end-----"
