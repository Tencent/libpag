#!/bin/bash

build() {
    local BUILD_TYPE=${1}
    local ORG_PWD=$(pwd)
    
    LIB_OUT_PATH="${ROOT_DIR}/third_party/out/sparkle/lib/${BUILD_TYPE}"
    
    if [ -d "${LIB_OUT_PATH}" ]; then
        echo "Sparkle path[${LIB_OUT_PATH}] is exist, skip build"
        return
    fi
    
    BUILD_DIR=${VENDOR_SOURCE_DIR}/build
    if [ -d "${BUILD_DIR}" ];
    then
        rm -fr ${BUILD_DIR}
    fi
    mkdir -p ${BUILD_DIR}
    cd ${VENDOR_SOURCE_DIR}
    # build
    make release

    mkdir -p ${LIB_OUT_PATH}
    cp -fRP ./build/Sparkle.*/Build/Products/Release/Sparkle.framework ${LIB_OUT_PATH}/

    rm -fr ${BUILD_DIR}

    cd ${ORG_PWD}
}

# set variables
ROOT_DIR=$(dirname $(dirname $(dirname $(cd "$(dirname "${0}")"; pwd))))
VENDOR_SOURCE_DIR=${ROOT_DIR}/third_party/sparkle

build "Release"
