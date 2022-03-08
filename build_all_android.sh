# Copyright 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Script to build Oboe for multiple Android ABIs and prepare them for distribution
# via Prefab (github.com/google/prefab)
#
# Ensure that ANDROID_NDK environment variable is set to your Android NDK location
# e.g. /Library/Android/sdk/ndk-bundle

#!/bin/bash

if [ -z "$ANDROID_NDK" ]; then
  echo "Please set ANDROID_NDK to the Android NDK folder"
  exit 1
fi

# Directories, paths and filenames
BUILD_DIR=build

CMAKE_ARGS="-H. \
  -DBUILD_SHARED_LIBS=true \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_TOOLCHAIN=clang \
  -DANDROID_STL=c++_static \
  -DANDROID_LD=deprecated \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DCMAKE_INSTALL_PREFIX=. \
  -DPAG_USE_LIBAVC=ON"

function build_pag {

  ABI=$1
  MINIMUM_API_LEVEL=$2
  ABI_BUILD_DIR=build/${ABI}
  STAGING_DIR=staging

  echo "Building libpag for ${ABI}"

  mkdir -p ${ABI_BUILD_DIR} ${ABI_BUILD_DIR}/${STAGING_DIR}

  cmake -B${ABI_BUILD_DIR} \
        -DANDROID_ABI=${ABI} \
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${STAGING_DIR}/lib/${ABI} \
        -DANDROID_PLATFORM=android-${MINIMUM_API_LEVEL} \
        ${CMAKE_ARGS}

  pushd ${ABI_BUILD_DIR}
  make -j5
  popd
}

function strip {
  ABI=$1
  ABI_BUILD_DIR=build/${ABI}
  STRIP=${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android-strip

  STRIP -s $(pwd)/${ABI_BUILD_DIR}/lib$2.so
}

build_pag arm64-v8a 16
build_pag armeabi-v7a 16
#build_pag x86 16
#build_pag x86_64 21

strip arm64-v8a pag
strip armeabi-v7a pag

# Currently unsupported ABIs
# build_oboe armeabi 16 - This was deprecated in Android 16 and removed in 17
# build_oboe mips 21 - This was deprecated in Android 16 and removed in 17
# build_oboe mips64
