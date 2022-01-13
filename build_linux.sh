#!/bin/bash -e

function make_dir() {
  rm -rf $1
  mkdir -p $1
}

BUILD_DIR="linux_out"

make_dir $BUILD_DIR

cd $BUILD_DIR

cmake -DPAG_USE_SWIFTSHADER=ON -DPAG_BUILD_SHARED=OFF -DCMAKE_BUILD_TYPE=Release ../

RETURN_CODE=$?
if [ "$RETURN_CODE" -eq "0" ]
then
  echo "cmake success"
else
  echo "cmake failed"
  exit 1
fi

cmake --build . --target pag -- -j 12
RETURN_CODE=$?
if [ "$RETURN_CODE" -eq "0" ]
then
  echo "compile success"
else
  echo "compile failed"
  exit 1
fi

cd ..

make_dir vendor/pag/include
cp -a include/* vendor/pag/include
make_dir vendor/pag/linux
cp -a $BUILD_DIR/libpag.a vendor/pag/linux

rm -rf $BUILD_DIR


