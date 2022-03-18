#!/bin/bash

# 只对master和release分支的编译推送到demo工程
GIT_BRANCH=""
AAR_TAG=""
if [[ ${hookBranch} == "master" ]]
then
    AAR_TAG=dev/v${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}
    GIT_BRANCH=develop
elif [[ ${hookBranch} == "release" ]]
then
    AAR_TAG=v${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}
    GIT_BRANCH=master
fi

echo "GIT_BRANCH:${GIT_BRANCH}"
echo "AAR_TAG:${AAR_TAG}"

WORKSPACE=$(pwd)
LIBPAG_ANROID_PATH=${WORKSPACE}/android
BIN_PATH=${WORKSPACE}/bin
SAMPLE_PATH=${WORKSPACE}/pag-android

if [ -e release ] ;then
rm -rf release;
fi
mkdir release

# add dev prefix
DEV_BUILD_PRO="";

# include armeabi so path
MAVEN_OUT_PATH="${LIBPAG_ANROID_PATH}/maven-out"
g_MAVEN_LIBPAG_RELESE_AAR="${MAVEN_OUT_PATH}/libpag-release_maven.aar"

g_LIBPAG_RELESE_OUT_AAR="libpag/build/outputs/aar/libpag-release.aar"
g_LIBPAG_RELESE_AAR="libpag_${DEV_BUILD_PRO}release_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}.aar"
g_RELEASE_AAR_PRE="libpag_${DEV_BUILD_PRO}release_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}"

# need pass libpag android path
buildLibpag() {
    echo "~~~~~~libpag 编译: gradle build~~~~~~"
    echo "[params] libpag android path: $1, noFfempeg: $2, isArm64v8a: $3, gradlePath: $4 pagMovie: $5"
    echo "cd $1"
    cd $1
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    echo "rm -rf libpag/build/outputs"
    rm -rf libpag/build/outputs
    echo "rm -rf app/build/outputs"
    rm -rf app/build/outputs
    rm -rf libpag/.externalNativeBuild

    echo "reset android config: "
    echo "git checkout ."
    # 由于在updateToMaven的时候，修改了libpag/build.gradle文件，在编译ffempeg版本的时候需要重置
    git checkout .

    # 是否需要开启上报
    handleDataReporter

    #修改libpag的版本信息
    old_version='3.0.0.1'
    new_version="${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}"
    sed -i "" "s:${old_version}:${new_version}:" ../src/rendering/PAG.cpp
    echo "PAG信息：" `cat ../src/rendering/PAG.cpp`

    # 处理是否需要开启FFMpeg
    if [ $2 -gt 0 ];
    then
        sed -i "" "s:PAG_USE_FFMPEG=ON:PAG_USE_FFMPEG=OFF:" libpag/build.gradle
    else
        sed -i "" "s:PAG_USE_FFMPEG=OFF:PAG_USE_FFMPEG=ON:" libpag/build.gradle
    fi

    # 处理是否需要开启PAGMovie
    if [ $5 -eq 1 ];
    then
        sed -i "" "s:PAG_USE_MOVIE=OFF:PAG_USE_MOVIE=ON:" libpag/build.gradle
        echo "build PAGMovie"
    else
        sed -i "" "s:PAG_USE_MOVIE=ON:PAG_USE_MOVIE=OFF:" libpag/build.gradle
    fi

    APP_GRADLE=`cat app/build.gradle`
    echo "app/build.gradle:"
    echo ${APP_GRADLE}

    LIBPAG_GRADLE=`cat libpag/build.gradle`
    echo "libpag/build.gradle: "
    echo ${LIBPAG_GRADLE}

    if [ $4 -gt 0 ];
    then
        echo "cd gradlePath: $4"
        cd $4
        echo "~~~~~~~~~~~~~~~~~~~gradle build目录:" `pwd`
    fi

    echo "~~~~~~libpag 编译: gradle build~~~~~~"
    gradle -v
    gradle assembleRelease

    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ gradle build成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ gradle build失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi
}

# need pass libpag android path
exportLibpag() {
    echo "~~~~~~导出libpag aar文件开始: ~~~~~~"
    echo "[params] libpag android path: $1, isLite: $2, isFfempeg: $3, isSaveMavenLibpagAAR: $4, pagMovie:$5"

    echo "cd $1"
    cd $1
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    PAG_PRE="libpag"
    if [[ $2 -gt 0 ]];
    then
        PAG_PRE="libpag-lite"
    fi

    FFEMPEG_PRE="-no-ffmpeg"
    if [[ $3 -gt 0 ]];
    then
        FFEMPEG_PRE=""
    fi

    PAGMOVIE_PRE="-pag-movie"
    if [[ $5 -eq 1 ]];
    then
        FFEMPEG_PRE=""
    else
        PAGMOVIE_PRE=""
    fi

    # 备份生成的aar
    outputAar=${PAG_PRE}/build/outputs/aar/${PAG_PRE}-release.aar
    backupAar=${BIN_PATH}/${PAG_PRE}${PAGMOVIE_PRE}${FFEMPEG_PRE}_${DEV_BUILD_PRO}aar_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_release_android_armeabi_armv7a_arm64v8a.aar
    echo "backupAar:${backupAar}"
    cp -f ${outputAar} ${backupAar}

    if [[ $4 -gt 0 ]];
    then
        echo "~~~~~~~~~~~~~~~~~~~ 拷贝aar文件到临时文件，以备maven上传时使用~~~~~~~~~~~~~~~~~~~"
        rm -rf ${g_MAVEN_LIBPAG_RELESE_AAR}
        cp -r ${outputAar} ${g_MAVEN_LIBPAG_RELESE_AAR}
        echo "~~~~~~~~~~~~~~~~~~~ 拷贝aar文件到临时文件结束~~~~~~~~~~~~~~~~~~~"

        echo "~~~~~~~~~~~~~~~~~~~ 拷贝obj到bin目录，以备rdm查询Crash源码~~~~~~~~~~~~~~~~~~~"
        BIN_OBJ_ZIP="${BIN_PATH}/${PAG_PRE}${PAGMOVIE_PRE}${FFEMPEG_PRE}_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_obj.zip"
        OBJ_ZIP="${PAG_PRE}${PAGMOVIE_PRE}${FFEMPEG_PRE}_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_obj.zip"
        OBJ_DIR="${PAG_PRE}/build/intermediates/cmake"
        echo "${BIN_OBJ_ZIP}"
        echo "${OBJ_ZIP}"
        echo "${OBJ_DIR}"

        zip -qr ${OBJ_ZIP} ${OBJ_DIR}
        cp ${OBJ_ZIP} ${BIN_OBJ_ZIP}
        rm -rf ${OBJ_ZIP}
        echo "~~~~~~~~~~~~~~~~~~~ 拷贝obj到bin目录结束~~~~~~~~~~~~~~~~~~~"
    fi
}

# need pass libpag android path
exportLibpagDoc() {
    if [[ "${hookBranch}" != "release" ]] ;then
        return;
    fi
    echo "~~~~~~~~~~~~~~~~~~~开始导出libpag文档~~~~~~~~~~~~~~~~~~~"
    echo "libpag android path: $1"
    echo "cd $1"
    cd $1
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    JAVA_DOC_ZIP="libpag_${DEV_BUILD_PRO}${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_android_docs.zip"

    javadoc \
      -locale en_US \
      -windowtitle libpag_android_docs \
      -nodeprecatedlist \
      -nohelp \
      -quiet \
      -d libpag_android_docs \
      libpag/src/main/java/org/libpag/*.java

    zip -qr ${JAVA_DOC_ZIP} "libpag_android_docs"
    cp ${JAVA_DOC_ZIP} "${BIN_PATH}/${JAVA_DOC_ZIP}"

    echo "~~~~~~~~~~~~~~~~~~~结束导出libpag文档~~~~~~~~~~~~~~~~~~~"
}

exportLibpagLiteDoc() {
    if [[ "${hookBranch}" != "release" ]] ;then
        return;
    fi
    echo "~~~~~~~~~~~~~~~~~~~开始导出libpag-lite文档~~~~~~~~~~~~~~~~~~~"
    echo "libpag android path: $1"
    echo "cd $1"
    cd $1
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    JAVA_DOC_ZIP="libpag-lite_${DEV_BUILD_PRO}${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_android_docs.zip"

    javadoc \
      -locale en_US \
      -windowtitle libpag-lite_android_docs \
      -nodeprecatedlist \
      -nohelp \
      -quiet \
      -d libpag-lite_android_docs \
      libpag-lite/src/main/java/org/libpag/*.java

    zip -qr ${JAVA_DOC_ZIP} "libpag-lite_android_docs"
    cp ${JAVA_DOC_ZIP} "${BIN_PATH}/${JAVA_DOC_ZIP}"

    echo "~~~~~~~~~~~~~~~~~~~结束导出libpag-lite文档~~~~~~~~~~~~~~~~~~~"
}

# need pass libpag android path
copyLibpagKeepFile() {
    echo "~~~~~~~~~~~~~~~~~~~开始拷贝keep文件~~~~~~~~~~~~~~~~~~~"
    echo "libpag android path: $1"
    echo "cd $1"
    cd $1
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    KeepFile="libpag/src/main/java/org/libpag/libpag_class_keep.txt"
    JAVA_KEEP_FILE="libpag_${DEV_BUILD_PRO}${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_android_class_keep.txt"

    cp ${KeepFile} "${BIN_PATH}/${JAVA_KEEP_FILE}"
    echo "~~~~~~~~~~~~~~~~~~~结束拷贝keep文件~~~~~~~~~~~~~~~~~~~"
}

# need pass libpag android path
copyLibpagLiteKeepFile() {
    echo "~~~~~~~~~~~~~~~~~~~开始拷贝keep文件~~~~~~~~~~~~~~~~~~~"
    echo "libpag android path: $1"
    echo "cd $1"
    cd $1
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    KeepFile="libpag-lite/src/main/java/org/libpag/lite/libpag-lite_class_keep.txt"
    JAVA_KEEP_FILE="libpag-lite_${DEV_BUILD_PRO}${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_android_class_keep.txt"

    cp ${KeepFile} "${BIN_PATH}/${JAVA_KEEP_FILE}"
    echo "~~~~~~~~~~~~~~~~~~~结束拷贝keep文件~~~~~~~~~~~~~~~~~~~"
}

# need pass libpag android path
uploadToMaven() {
    echo "~~~~~~~~~~~~~~~~~~~开始上传aar到maven~~~~~~~~~~~~~~~~~~~"
    echo "libpag android path: $1"
    echo "cd $1"
    cd $1
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    echo "reset android config: "
    echo "git checkout ."
    git checkout .

    sed -i "" "s/version = '.*'/version = '${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}'/" libpag/build.gradle

    artifact_old='artifact bundleReleaseAar'
    artifacts_new='artifacts = [file("../maven-out/libpag-release_maven.aar")]'

    echo 'update the abiFilters config: artifacts = [file("../maven-out/libpag-release_maven.aar")]'
    sed -i "" "s:${artifact_old}:${artifacts_new}:" libpag/build.gradle

    echo "cd libapg"
    cd libpag
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`
    echo "gradle publishAarPublicationToMavenRepository"
    gradle publishAarPublicationToMavenRepository
    echo "~~~~~~~~~~~~~~~~~~~结束上传aar到maven~~~~~~~~~~~~~~~~~~~"
}

uploadToJcenter() {
    echo "~~~~~~~~~~~~~~~~~~~开始上传aar到Jcenter~~~~~~~~~~~~~~~~~~~"
    cd ${LIBPAG_ANROID_PATH}/libpag/
    sed -i "" "s/version = '.*'/version = '${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}'/" build.gradle
    sed -i "" "s:web-proxy.oa.com:devnet-proxy.oa.com:" ../gradle.properties
    ../gradlew --scan --stacktrace clean build bintrayUpload -PbintrayUser=libpag -PbintrayKey=73aa993d8fdbc480bcaa53bd45ce95c2138a4b2c -PdryRun=false
    echo "~~~~~~~~~~~~~~~~~~~结束上传aar到Jcenter~~~~~~~~~~~~~~~~~~~"
}

buildAndUpdateSample() {
    if [[ "${hookBranch}" != "release" ]] ;then
        return;
    fi

    echo "~~~~~~~~~~~~~~~~~~~开始编译Sample~~~~~~~~~~~~~~~~~~~"
    cd ${WORKSPACE}
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    if [ -e pag-android ] ;then
        rm -rf pag-android;
    fi

    echo "拉取 pag-android 的代码"
    git clone -b master https://${DomainName}/CameraToolsGroup/pag-android.git
    cd pag-android
    sed -i "" "s/'com.tencent.tav:libpag:.*'/'com.tencent.tav:libpag:${MajorVersion}.${MinorVersion}
    .${FixVersion}.${BuildNo}'/" ./decoder_sample/app/build.gradle
    sed -i "" "s/'com.tencent.tav:libpag:.*'/'com.tencent.tav:libpag:${MajorVersion}.${MinorVersion}
    .${FixVersion}.${BuildNo}'/" ./lottie_sample/app/build.gradle
    sed -i "" "s/'com.tencent.tav:libpag:.*'/'com.tencent.tav:libpag:${MajorVersion}.${MinorVersion}
    .${FixVersion}.${BuildNo}'/" ./sample/app/build.gradle
    echo "cat ./sample/app/build.gradle"
    echo `cat ./sample/app/build.gradle`

    #进入项目目录
    echo "~~~~~~进入sample目录：cd sample~~~~~~"
    cd ${WORKSPACE}/pag-android/sample

    #sample编译
    echo "~~~~~~sample 编译: gradle build~~~~~~"
    gradle build

    if test $? -eq 0
    then
    echo "~~~~~~~~~~~~~~~~~~~ 编译sample成功~~~~~~~~~~~~~~~~~~~"
    else
    echo "~~~~~~~~~~~~~~~~~~~ 编译sample失败~~~~~~~~~~~~~~~~~~~"
    exit -1
    fi

    #进入项目目录
    echo "~~~~~~进入decoder_sample目录：cd decoder_sample~~~~~~"
    cd ${WORKSPACE}/pag-android/decoder_sample

    echo cmake.dir="/data/bkdevops/apps/android-sdk/cmake" > ./local.properties

    echo "echo ./local.properties"
    echo `cat ./local.properties`

    echo "~~~~~~decoder_sample 编译: gradle build~~~~~~"
    gradle build

    if test $? -eq 0
    then
    echo "~~~~~~~~~~~~~~~~~~~ 编译decoder_sample成功~~~~~~~~~~~~~~~~~~~"
    else
    echo "~~~~~~~~~~~~~~~~~~~ 编译decoder_sample失败~~~~~~~~~~~~~~~~~~~"
    exit -1
    fi
    echo "~~~~~~~~~~~~~~~~~~~结束编译Sample~~~~~~~~~~~~~~~~~~~"


    cd ${WORKSPACE}/pag-android
    git add ./decoder_sample/app/build.gradle
    git add ./lottie_sample/app/build.gradle
    git add ./sample/app/build.gradle

    echo "~~~~~~~~~~~~~~~~~~~PUSH 开始~~~~~~~~~~~~~~~~~~~"
    git commit -m "Push From Landun: update SDK"
    git tag -a ${AAR_TAG} -m "Push From Landun: update SDK"
    echo "git tag -a ${AAR_TAG} -m Push From Landun: update SDK"
    git push origin ${GIT_BRANCH} ${AAR_TAG}

    echo "~~~~~~~~~~~~~~~~~~~PUSH 结束~~~~~~~~~~~~~~~~~~~"
}

pushReleaseTag() {
    if [[ "${pushTag}" == true ]]; then
        return;
    fi
    if [[ "${hookBranch}" != "release" ]] && [[ "${hookBranch}" != "master" ]] ;then
        return;
    fi
    cd ${WORKSPACE}
    releaseData=${WORKSPACE}/release

    if [[ "${forLightSDK}" = true ]]; then
        cp ${BIN_PATH}/libpag-pag-movie_${DEV_BUILD_PRO}aar_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_release_android_armeabi_armv7a_arm64v8a.aar ${releaseData}/
    else
        cp ${BIN_PATH}/libpag-no-ffmpeg_${DEV_BUILD_PRO}aar_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_release_android_armeabi_armv7a_arm64v8a.aar ${releaseData}/
        cp ${BIN_PATH}/libpag_${DEV_BUILD_PRO}aar_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_release_android_armeabi_armv7a_arm64v8a.aar ${releaseData}/
    fi

    cp  {BIN_PATH}/libpag_${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}_obj.zip ${releaseData}/
    ls ${releaseData}
}

updatePAGDocVersion() {
    if [[ "${hookBranch}" != "release" ]] ;then
        return;
    fi

    echo "~~~~~~~~~~~~~~~~~~~开始更新PAG Doc版本~~~~~~~~~~~~~~~~~~~"
    cd ${WORKSPACE}
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    if [[ -e pag-docs ]] ;then
        rm -rf pag-docs;
    fi

    echo "拉取 pag-android 的代码"
    git clone -b master https://${DomainName}/shadow_frontend/PAG-docs.git pag-docs

    cd pag-docs
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`

    sed -i "" "s/'com.tencent.tav:libpag:.*'/'com.tencent.tav:libpag:${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}'/" ./docs/sdk-guide/install-sdk.md
    sed -i "" "s/implementation(name: '.*', ext: 'aar')/implementation(name: 'libpag-${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNo}', ext: 'aar')/" ./docs/sdk-guide/install-sdk.md
    git add ./docs/sdk-guide/install-sdk.md
    git commit -m "update pag android version from RDM"
    git push
    cd ${WORKSPACE}

    echo "~~~~~~~~~~~~~~~~~~~结束更新PAG Doc版本~~~~~~~~~~~~~~~~~~~"
}

build() {
    echo "~~~~~~~~~~~~~~~~~~~编译开始~~~~~~~~~~~~~~~~~~~"
    echo "libpag android path: $1"
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`
    init

    if [[ "${forLightSDK}" = true ]]; then
        buildForLightSDK $1
    else
        echo "build for normal"
        buildForNormal $1
    fi

    # 上传aar文件到软件源
    echo "forJcenter=${forJcenter}"
    if [[ "${forJcenter}" = true ]]; then
        uploadToJcenter $1
    else
        uploadToMaven $1
    fi
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ 上传软件源成功 ~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ 上传软件源失败 ~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi

    pushReleaseTag

    updatePAGDocVersion

    # 导出libpag doc文件
    echo "导出libpag doc文件"
    exportLibpagDoc $1
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ exportLibpagDoc成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ exportLibpagDoc失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi

    # 拷贝libpag keepFile文件
    echo "拷贝libpag keepFile文件"
    copyLibpagKeepFile $1
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ 拷贝libpag keepFile文件成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ 拷贝libpag keepFile文件失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi

    # 构建sample验证, 并推送到服务器
    echo "构建sample验证"
    buildAndUpdateSample
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ buildAndUpdateSamplee成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ buildAndUpdateSample失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi

    echo "~~~~~~~~~~~~~~~~~~编译结束~~~~~~~~~~~~~~~~~~~"
}

buildForLightSDK() {
##############################PAGMovie版本##########################################
    # [params] libpag android path: $1, noFfempeg: $2, isArm64v8a: $3, gradlePath: $4, pagMovie:$4
    echo "build for ligthSDK"
    buildLibpag $1 0 1 0 1
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ buildLibpag-pagMovie成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ buildLibpag-pagMovie失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi

    # 导出libpag aar文件
    # [params] libpag android path: $1, isLite: $2, isFfempeg: $3, isSaveMavenLibpagAAR: $4, pagMovie:$4
    echo "导出libpag-pagMovie aar文件"
    exportLibpag $1 0 1 1 1
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ exportLibpag-pagMovie 成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ exportLibpag-pagMovie 失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi
##############################PAGMovie版本##########################################
}


buildForNormal() {
    echo "~~~~~~~~~~~~~~~~~~~编译开始~~~~~~~~~~~~~~~~~~~"
    echo "libpag android path: $1"
    echo "~~~~~~~~~~~~~~~~~~~当前目录:" `pwd`
    init

    # 编译64位版本
    # [params] libpag android path: $1, noFfempeg: $2, isArm64v8a: $3, gradlePath: $4
    echo "编译没有ffmpeg的版本"
    buildLibpag $1 1 1 0
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ buildLibpag-no-ffmpeg成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ buildLibpag-no-ffmpeg失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi

    # 导出libpag aar文件
    # [params] libpag android path: $1, isLite: $2, isFfempeg: $3, isSaveMavenLibpagAAR: $4
    echo "导出libpag-no-ffmpeg aar文件"
    exportLibpag $1 0 0 0
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ exportLibpag-no-ffmpeg 成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ exportLibpag-no-ffmpeg 失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi

    # 编译带ffmpeg的64位版本
    # [params] libpag android path: $1, noFfempeg: $2, isArm64v8a: $3, gradlePath: $4
    echo "编译带ffmpeg的版本"
    buildLibpag $1 0 1 0
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ buildLibpag-ffmpeg成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ buildLibpag-ffmpeg失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi

    # 导出libpag aar文件
    # [params] libpag android path: $1, isLite: $2, isFfempeg: $3, isSaveMavenLibpagAAR: $4
    echo "导出libpag-ffmpeg aar文件"
    exportLibpag $1 0 1 1
    if test $? -eq 0
    then
        echo "~~~~~~~~~~~~~~~~~~~ exportLibpag-ffmpeg 成功~~~~~~~~~~~~~~~~~~~"
    else
        echo "~~~~~~~~~~~~~~~~~~~ exportLibpag-ffmpeg 失败~~~~~~~~~~~~~~~~~~~"
        exit -1
    fi
}

init() {
    echo "初始化 init 开始"

    if [[ -e ${MAVEN_OUT_PATH} ]] ;then
        rm -rf ${MAVEN_OUT_PATH};
    fi
    mkdir ${MAVEN_OUT_PATH}
    echo "maven out path: ${MAVEN_OUT_PATH}"

    if [[ -e ${BIN_PATH} ]] ;then
        rm -rf ${BIN_PATH};
    fi
    mkdir ${BIN_PATH}
    echo "bin path: ${BIN_PATH}"

    echo "初始化 init 结束"
}

handleDataReporter() {
    if [[ "${forJcenter}" = true ]]; then
        rm -rf ${LIBPAG_ANROID_PATH}/libpag/src/main/java/org/libpag/reporter
        sed -i "" "/PAGReporter-OnReportData/,+9d" ${LIBPAG_ANROID_PATH}/libpag/src/platform/android/NativePlatform.cpp
        sed -i "" "/com.tencent.beacon/d" ${LIBPAG_ANROID_PATH}/libpag/build.gradle
    fi
}

build ${LIBPAG_ANROID_PATH}
