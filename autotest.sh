#!/usr/bin/env bash

function make_dir() {
  rm -rf $1
  mkdir -p $1
}

echo "shell log - autotest start begin "

echo `pwd`

WORKSPACE=$(pwd)

cd $WORKSPACE

make_dir result
make_dir build

cd build

cmake -DcppFlags="-fprofile-arcs -ftest-coverage -g -O0" -DCMAKE_BUILD_TYPE=Debug ../
if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~CMakeLists OK~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~CMakeLists error~~~~~~~~~~~~~~~~~~"
exit -1
fi

cmake --build . --target PAGFullTest -- -j 12
if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest make successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest make error~~~~~~~~~~~~~~~~~~"
exit -1
fi

./PAGFullTest --gtest_output=json > $WORKSPACE/result/autotest.json

if test $? -eq 0

then
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest Failed~~~~~~~~~~~~~~~~~~"
cp -a $WORKSPACE/test/out/*.json $WORKSPACE/result/
cp -a $WORKSPACE/test/out/*.png $WORKSPACE/result/
exit -1
fi

cp -a $WORKSPACE/build/test_detail.json $WORKSPACE/result/

cd ..
rm -rf build