#!/bin/bash

build() {
    local BUILD_TYPE=${1}
    local ARCHS=("x64" "arm64")
    
    LIB_OUT_PATH="${ROOT_DIR}/third_party/out/libpag/lib/${BUILD_TYPE}"
    INCLUDE_OUT_PATH="${ROOT_DIR}/third_party/out/libpag/include"
    
    if [ -d "${LIB_OUT_PATH}" ] && [ -d "${INCLUDE_OUT_PATH}" ]; then
        echo "Libpag path[${LIB_OUT_PATH}] is exist, skip build"
        return
    fi
    
    # build
    for ARCH in "${ARCHS[@]}"; do
        if [ "${BUILD_TYPE}" == "Debug" ];
        then
            node ${ROOT_DIR}/tools/vendor_tools/cmake-build tgfx-vendor pag-vendor pag -p mac -a ${ARCH} -s ${VENDOR_SOURCE_DIR} --verbose --debug \
                -DPAG_BUILD_SHARED=OFF -DPAG_USE_ENCRYPT_ENCODE=ON -DPAG_BUILD_FRAMEWORK=OFF -DPAG_USE_MOVIE=ON -DPAG_USE_QT=ON \
                -DPAG_USE_RTTR=ON -DPAG_USE_LIBAVC=OFF -DPAG_BUILD_TESTS=OFF -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
        else
            node ${ROOT_DIR}/tools/vendor_tools/cmake-build tgfx-vendor pag-vendor pag -p mac -a ${ARCH} -s "${VENDOR_SOURCE_DIR}" --verbose \
                -DPAG_BUILD_SHARED=OFF -DPAG_USE_ENCRYPT_ENCODE=ON -DPAG_BUILD_FRAMEWORK=OFF -DPAG_USE_MOVIE=ON -DPAG_USE_QT=ON \
                -DPAG_USE_RTTR=ON -DPAG_USE_LIBAVC=OFF -DPAG_BUILD_TESTS=OFF -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
        fi
    done
    
    # make out dir
    mkdir -p ${LIB_OUT_PATH}
    
    # merge librarys
    local LIBS=("libpag.a" "libtgfx-vendor.a" "libpag-vendor.a")
    for LIB_NAME in "${LIBS[@]}"; do
        local X64_LIB=""
        local ARM64_LIB=""
        
        case ${LIB_NAME} in
            "libtgfx-vendor.a")
                X64_LIB=${VENDOR_SOURCE_DIR}/out/x64/x64/tgfx/CMakeFiles/tgfx-vendor.dir/x64/${LIB_NAME}
                ARM64_LIB=${VENDOR_SOURCE_DIR}/out/arm64/arm64/tgfx/CMakeFiles/tgfx-vendor.dir/arm64/${LIB_NAME}
                ;;
            *)
                X64_LIB=${VENDOR_SOURCE_DIR}/out/x64/${LIB_NAME}
                ARM64_LIB=${VENDOR_SOURCE_DIR}/out/arm64/${LIB_NAME}
                ;;
        esac
        
        lipo -create "${X64_LIB}" "${ARM64_LIB}" -output "${LIB_OUT_PATH}/${LIB_NAME}"
    done

    mkdir -p ${INCLUDE_OUT_PATH}
    cp -R ${VENDOR_SOURCE_DIR}/include/ ${INCLUDE_OUT_PATH}/
}

# set variables
ROOT_DIR=$(dirname $(dirname $(dirname $(cd "$(dirname "${0}")"; pwd))))
VENDOR_SOURCE_DIR=$(dirname ${ROOT_DIR})
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
