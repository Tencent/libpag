#!/usr/bin/env bash

function make_dir() {
  rm -rf $1
  mkdir -p $1
}
echo "shell log - autotest start"
if [[ $(uname) == 'Darwin' ]]; then
  MAC_REQUIRED_TOOLS="gcovr"
  for TOOL in ${MAC_REQUIRED_TOOLS[@]}; do
    if [ ! $(which $TOOL) ]; then
      if [ ! $(which brew) ]; then
        echo "Homebrew not found. Trying to install..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" ||
          exit 1
      fi
      echo "$TOOL not found. Trying to install..."
      brew install $TOOL || exit 1
    fi
  done
fi

echo $(pwd)

COMPLIE_RESULT=true

WORKSPACE=$(pwd)

cd $WORKSPACE

make_dir result
make_dir build

./update_baseline.sh 1

cd build

cmake -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage -g -O0" -DPAG_USE_SWIFTSHADER=ON -DCMAKE_BUILD_TYPE=Debug ../
if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~CMakeLists OK~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~CMakeLists error~~~~~~~~~~~~~~~~~~"
  exit
fi

cmake --build . --target PAGFullTest -- -j 12
if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~PAGFullTest make successed~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~PAGFullTest make error~~~~~~~~~~~~~~~~~~"
  exit 1
fi

./PAGFullTest --gtest_output=json:PAGFullTest.json

if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~PAGFullTest successed~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~PAGFullTest Failed~~~~~~~~~~~~~~~~~~"
  COMPLIE_RESULT=false
fi

cp -a $WORKSPACE/build/*.json $WORKSPACE/result/

cd ..

gcovr -r . -e='test/*.*' -e='vendor/*.*'--html -o ./result/coverage.html
gcovr -r . -e='test/*.*' -e='vendor/*.*' --xml-pretty -o ./result/coverage.xml

rm -rf build

if [ "$COMPLIE_RESULT" == false ]; then
  cp -r $WORKSPACE/test/out/. $WORKSPACE/result
  exit 1
fi
