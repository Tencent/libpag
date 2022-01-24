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

gcovr -r . -e='test/*.*' -e='vendor/*.*' --html -o ./result/coverage.html
gcovr -r . -e='test/*.*' -e='vendor/*.*' --xml-pretty -o coverage.xml
diff-cover coverage.xml --compare-branch=origin/main --exclude 'test/*.*' 'vendor/*.*' --html-report coveragediff.html>coveragediff.txt
mv coveragediff.html ./result/coveragediff.html
coveragediff=`grep -E "Coverage:" coveragediff.txt | awk -F"[ ]" '{print substr($2,1,length($2)-1)}'`
if [ ! "$coveragediff" ];then coveragediff=100;fi
setGateValue "UnitTestLineCoverage" $coveragediff
rm -rf coverag*
