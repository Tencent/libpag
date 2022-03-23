
#!/bin/sh
# libpag iOS build script

echo "shell log - show pwd at begin"

# 外部输入参数说明
# PAGBuildConfig: movie、release、public
  # movie: 构建出movie和movie_no_ffmpeg两种类型的包
  # release：构建出不含movie的包，含统计上报
  # public：构建出不含movie的包，不含统计上报


###############设置需编译的项目配置名称
BUILD_CONFIG=Release #编译的方式,有Release,Debug，自定义的AdHoc等
PROJECT_NAME=PAGViewer
FRAMEWORK_NAME=libpag
GIT_BRANCH=master

GIT_VERSION=`git log --abbrev-commit|head -1|cut -d' ' -f 2`
PACKAGE_DATE=`date '+%Y%m%d%H'`

SDK_VERSION=${MajorVersion}.${MinorVersion}.${FixVersion}.${BK_CI_BUILD_NO}
FRAMEWORK_TAG=${SDK_VERSION}-symbol
FRAMEWORK_TAG_STRIP=${SDK_VERSION}


echo `pwd`

WORKSPACE=$(pwd)

BUILD_APP_DIR=${WORKSPACE}/result #编译打包完成后.app文件存放的目录
BUILD_APP_DIR_RELEASE=${WORKSPACE}/release #编译打包完成后.app文件存放的目录
STRIP_DIR=${WORKSPACE}/ios/strip
######Full 版本设置
IPHONE_OS_DIR=$BUILD_APP_DIR/${BUILD_CONFIG}-iphoneos/${FRAMEWORK_NAME}
IPHONE_SIMULATOR_DIR=$BUILD_APP_DIR/${BUILD_CONFIG}-iphonesimulator/${FRAMEWORK_NAME}
LIB_STATIC_NAME=${FRAMEWORK_NAME}.framework


cd $WORKSPACE

if [ -e result ] ;then
rm -rf result;
fi
mkdir result

if [ -e release ] ;then
rm -rf release;
fi
mkdir release


echo "项目名称:$PROJECT_NAME"
echo $WORKSPACE

###############进入项目目录
cd $WORKSPACE
echo $SDK_VERSION
#修改libpag的版本信息
sed -i "" "s/^static const char sdkVersion.*/static const char sdkVersion[] = \"${SDK_VERSION}\";/g" src/rendering/PAG.cpp

DIR_ARM64=
DIR_ALL=

pushPAGTag() {
    if [[ "${pushTag}" != true ]]; then
        return;
    fi
    cd ${WORKSPACE}
    echo "~~~~~~~~~~~~~~~~~~~PUSH PAG TAG 开始~~~~~~~~~~~~~~~~~~~"
    pagTag="v${MajorVersion}.${MinorVersion}.${FixVersion}.${BK_CI_BUILD_NO}_${hookBranch}_iOS_$(date +%Y%m%d%H%M)"
    git tag -a ${pagTag} -m ""
    echo "git tag -a ${pagTag} -m "
    git push origin ${hookBranch} ${pagTag}
    echo "~~~~~~~~~~~~~~~~~~~PUSH PAG TAG 结束~~~~~~~~~~~~~~~~~~~"
}

pushPAG() {
  # param1：push TAG信息
  # param2：framework所在文件夹名称
   param1=$1
   param2=$2
   # 修改podspec版本号及.orange-ci.yml
  sed -i "" "s/^  s.version.*/  s.version  = \'${param1}\'/g" libpag.podspec
  sed -i "" "s/^            version:.*/            version: ${param1}/g" .orange-ci.yml

  rm -rf ${BUILD_APP_DIR}/pag-ios/pag-ios/pag-ios/framework/full/*
  cp -a "${BUILD_APP_DIR}/${param2}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/pag-ios/pag-ios/pag-ios/framework/full/${LIB_STATIC_NAME}"

  echo "拷贝结束"
  git add .
  git commit -m "Push From Landun: update SDK"
  git tag -a ${param1} -m "Push From Landun: update SDK"
  git push origin ${GIT_BRANCH} ${param1}
  echo "push结束"
}

pushPAG_iOS() {
  parameter=$@
  cd $BUILD_APP_DIR
  if [ -e pag-ios ] ;then
  rm -r pag-ios;
  fi
  mkdir pag-ios

  cd pag-ios
  echo "拉取 pag-ios 的代码"

  git clone -b ${GIT_BRANCH} http://${DomainName}/CameraToolsGroup/pag-ios.git

  cd pag-ios
  if [ "$parameter" = "static" ]
    then
        pushPAG ${FRAMEWORK_TAG} ${DIR_ALL_STATIC}
    else
        pushPAG ${FRAMEWORK_TAG} ${DIR_ALL}
        pushPAG ${FRAMEWORK_TAG_STRIP} ${DIR_ALL_STRIP}
    fi
}

updatePAGDocVersion() {
  #生成文档
    cd $WORKSPACE/ios

    DOC_ZIP_NAME=${FRAMEWORK_NAME}_${SDK_VERSION}_ios_docs.zip

    ./updateDoc.sh

    cd docs

    mv "./html" "./libpag_iOS_docs"

    zip -qr "${DOC_ZIP_NAME}" "libpag_iOS_docs"
    cp -a "${DOC_ZIP_NAME}" "${BUILD_APP_DIR}/${DOC_ZIP_NAME}"

    if [[ "${BK_CI_HOOK_BRANCH}" != "release" ]] ;then
        return;
    fi

    echo "~~~~~~~~~~~~~~~~~~~开始更新PAG Doc版本~~~~~~~~~~~~~~~~~~~"
    cd ${WORKSPACE}
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    if [ -e pag-docs ] ;then
        rm -rf pag-docs;
    fi

    echo "拉取 pag-doc 的代码"
    git clone -b master http://${DomainName}/shadow_frontend/PAG-docs.git pag-docs

    cd pag-docs
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    sed -i "" "s?pod 'libpag', :git => 'http://git.code.oa.com/CameraToolsGroup/pag-ios.git', :tag =>.*?pod 'libpag', :git => 'http://git.code.oa.com/CameraToolsGroup/pag-ios.git', :tag =>'${FRAMEWORK_TAG_STRIP}'?" ./docs/sdk-guide/install-sdk.md
    git add ./docs/sdk-guide/install-sdk.md
    git commit -m "update pag iOS version from RDM"
    git push
    cd ${WORKSPACE}

    echo "~~~~~~~~~~~~~~~~~~~结束更新PAG Doc版本~~~~~~~~~~~~~~~~~~~"
}


function LoopBuild(){
   VALUE=$@
   echo "---input---:${VALUE}"
   cd ${BUILD_APP_DIR}
   DIR_ARM64=arm64${VALUE}
   DIR_ARM64_STRIP=arm64${VALUE}-strip
   DIR_ALL=all${VALUE}
   DIR_ALL_STRIP=all${VALUE}-strip
   DIR_IPHONE=iphones${VALUE}
   DIR_IPHONE_STRIP=iphones${VALUE}-strip
   mkdir ${DIR_ARM64}
   mkdir ${DIR_ARM64_STRIP}
   mkdir ${DIR_IPHONE}
   mkdir ${DIR_IPHONE_STRIP}
   mkdir ${DIR_ALL}
   mkdir ${DIR_ALL_STRIP}

   ###Full版本
   ZIP_PREFIX=${FRAMEWORK_NAME}${VALUE}_framework_${SDK_VERSION}
   SDK_ZIP_NAME_IPHONEOS=${ZIP_PREFIX}_release_ios_arm64.zip
   SDK_STRIP_ZIP_NAME_IPHONEOS=${ZIP_PREFIX}_release_ios_arm64_strip.zip

   SDK_ZIP_NAME_ALL=${ZIP_PREFIX}_release_ios_arm64_armv7_x64.zip
   SDK_STRIP_ZIP_NAME_ALL=${ZIP_PREFIX}_release_ios_arm64_armv7_x64_strip.zip

   SDK_ZIP_NAME_IPHONEOS_ALL=${ZIP_PREFIX}_release_ios_arm64_armv7.zip
   SDK_STRIP_ZIP_NAME_IPHONEOS_ALL=${ZIP_PREFIX}_release_ios_arm64_armv7_strip.zip

   echo "~~~~~~~~~~~~~~~~~~~开始编译iphoneos arm64~~~~~~~~~~~~~~~~~~~"
   cd $WORKSPACE/ios
   if [ "$VALUE" = "_movie" ]
    then
        echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
        pod install --no-repo-update
    elif [ "$VALUE" = "_movie_no_ffmpeg" ]
    then
       echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
       libpag_config=MOVIE_NOFFMPEG pod install --no-repo-update
    elif [ "$VALUE" = "_public" ]
    then
        echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
        libpag_config=PUBLIC_NO_FFMPEG pod install --no-repo-update
    else
        echo "~~~~~~~~~~~~~~~~none~~~~~~~~~~~~~~~~~~"
        libpag_config=NONE pod install --no-repo-update
    fi
   ###############开始编译app
    #clean
    xcodebuild clean -workspace $PROJECT_NAME.xcworkspace -scheme $PROJECT_NAME  -configuration $BUILD_CONFIG

    xcodebuild  -workspace $PROJECT_NAME.xcworkspace -scheme $PROJECT_NAME  -configuration $BUILD_CONFIG -sdk iphoneos -arch arm64 SYMROOT=$BUILD_APP_DIR
    #判断编译结果
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ iphoneos  arm64编译成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ iphoneos  arm64编译失败~~~~~~~~~~~~~~~~~~~"
    exit 1
    fi

    cd ${BUILD_APP_DIR}/${DIR_ARM64}

    ###Full 版本
    #拷贝arm64 framework到TEMP_DIR
    cp -a "${IPHONE_OS_DIR}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_ARM64}/${LIB_STATIC_NAME}"

    ###Full 版本
    zip -qr ${SDK_ZIP_NAME_IPHONEOS} ${LIB_STATIC_NAME}
    cp -a "${SDK_ZIP_NAME_IPHONEOS}" "${BUILD_APP_DIR}/${SDK_ZIP_NAME_IPHONEOS}"

    #STRIP包
    cd ${BUILD_APP_DIR}/${DIR_ARM64_STRIP}
    #拷贝arm64-strip framework到TEMP_DIR
    cp -a "${IPHONE_OS_DIR}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_ARM64_STRIP}/${LIB_STATIC_NAME}"

    ###Full 版本
    ${STRIP_DIR} -X ${LIB_STATIC_NAME}/${LIB_STATIC_NAME}

    zip -qr ${SDK_STRIP_ZIP_NAME_IPHONEOS} ${LIB_STATIC_NAME}
    cp -a "${SDK_STRIP_ZIP_NAME_IPHONEOS}" "${BUILD_APP_DIR}/${SDK_STRIP_ZIP_NAME_IPHONEOS}"
    cp -a "${SDK_STRIP_ZIP_NAME_IPHONEOS}" "${BUILD_APP_DIR_RELEASE}/${SDK_STRIP_ZIP_NAME_IPHONEOS}"

    echo "~~~~~~~~~~~~~~~~~~~开始编译iphoneos armv7+arm64~~~~~~~~~~~~~~~~~~~"
    rm -r $BUILD_APP_DIR/${BUILD_CONFIG}-iphoneos
    cd ${WORKSPACE}/ios
    xcodebuild  -workspace $PROJECT_NAME.xcworkspace -scheme $PROJECT_NAME  -configuration $BUILD_CONFIG -sdk iphoneos -arch armv7 -arch arm64 SYMROOT=$BUILD_APP_DIR
    #判断编译结果
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ iphoneos  armv7+arm64编译成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ iphoneos  armv7+arm64编译失败~~~~~~~~~~~~~~~~~~~"
    exit 1
    fi

    cd ${BUILD_APP_DIR}/${DIR_IPHONE}
    ###Full 版本
    ##copy arm64、armv7
    cp -a "${IPHONE_OS_DIR}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_IPHONE}/${LIB_STATIC_NAME}"
    zip -qr ${SDK_ZIP_NAME_IPHONEOS_ALL} ${LIB_STATIC_NAME}
    cp -a "${SDK_ZIP_NAME_IPHONEOS_ALL}" "${BUILD_APP_DIR}/${SDK_ZIP_NAME_IPHONEOS_ALL}"

    cp -a ${IPHONE_OS_DIR}/${LIB_STATIC_NAME}.dSYM ${BUILD_APP_DIR}/${LIB_STATIC_NAME}${VALUE}.dSYM
    zip -qr ${BUILD_APP_DIR}/${LIB_STATIC_NAME}${VALUE}.dSYM.zip ${BUILD_APP_DIR}/${LIB_STATIC_NAME}${VALUE}.dSYM


    zip -qr ${BUILD_APP_DIR}/${LIB_STATIC_NAME}${VALUE}.dSYM.zip ${BUILD_APP_DIR}/${LIB_STATIC_NAME}${VALUE}.dSYM

    cd ${BUILD_APP_DIR}/${DIR_IPHONE_STRIP}
    ###Full STRIP 版本
    ##copy arm64、armv7
    cp -a "${IPHONE_OS_DIR}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_IPHONE_STRIP}/${LIB_STATIC_NAME}"
    ${STRIP_DIR} -X ${LIB_STATIC_NAME}/${FRAMEWORK_NAME}
    zip -qr ${SDK_STRIP_ZIP_NAME_IPHONEOS_ALL} ${LIB_STATIC_NAME}
    cp -a "${SDK_STRIP_ZIP_NAME_IPHONEOS_ALL}" "${BUILD_APP_DIR}/${SDK_STRIP_ZIP_NAME_IPHONEOS_ALL}"
    cp -a "${SDK_STRIP_ZIP_NAME_IPHONEOS_ALL}" "${BUILD_APP_DIR_RELEASE}/${SDK_STRIP_ZIP_NAME_IPHONEOS_ALL}"

    echo "~~~~~~~~~~~~~~~~~~~iphonesimulator 增加FFMPEG~~~~~~~~~~~~~~~~~~~"
    cd ${WORKSPACE}/ios
    if [ "$VALUE" = "_movie" ]
    then
        echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
        pod install --no-repo-update
    elif [ "$VALUE" = "_movie_no_ffmpeg" ]
    then
       echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
       libpag_config=MOVIE_NOFFMPEG pod install --no-repo-update
    elif [ "$VALUE" = "_public" ]
    then
       echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
       libpag_config=PUBLIC pod install --no-repo-update
    else
        echo "~~~~~~~~~~~~~~~~none~~~~~~~~~~~~~~~~~~"
        libpag_config=FFMPEG pod install --no-repo-update

    fi
    echo "~~~~~~~~~~~~~~~~~~~开始编译iphonesimulator ~~~~~~~~~~~~~~~~~~~"
    ###############开始编译app
    xcodebuild  -workspace $PROJECT_NAME.xcworkspace -scheme $PROJECT_NAME  -configuration $BUILD_CONFIG -sdk iphonesimulator -arch x86_64 SYMROOT=$BUILD_APP_DIR
    #判断编译结果
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ iphonesimulator x86_64 编译成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ iphonesimulator x86_64 编译失败~~~~~~~~~~~~~~~~~~~"
    exit 1
    fi

    cd ${BUILD_APP_DIR}/${DIR_ALL}

    ##合并arm64、x86_64、armv7
    ###Full 版本
    #拷贝x86_64 framework到TEMP_DIR
    cp -a "${BUILD_APP_DIR}/${DIR_IPHONE}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_ALL}/${LIB_STATIC_NAME}"
    lipo -create ${BUILD_APP_DIR}/${DIR_ALL}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME} ${IPHONE_SIMULATOR_DIR}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME} -output ${BUILD_APP_DIR}/${DIR_ALL}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME}
    zip -qr ${SDK_ZIP_NAME_ALL} ${LIB_STATIC_NAME}
    cp -a "${SDK_ZIP_NAME_ALL}" "${BUILD_APP_DIR}/${SDK_ZIP_NAME_ALL}"

    cd ${BUILD_APP_DIR}/${DIR_ALL_STRIP}

    ##合并arm64、x86_64、armv7
    ###Full 版本
    #拷贝x86_64 framework到TEMP_DIR
    cp -a "${BUILD_APP_DIR}/${DIR_IPHONE_STRIP}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_ALL_STRIP}/${LIB_STATIC_NAME}"
    lipo -create ${BUILD_APP_DIR}/${DIR_ALL_STRIP}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME} ${IPHONE_SIMULATOR_DIR}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME} -output ${BUILD_APP_DIR}/${DIR_ALL_STRIP}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME}
    ${STRIP_DIR} -X ${LIB_STATIC_NAME}/${FRAMEWORK_NAME}
    zip -qr ${SDK_STRIP_ZIP_NAME_ALL} ${LIB_STATIC_NAME}
    cp -a "${SDK_STRIP_ZIP_NAME_ALL}" "${BUILD_APP_DIR}/${SDK_STRIP_ZIP_NAME_ALL}"
    cp -a "${SDK_STRIP_ZIP_NAME_ALL}" "${BUILD_APP_DIR_RELEASE}/${SDK_STRIP_ZIP_NAME_ALL}"

   return 0;
}

function LoopBuildStatic(){
   VALUE=$@
   echo "--LoopBuildStatic-input---:${VALUE}"
   STATIC_LIB_OPTION='MACH_O_TYPE=staticlib GCC_DEBUGGING_SYMBOLS=used GCC_GENERATE_DEBUGGING_SYMBOLS=NO'
   cd ${BUILD_APP_DIR}


   FFMPEG_FOLDER=sequence
   DIR_ARM64=arm64_static${VALUE}
   DIR_ARMv7=armv7_static${VALUE}
   DIR_ALL_STATIC=static${VALUE}
   DIR_IPHONE=iphones_static${VALUE}
   mkdir ${DIR_ARM64}
   mkdir ${DIR_ARMv7}
   mkdir ${DIR_IPHONE}
   mkdir ${DIR_ALL_STATIC}

   ###Full版本
   ZIP_PREFIX=${FRAMEWORK_NAME}_static${VALUE}_framework_${SDK_VERSION}
   SDK_ZIP_NAME_IPHONEOS_ALL=${ZIP_PREFIX}_release_ios_arm64_armv7.zip

   echo "~~~~~~~~~~~~~~~~~~~开始编译iphoneos arm64~~~~~~~~~~~~~~~~~~~"
   cd $WORKSPACE/ios
    if [ "$VALUE" = "_movie" ]
    then
        echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
        FFMPEG_FOLDER=movie
        pod install --no-repo-update
    elif [ "$VALUE" = "_movie_no_ffmpeg" ]
    then
       echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
       FFMPEG_FOLDER=movie_no_videodecoder
       libpag_config=MOVIE_NOFFMPEG pod install --no-repo-update
    elif [ "$VALUE" = "_public" ]
    then
        echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
        libpag_config=PUBLIC_NO_FFMPEG pod install --no-repo-update
    else
        echo "~~~~~~~~~~~~~~~~none~~~~~~~~~~~~~~~~~~"
        libpag_config=NONE pod install --no-repo-update
    fi
   ###############开始编译app
    #clean
    xcodebuild clean -workspace $PROJECT_NAME.xcworkspace -scheme $PROJECT_NAME  -configuration $BUILD_CONFIG

    xcodebuild  -workspace $PROJECT_NAME.xcworkspace -scheme $FRAMEWORK_NAME  -configuration $BUILD_CONFIG -sdk iphoneos -arch arm64 SYMROOT=$BUILD_APP_DIR $STATIC_LIB_OPTION
    #判断编译结果
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ iphoneos  arm64编译成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ iphoneos  arm64编译失败~~~~~~~~~~~~~~~~~~~"
    exit 1
    fi

    ###Full 版本
    #拷贝arm64 framework到TEMP_DIR
    cp -a "${IPHONE_OS_DIR}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_ARM64}/${LIB_STATIC_NAME}"

    cp -a "${IPHONE_OS_DIR}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_ALL_STATIC}/${LIB_STATIC_NAME}"

    xcodebuild  -workspace $PROJECT_NAME.xcworkspace -scheme $FRAMEWORK_NAME  -configuration $BUILD_CONFIG -sdk iphoneos -arch armv7 SYMROOT=$BUILD_APP_DIR $STATIC_LIB_OPTION
    #判断编译结果
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ iphoneos  armv7编译成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ iphoneos  armv7编译失败~~~~~~~~~~~~~~~~~~~"
    exit 1
    fi
    cp -a "${IPHONE_OS_DIR}/${LIB_STATIC_NAME}" "${BUILD_APP_DIR}/${DIR_ARMv7}/${LIB_STATIC_NAME}"

    echo "~~~~~~~~~~~~~~~~~~~iphonesimulator 增加FFMPEG~~~~~~~~~~~~~~~~~~~"
    cd ${WORKSPACE}/ios
    if [ "$VALUE" = "_movie" ]
    then
        echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
        pod install --no-repo-update
    elif [ "$VALUE" = "_movie_no_ffmpeg" ]
    then
       echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
       libpag_config=MOVIE_NOFFMPEG pod install --no-repo-update
    elif [ "$VALUE" = "_public" ]
    then
       echo "~~~~~~~~~~~~~~~~$VALUE~~~~~~~~~~~~~~~~~~"
       libpag_config=PUBLIC pod install --no-repo-update
    else
        echo "~~~~~~~~~~~~~~~~none~~~~~~~~~~~~~~~~~~"
        libpag_config=FFMPEG pod install --no-repo-update

    fi
    echo "~~~~~~~~~~~~~~~~~~~开始编译iphonesimulator ~~~~~~~~~~~~~~~~~~~"
    ###############开始编译app
    xcodebuild  -workspace $PROJECT_NAME.xcworkspace -scheme $FRAMEWORK_NAME  -configuration $BUILD_CONFIG -sdk iphonesimulator -arch x86_64 SYMROOT=$BUILD_APP_DIR $STATIC_LIB_OPTION
    #判断编译结果
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ iphonesimulator x86_64 编译成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ iphonesimulator x86_64 编译失败~~~~~~~~~~~~~~~~~~~"
    exit 1
    fi

    ##合并arm64、x86_64、armv7
    ###Full 版本
    #拷贝x86_64 framework到TEMP_DIR

    lipo ${WORKSPACE}/vendor/skia/ios/libskia.a -thin arm64 -o ${BUILD_APP_DIR}/libskia-arm64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavutil.a -thin arm64 -o ${BUILD_APP_DIR}/libavutil-arm64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavcodec.a -thin arm64 -o ${BUILD_APP_DIR}/libavcodec-arm64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavformat.a -thin arm64 -o ${BUILD_APP_DIR}/libavformat-arm64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libswresample.a -thin arm64 -o ${BUILD_APP_DIR}/libswresample-arm64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libswscale.a -thin arm64 -o ${BUILD_APP_DIR}/libswscale-arm64.a

    lipo ${WORKSPACE}/vendor/skia/ios/libskia.a -thin armv7 -o ${BUILD_APP_DIR}/libskia-armv7.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavutil.a -thin armv7 -o ${BUILD_APP_DIR}/libavutil-armv7.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavcodec.a -thin armv7 -o ${BUILD_APP_DIR}/libavcodec-armv7.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavformat.a -thin armv7 -o ${BUILD_APP_DIR}/libavformat-armv7.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libswresample.a -thin armv7 -o ${BUILD_APP_DIR}/libswresample-armv7.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libswscale.a -thin armv7 -o ${BUILD_APP_DIR}/libswscale-armv7.a

    lipo ${WORKSPACE}/vendor/skia/ios/libskia.a -thin x86_64 -o ${BUILD_APP_DIR}/libskia-x86_64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavutil.a -thin x86_64 -o ${BUILD_APP_DIR}/libavutil-x86_64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavcodec.a -thin x86_64 -o ${BUILD_APP_DIR}/libavcodec-x86_64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libavformat.a -thin x86_64 -o ${BUILD_APP_DIR}/libavformat-x86_64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libswresample.a -thin x86_64 -o ${BUILD_APP_DIR}/libswresample-x86_64.a
    lipo ${WORKSPACE}/vendor/ffmpeg/ios/$FFMPEG_FOLDER/libswscale.a -thin x86_64 -o ${BUILD_APP_DIR}/libswscale-x86_64.a

    libtool -static -o ${BUILD_APP_DIR}/${FRAMEWORK_NAME}-arm64.a ${BUILD_APP_DIR}/${DIR_ARM64}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME} ${BUILD_APP_DIR}/libskia-arm64.a  ${BUILD_APP_DIR}/libavcodec-arm64.a ${BUILD_APP_DIR}/libavformat-arm64.a ${BUILD_APP_DIR}/libavutil-arm64.a ${BUILD_APP_DIR}/libswresample-arm64.a ${BUILD_APP_DIR}/libswscale-arm64.a
    libtool -static -o ${BUILD_APP_DIR}/${FRAMEWORK_NAME}-armv7.a ${BUILD_APP_DIR}/${DIR_ARMv7}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME} ${BUILD_APP_DIR}/libskia-armv7.a  ${BUILD_APP_DIR}/libavcodec-armv7.a ${BUILD_APP_DIR}/libavformat-armv7.a ${BUILD_APP_DIR}/libavutil-armv7.a ${BUILD_APP_DIR}/libswresample-armv7.a ${BUILD_APP_DIR}/libswscale-armv7.a
    libtool -static -o ${BUILD_APP_DIR}/${FRAMEWORK_NAME}-x86_64.a ${IPHONE_SIMULATOR_DIR}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME} ${BUILD_APP_DIR}/libskia-x86_64.a ${BUILD_APP_DIR}/libavcodec-x86_64.a ${BUILD_APP_DIR}/libavformat-x86_64.a ${BUILD_APP_DIR}/libavutil-x86_64.a ${BUILD_APP_DIR}/libswresample-x86_64.a ${BUILD_APP_DIR}/libswscale-x86_64.a

    lipo -create -output ${BUILD_APP_DIR}/${DIR_ALL_STATIC}/${LIB_STATIC_NAME}/${FRAMEWORK_NAME} ${BUILD_APP_DIR}/${FRAMEWORK_NAME}-arm64.a ${BUILD_APP_DIR}/${FRAMEWORK_NAME}-x86_64.a ${BUILD_APP_DIR}/${FRAMEWORK_NAME}-armv7.a

    cd ${BUILD_APP_DIR}/${DIR_ALL_STATIC}
    zip -qr ${SDK_ZIP_NAME_IPHONEOS_ALL} ${LIB_STATIC_NAME}
    cp -a ${SDK_ZIP_NAME_IPHONEOS_ALL} ${BUILD_APP_DIR}/${SDK_ZIP_NAME_IPHONEOS_ALL}

   return 0;
}

if [ "$PAGBuildConfig" = "movie" ]; then
    FRAMEWORK_TAG=${SDK_VERSION}-movie-symbol
    FRAMEWORK_TAG_STRIP=${SDK_VERSION}-movie
    LoopBuild _movie
    pushPAGTag
    pushPAG_iOS

    FRAMEWORK_TAG=${SDK_VERSION}-noffmpeg-symbol
    FRAMEWORK_TAG_STRIP=${SDK_VERSION}-noffmpeg

    LoopBuild _movie_no_ffmpeg
    pushPAG_iOS

    FRAMEWORK_TAG=${SDK_VERSION}-noffmpeg-static
    LoopBuildStatic _movie_no_ffmpeg
    pushPAG_iOS static

    FRAMEWORK_TAG=${SDK_VERSION}-movie-static
    LoopBuildStatic _movie
    pushPAG_iOS static

    elif [ "$PAGBuildConfig" = "public" ]
    then
      LoopBuild _public
      pushPAGTag
      return;
    else
      LoopBuild
      pushPAGTag
      pushPAG_iOS
fi

updatePAGDocVersion
