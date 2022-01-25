#!/bin/bash -e

echo "shell log - autotest start begin "

echo `pwd`

WORKSPACE=$(pwd)

cd $WORKSPACE

if [ -e result ] ;then
rm -rf result;
fi
mkdir result

cd result

cmake -DcppFlags="-fprofile-arcs -ftest-coverage -g -O0" ../
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

./PAGFullTest --gtest_output=json
if test $? -eq 0
then
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest successed~~~~~~~~~~~~~~~~~~"
else
echo "~~~~~~~~~~~~~~~~~~~PAGFullTest Failed~~~~~~~~~~~~~~~~~~"

cp -a "${WORKSPACE}/test/out/md5_dump.json" "${WORKSPACE}/result/md5_dump.json"
cp -a "${WORKSPACE}/test/out/dump.json" "${WORKSPACE}/result/dump.json"
cp -a ${WORKSPACE}/test/out/*.png ${WORKSPACE}/result/
exit -1
fi
