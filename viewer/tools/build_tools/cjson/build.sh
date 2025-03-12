#!/bin/bash

build() {
    local BUILD_TYPE=${1}
    local ARCHS=("x64" "arm64")
    
    LIB_OUT_PATH="${ROOT_DIR}/third_party/out/cjson/lib/${BUILD_TYPE}"
    INCLUDE_OUT_PATH="${ROOT_DIR}/third_party/out/cjson/include"
    
    if [ -d "${LIB_OUT_PATH}" ] && [ -d "${INCLUDE_OUT_PATH}" ]; then
        echo "cJSON path[${LIB_OUT_PATH}] is exist, skip build"
        return
    fi
    
    # build
    for ARCH in "${ARCHS[@]}"; do
        if [ "${BUILD_TYPE}" == "Debug" ];
        then
            node ${ROOT_DIR}/tools/vendor_tools/cmake-build cjson-static -p mac -a ${ARCH} -s ${VENDOR_SOURCE_DIR} --verbose --debug \
                -DENABLE_CJSON_UTILS=Off -DENABLE_CJSON_TEST=Off -DBUILD_SHARED_AND_STATIC_LIBS=On
        else
            node ${ROOT_DIR}/tools/vendor_tools/cmake-build cjson-static -p mac -a ${ARCH} -s ${VENDOR_SOURCE_DIR} --verbose \
                -DENABLE_CJSON_UTILS=Off -DENABLE_CJSON_TEST=Off -DBUILD_SHARED_AND_STATIC_LIBS=On
        fi
    done
    
    # make out dir
    mkdir -p ${LIB_OUT_PATH}
    
    # merge librarys
    LIB_NAME="libcjson.a"
    X64_LIB=${VENDOR_SOURCE_DIR}/out/x64/${LIB_NAME}
    ARM64_LIB=${VENDOR_SOURCE_DIR}/out/arm64/${LIB_NAME}
    lipo -create "${X64_LIB}" "${ARM64_LIB}" -output "${LIB_OUT_PATH}/${LIB_NAME}"

    mkdir -p ${INCLUDE_OUT_PATH}
    cp -R ${VENDOR_SOURCE_DIR}/cJSON.h ${INCLUDE_OUT_PATH}/
}

# set variables
ROOT_DIR=$(dirname $(dirname $(dirname $(cd "$(dirname "${0}")"; pwd))))
VENDOR_SOURCE_DIR=${ROOT_DIR}/third_party/cjson

build "Release"
build "Debug"
