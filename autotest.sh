#!/usr/bin/env bash

function make_dir() {
  rm -rf $1
  mkdir -p $1
}

echo "shell log - autotest start begin "

echo `pwd`

COMPLIE_RESULT=true

WORKSPACE=$(pwd)

cd $WORKSPACE

make_dir result
make_dir build

cd build

cmake -DcppFlags="-fprofile-arcs -ftest-coverage -g -O0" -DSMOKE_TEST=ON -DCMAKE_BUILD_TYPE=Debug ../
if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~CMakeLists OK~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~CMakeLists error~~~~~~~~~~~~~~~~~~"
exit
fi

cmake --build . --target PAGFullTest -- -j 12
if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest make successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest make error~~~~~~~~~~~~~~~~~~"
exit 1
fi

cmake --build . --target PAGSmokeTest -- -j 12
if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~PAGSmokeTest make successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGSmokeTest make error~~~~~~~~~~~~~~~~~~"
exit 1
fi

./PAGFullTest --gtest_output=json:PAGFullTest.json

if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest Failed~~~~~~~~~~~~~~~~~~"
COMPLIE_RESULT=false
fi

./PAGSmokeTest --gtest_output=json:PAGSmokeTest.json
if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~PAGSmokeTest successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGSmokeTest Failed~~~~~~~~~~~~~~~~~~"
COMPLIE_RESULT=false
fi

cp -a $WORKSPACE/build/*.json $WORKSPACE/result/
rm -rf build

brew install gcovr
pip3 install diff-cover

gcovr -r . -e='test/*.*' -e='vendor/*.*' --html -o ./result/coverage.html
gcovr -r . -e='test/*.*' -e='vendor/*.*' --xml-pretty -o coverage.xml
diff-cover coverage.xml --compare-branch=origin/main --exclude 'test/*.*' 'vendor/*.*' --html-report coveragediff.html>coveragediff.txt
cp -a coveragediff.html ./result/

if [ "$COMPLIE_RESULT" == false ]
then
 cp -a $WORKSPACE/test/out/baseline/**/*.lzma2 $WORKSPACE/result/
 cp -a $WORKSPACE/test/out/compare/*.webp $WORKSPACE/result/
 exit 1
fi


