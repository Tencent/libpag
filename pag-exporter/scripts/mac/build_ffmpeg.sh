#!/bin/bash -e

. $(dirname $(dirname $(dirname $(dirname $0))))/third_party/vendor_tools/Tools.sh

SOURCE_DIR=$(pwd)
BUILD_DIR=${SOURCE_DIR}/build

if [ -z "$VENDOR_ARCHS" ]; then
    echo "VENDOR_ARCHS is unset or empty"
    build_archs="arm64 x64"
else
    echo "VENDOR_ARCHS: $VENDOR_ARCHS"
    build_archs=${VENDOR_ARCHS}
fi

echo "SOURCE_DIR: ${SOURCE_DIR}"
echo "BUILD_DIR: ${BUILD_DIR}"
echo "Building for architectures: ${build_archs}"

cd ${SOURCE_DIR}
# Build for each architecture
for arch in ${build_archs}; do
    echo "Building for ${arch} start"
    echo "clean build env"
    make distclean

    case "${arch}" in
        "arm64")
            extra_cflags="-arch arm64"
            extra_ldflags="-arch arm64 -Wl,-search_paths_first"
            ;;
        "x64")
            extra_cflags="-arch x86_64"
            extra_ldflags="-arch x86_64 -Wl,-search_paths_first"
            ;;
        *)
            echo "Unsupported architecture: ${arch}"
            exit 1
            ;;
    esac

    ./configure --prefix=${BUILD_DIR}/mac/${arch} \
        --target-os=darwin \
        --enable-static \
        --disable-shared \
        --cc=clang \
        --ar="/usr/bin/ar" \
        --ranlib="/usr/bin/ranlib" \
        --extra-cflags="${extra_cflags}" \
        --extra-ldflags="${extra_ldflags}" \
        --disable-all \
        --enable-avcodec \
        --enable-avformat \
        --enable-avutil \
        --enable-avfilter \
        --enable-swresample \
        --enable-swscale

    make -j8 && make install
    echo "Build for ${arch} completed"
done

# Copy libraries
copy_library -p mac -l "x64=${BUILD_DIR}/mac/x64/lib/*.a;arm64=${BUILD_DIR}/mac/arm64/lib/*.a"

echo "copy ffmpeg libs to ${VENDOR_OUT_DIR}"