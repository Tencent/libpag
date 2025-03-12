#!/bin/bash

build() {
    local BUILD_TYPE=${1}
    local ARCHS=("x64" "arm64")
    
    LIB_OUT_PATH="${ROOT_DIR}/third_party/out/log4qt/lib/${BUILD_TYPE}"
    INCLUDE_OUT_PATH="${ROOT_DIR}/third_party/out/log4qt/include"
    
    if [ -d "${LIB_OUT_PATH}" ] && [ -d "${INCLUDE_OUT_PATH}" ]; then
        echo "Log4qt path[${LIB_OUT_PATH}] is exist, skip build"
        return
    fi
    
    # build
    for ARCH in "${ARCHS[@]}"; do
        if [ "${BUILD_TYPE}" == "Debug" ];
        then
            node ${ROOT_DIR}/tools/vendor_tools/cmake-build log4qt -p mac -a ${ARCH} -s ${VENDOR_SOURCE_DIR} --verbose --debug \
                -DBUILD_SHARED_LIBS=OFF -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
        else
            node ${ROOT_DIR}/tools/vendor_tools/cmake-build log4qt -p mac -a ${ARCH} -s ${VENDOR_SOURCE_DIR} --verbose \
                -DBUILD_SHARED_LIBS=OFF -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
        fi
    done
    
    # make out dir
    mkdir -p ${LIB_OUT_PATH}
    
    # merge librarys
    LIB_NAME="liblog4qt.a"
    X64_LIB=${VENDOR_SOURCE_DIR}/out/x64/${LIB_NAME}
    ARM64_LIB=${VENDOR_SOURCE_DIR}/out/arm64/${LIB_NAME}
    lipo -create "${X64_LIB}" "${ARM64_LIB}" -output "${LIB_OUT_PATH}/${LIB_NAME}"

    mkdir -p ${INCLUDE_OUT_PATH}
    cp -R ${VENDOR_SOURCE_DIR}/src/ ${INCLUDE_OUT_PATH}/
    find ${INCLUDE_OUT_PATH} -type f ! -name "*.h" -delete
}

# set variables
ROOT_DIR=$(dirname $(dirname $(dirname $(cd "$(dirname "${0}")"; pwd))))
VENDOR_SOURCE_DIR=${ROOT_DIR}/third_party/log4qt
QT_CONFIG_PATH=${ROOT_DIR}/QTCMAKE.cfg

if [ -f "${QT_CONFIG_PATH}" ];
then
    CMAKE_PREFIX_PATH=$(awk -F'[ )]' '/CMAKE_PREFIX_PATH/ {print $2}' ${QT_CONFIG_PATH})
    if [ -z "${CMAKE_PREFIX_PATH}" ]; then
        echo "Failed to get cmake prefix path"
        exit 1
    fi
else
    echo "Failed to find QTCMAKE.cfg"
    exit 1
fi

build "Release"
build "Debug"
