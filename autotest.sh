#!/usr/bin/env bash

# Build + run PAGFullTest for one GPU backend.
#
# Usage:
#   ./autotest.sh [USE_OPENGL|USE_OPENGL_SWIFTSHADER|USE_METAL]
#
# Default is USE_OPENGL when no backend is specified. Aligned with update_baseline.sh so both
# scripts speak the same backend vocabulary and the per-backend cache under
# test/baseline/.cache/<backend>/ stays consistent.

function make_dir() {
  rm -rf $1
  mkdir -p $1
}
echo "shell log - autotest start"
./install_tools.sh

echo $(pwd)

COMPLIE_RESULT=true

WORKSPACE=$(pwd)

cd $WORKSPACE

# Parse arguments the same way update_baseline.sh does, so the backend keyword and the
# --skip-images flag can appear in any order (e.g. `./autotest.sh --skip-images USE_METAL`).
# Without this, the dispatch below would reject --skip-images as an unknown backend even though
# update_baseline.sh (invoked next) handles it fine, leaving the user in a confusing state.
BACKEND_ARG=""
for arg in "$@"; do
  case "$arg" in
    --skip-images) ;;
    *) BACKEND_ARG="$arg" ;;
  esac
done

./update_baseline.sh "$@"
if test $? -ne 0; then
   exit 1
fi
cp -r $WORKSPACE/test/baseline $WORKSPACE/result

# Determine cmake args and target suffix from the requested backend keyword.
case "$BACKEND_ARG" in
  USE_OPENGL_SWIFTSHADER)
    CMAKE_BACKEND_ARGS="-DPAG_USE_SWIFTSHADER=ON"
    TARGET_SUFFIX="OpenGL" ;;
  USE_METAL)
    CMAKE_BACKEND_ARGS="-DPAG_USE_METAL=ON -DPAG_USE_OPENGL=OFF"
    TARGET_SUFFIX="Metal" ;;
  USE_VULKAN)
    CMAKE_BACKEND_ARGS="-DPAG_USE_VULKAN=ON -DPAG_USE_OPENGL=OFF"
    TARGET_SUFFIX="Vulkan" ;;
  USE_D3D12)
    CMAKE_BACKEND_ARGS="-DPAG_USE_D3D12=ON -DPAG_USE_OPENGL=OFF"
    TARGET_SUFFIX="D3D12" ;;
  USE_OPENGL|"")
    CMAKE_BACKEND_ARGS=""
    TARGET_SUFFIX="OpenGL" ;;
  *)
    echo "Error: unknown backend '$BACKEND_ARG'."
    echo "Supported: USE_OPENGL (default), USE_OPENGL_SWIFTSHADER, USE_METAL, USE_VULKAN, USE_D3D12."
    exit 1 ;;
esac

make_dir result
make_dir build
cd build

cmake -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage -g -O0" $CMAKE_BACKEND_ARGS \
      -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ../
if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~CMakeLists OK~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~CMakeLists error~~~~~~~~~~~~~~~~~~"
  exit
fi

cmake --build . --target PAGFullTest_${TARGET_SUFFIX} -- -j 12
if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~PAGFullTest_${TARGET_SUFFIX} make successed~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~PAGFullTest_${TARGET_SUFFIX} make error~~~~~~~~~~~~~~~~~~"
  exit 1
fi

./PAGFullTest_${TARGET_SUFFIX} --gtest_output=json:PAGFullTest.json

if test $? -eq 0; then
  echo "~~~~~~~~~~~~~~~~~~~PAGFullTest_${TARGET_SUFFIX} successed~~~~~~~~~~~~~~~~~~"
else
  echo "~~~~~~~~~~~~~~~~~~~PAGFullTest_${TARGET_SUFFIX} Failed~~~~~~~~~~~~~~~~~~"
  COMPLIE_RESULT=false
fi

cp -a $WORKSPACE/build/*.json $WORKSPACE/result/

cd ..

cp -r $WORKSPACE/test/out $WORKSPACE/result

if [ "$COMPLIE_RESULT" == false ]; then
  exit 1
fi
