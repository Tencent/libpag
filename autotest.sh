#!/usr/bin/env bash

function make_dir() {
  rm -rf $1
  mkdir -p $1
}

echo "shell log - autotest start begin "

echo `pwd`

WORKSPACE=$(pwd)

cd $WORKSPACE

which ninja
which yasm

ls vendor_tools/

make_dir result
make_dir build

cd build

cmake --build . --target PAGFullTest -- -j 12
if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest make successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest make error~~~~~~~~~~~~~~~~~~"
exit -1
fi

./PAGFullTest --gtest_output=json 

if test $? -eq 0

then
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest Failed~~~~~~~~~~~~~~~~~~"
cp -a $WORKSPACE/test/out/baseline/**/*.lzma2 $WORKSPACE/result/
cp -a $WORKSPACE/test/out/compare/*.webp $WORKSPACE/result/
exit -1
fi

cp -a $WORKSPACE/build/test_detail.json $WORKSPACE/result/
