#!/bin/bash

build() {
    local BUILD_TYPE=${1}
    local ORG_PWD=$(pwd)
    
    CONFIG_FILE_PATH=${ROOT_DIR}/third_party/sodium/configure
    PREFIX_DIR=${ROOT_DIR}/third_party/out/sodium
    INCLUDE_OUT_PATH=${PREFIX_DIR}/include
    LIB_OUT_PATH=${PREFIX_DIR}/lib/${BUILD_TYPE}
    
    if [ -d "${LIB_OUT_PATH}" ] && [ -d "${INCLUDE_OUT_PATH}" ]; then
        echo "Sodium path[${LIB_OUT_PATH}] is exist, skip build"
        return
    fi
    
    BUILD_DIR=${VENDOR_SOURCE_DIR}/build/${BUILD_TYPE}
    if [ -d "${BUILD_DIR}" ];
    then
        rm -fr ${BUILD_DIR}
    fi
    mkdir -p ${BUILD_DIR}
    cd ${BUILD_DIR}
    # build
    if [ "${BUILD_TYPE}" == "Debug" ];
    then
        ${CONFIG_FILE_PATH} CFLAGS="-O0 -g3 -DDEBUG -Wall -Wextra -arch x86_64 -arch arm64" --prefix="${PREFIX_DIR}" --includedir="${INCLUDE_OUT_PATH}" --libdir="${LIB_OUT_PATH}"
    else
        ${CONFIG_FILE_PATH} CFLAGS="-O2 -DNDEBUG -arch x86_64 -arch arm64" --prefix="${PREFIX_DIR}" --includedir="${INCLUDE_OUT_PATH}" --libdir="${LIB_OUT_PATH}"
    fi
    make -j10
    make install
    cd ${ORG_PWD}
}

# set variables
ROOT_DIR=$(dirname $(dirname $(dirname $(cd "$(dirname "${0}")"; pwd))))
VENDOR_SOURCE_DIR=${ROOT_DIR}/third_party/sodium

build "Release"
build "Debug"
