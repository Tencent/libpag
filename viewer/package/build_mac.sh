# /bin/bash

function print() {
    local text="$1"
    local width=${2:-40}
    local textLength=${#text}
    local padingLength=$(((width - textLength) / 2))
    local padding=$(printf '%*s' $padingLength '')
    padding=${padding// /=}

    echo "${padding}${text}${padding}"
}

function createDmg()
{
    local creatDmg=${1}
    local sourcePath=${2}
    local dmgPath=${3}
    local iconPath=${4}

    ${creatDmg} \
    --volname "PAGViewer" \
    --volicon "${iconPath}" \
    --window-pos 200 120 \
    --window-size 800 400 \
    --icon-size 100 \
    --icon "PAGViewer.app" 200 190 \
    --hide-extension "PAGViewer.app" \
    --app-drop-link 600 185 \
    --skip-jenkins \
    "${dmgPath}" \
    "${sourcePath}"
}

# 1 初始化变量
print "[ 初始化变量 ]"
AppVersion=${MajorVersion}.${MinorVersion}.${BK_CI_BUILD_NO}
CurrentTime=$(date +"%Y%m%d%H%M%S")
RFCTime=$(date -R)
SourceDir=$(dirname "$(dirname "$(realpath "$0")")")
BuildDir="${SourceDir}/build_${CurrentTime}"
Deployqt="/usr/local/opt/qt@6/bin/macdeployqt"


# 2 编译
print "[ 编译 ]"

# 2.1 编译PAGViewer x86
print "[ 编译x86 ]"
x86BuildDir="${BuildDir}/build_x86"
if [ -d "${x86BuildDir}" ];
then
    rm -rf "${x86BuildDir}"
fi

cmake -S ${SourceDir} -B ${x86BuildDir} -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
if [ $? -ne 0 ];
then
    echo "构建PAGViewer x86失败"
    exit 1
fi

cmake --build ${x86BuildDir} --target PAGViewer -j 8
if [ $? -ne 0 ];
then
    echo "编译PAGViewer x86失败"
    exit 1
fi

# 2.2 编译PAGViewer arm
print "[ 编译arm64 ]"
armBuildDir="${BuildDir}/build_arm"
if [ -d "${armBuildDir}" ];
then
    rm -rf "${armBuildDir}"
fi

cmake -S ${SourceDir} -B ${armBuildDir} -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
if [ $? -ne 0 ];
then
    echo "构建PAGViewer arm64失败"
    exit 1
fi

cmake --build ${armBuildDir} --target PAGViewer -j 8
if [ $? -ne 0 ];
then
    echo "编译PAGViewer arm64失败"
    exit 1
fi


# 3 资源整理
# 3.1 合并PAGViewer
print "[ 合并PAGViewer ]"
AppDir="${BuildDir}/PAGViewer.app"
ExeDir="${AppDir}/Contents/MacOS"
ExePath="${ExeDir}/PAGViewer"
x86ExePath="${x86BuildDir}/PAGViewer"
armExePath="${armBuildDir}/PAGViewer"
if [ -d "${AppDir}" ];
then
    rm -rf "${AppDir}"
fi

mkdir -p ${ExeDir}
lipo -create ${x86ExePath} ${armExePath} -output ${ExePath}

# 3.2 获取PAGViewer的依赖
print "[ 获取PAGViewer的依赖 ]"
${Deployqt} ${AppDir} -qmldir=${SourceDir}/qml
if [ $? -ne 0 ];
then
    echo "获取依赖失败"
    exit 1
fi

FrameworkDir="${AppDir}/Contents/Frameworks"
SparklePath="${SourceDir}/vendor/sparkle/mac/Sparkle.framework"
cp -f -R -P ${SparklePath} ${FrameworkDir}

# 3.3 设置动态库加载路径
print "[ 设置动态库加载路径 ]"
# todo delete redundant paths
install_name_tool -delete_rpath "${SourceDir}/vendor/sparkle/mac" ${ExePath}

# 3.4 生成公钥和私钥
print "[ 生成公钥和私钥 ]"

# 3.4.1 生成DSA公钥和私钥
SignForUpdate=false
DSACert="${SourceDir}/package/certs/PAGViewer-Updater-DSA.p12"
DSAPublicKey="${BuildDir}/DSA-Public-Key.pem"
DSAPrivateKey="${BuildDir}/DSA-Private-Key.pem"
DSACertPassword=$(security find-generic-password -ws "PAGViewer-Updater-DSA-PASSWORD")
if [ -n "${DSACertPassword}" ];
then
    openssl pkcs12 -in "${DSACert}" -clcerts -nokeys -nodes -password pass:"${DSACertPassword}" | openssl x509 -pubkey -noout > "${DSAPublicKey}"
    if [ $? -ne 0 ];
    then
        echo "Failed to get DSA public key from ${DSACert}"
        exit 1
    fi

    openssl pkcs12 -in "${DSACert}" -nocerts -nodes -password pass:"${DSACertPassword}" -out "${DSAPrivateKey}"
    if [ $? -ne 0 ];
    then
        echo "Failed to get DSA private key from ${DSACert}"
        exit 1
    fi
fi

# 3.4.2 生成EDDSA公钥和私钥
EDDSACert="${SourceDir}/package/certs/PAGViewer-Updater-EDDSA.p12"
EDDSAPrivateKey="${BuildDir}/EDDSA-Private-Key.pem"
EDDSACertPassword=$(security find-generic-password -ws "PAGViewer-Updater-EDDSA-PASSWORD")
if [ -n "${EDDSACertPassword}" ];
then
    openssl pkcs12 -in "${EDDSACert}" -nocerts -nodes -password pass:"${EDDSACertPassword}" -out "${EDDSAPrivateKey}"
    if [ $? -ne 0 ];
    then
        echo "Failed to get EDDSA private key from ${EDDSACert}"
        exit 1
    fi
fi

if [ -n "${DSACertPassword}" ] && [ -n "${EDDSACertPassword}" ];
then
    SignForUpdate=true
fi

# 3.5 拷贝资源文件
print "[ 拷贝资源文件 ]"
ContentsDir="${AppDir}/Contents"
PlistPath=${SourceDir}/package/templates/Info.plist
cp -f ${PlistPath} ${ContentsDir}

ResourcesDir="${AppDir}/Contents/Resources"
cp -f ${SourceDir}/images/appIcon.icns ${ResourcesDir}
cp -f ${SourceDir}/images/pagIcon.icns ${ResourcesDir}
cp -f ${DSAPublicKey} ${ResourcesDir}
# todo 插件等其他资源

# 3.6 更新配置文件
print "[ 更新配置文件 ]"
DSAPublicKeyName=$(basename "${DSAPublicKey}")
SUPublicEDKey="UVl4rEDle9tT48LtonRXDADX92s2PsXKDqsB4SA6Fr8="
/usr/libexec/Plistbuddy -c "Set CFBundleVersion ${AppVersion}" "${ContentsDir}/Info.plist"
/usr/libexec/Plistbuddy -c "Set CFBundleShortVersionString ${AppVersion}" "${ContentsDir}/Info.plist"
/usr/libexec/Plistbuddy -c "Set SUPublicDSAKeyFile ${DSAPublicKeyName}" "${ContentsDir}/Info.plist"
/usr/libexec/Plistbuddy -c "Set SUPublicEDKey ${SUPublicEDKey}" "${ContentsDir}/Info.plist"


# 4 签名
print "[ 签名 ]"
SignCertName="Developer ID Application: Tencent Technology (Shanghai) Co., Ltd (FN2V63AD2J)"
if security find-certificate -c "${SignCertName}" >/dev/null 2>&1;
then
    # 4.1 签名PAGViewer.app
    print "[ 签名PAGViewer ]"
    EntitlementsPath="${SourceDir}/package/templates/PAGViewer.entitlements"
    NeedSignFiles=(
        "${ExePath}" 
        "${FrameworkDir}/Sparkle.framework/Versions/Current/Autoupdate" 
        "${FrameworkDir}/Sparkle.framework/Versions/Current/Sparkle" 
        "${FrameworkDir}/Sparkle.framework/Versions/Current/XPCServices/Downloader.xpc" 
        "${FrameworkDir}/Sparkle.framework/Versions/Current/XPCServices/Installer.xpc"
    )

    for NeedSignFile in ${NeedSignFiles[@]};
    do
        codesign --deep --force --entitlements ${EntitlementsPath} --timestamp --options "runtime" --sign "${SignCertName}" "${NeedSignFile}"
    done
    
    # 4.2 验证签名
    print "[ 验证签名 ]"
    codesign -vvv --deep "${AppDir}"

    # 4.3 公正PAGViewer.app
    (
        print "[ 公正PAGViewer ]"

        # 4.3.1 压缩PAGViewer.app
        print "[ 压缩PAGViewer ]"
        cd "${BuildDir}"
        KeychainProfile="AC_PASSWORD"
        TempDir="Applications"
        TempZip="Applications.zip"

        if [ -f "${TempZip}" ];
        then
            rm -f "${TempZip}"
        fi

        if [ -d "${TempDir}" ];
        then
            rm -rf "${TempDir}"
        fi

        mkdir -p "${TempDir}"
        cp -fRP "${AppDir}" "${TempDir}"
        zip --symlinks -r -q -X "${TempZip}" "${TempDir}"

        # 4.3.2 提交PAGViewer.app
        print "[ 提交PAGViewer ]"
        xcrun notarytool submit --keychain-profile "${KeychainProfile}" --wait "${TempZip}" 2>&1 | tee notarize.log
        cat notarize.log
        if [ $? -ne 0 ];
        then
            echo "Failed to submit app for notarization."
            exit 1
        fi

        UUID=$(cat notarize.log | grep -Eo '\w{8}-(\w{4}-){3}\w{12}' | head -n 1)
        echo "Submit app successfully. UUID: ${UUID}"

        # 4.3.3 验证公正结果
        print "[ 验证公正结果 ]"
        while true;
        do
            xcrun notarytool info "${UUID}" --keychain-profile "${KeychainProfile}"  2>&1 | tee validate_notarize.log
            Status=$(cat validate_notarize.log | grep "status" | awk '{print $2}' | tr -d '"')
            echo "Current notarization[${UUID}] status: ${Status}"
            Status=$(echo "$Status" | tr '[:lower:]' '[:upper:]')
            if [ "${Status}" == "ACCEPTED" ];
            then
                echo "Succeeded to notarize app."
                break;
            elif [ "${Status}" == "INVALID" ];
            then
                echo "Failed to notarize app."
                cat validate_notarize.log
                exit 1
            else
                echo "Notarization is not completed yet. Wait for 20 seconds..."
                sleep 20
            fi
        done

        # 4.3.4 附加公正Ticket
        print "[ 附加公正Ticket ]"
        xcrun stapler staple "${AppDir}"

        # 4.3.5 确保Ticket已附加
        print "[ 确保Ticket已附加 ]"
        xcrun stapler validate "${AppDir}"
    )
fi


# 5 打包PAGViewer.dmg
print "[ 打包PAGViewer.dmg ]"

# 5.1 更新Appcast.xml
print "[ 更新Appcast.xml ]"
if [ "${SignForUpdate}" == true ];
then
    (
        cd "${BuildDir}"

        ZipFile="PAGViewer.zip"
        if [ -f "${ZipFile}" ];
        then
            rm -f "${ZipFile}"
        fi

        zip --symlinks -r -q -X "${ZipFile}" "${AppDir}"

        SignScript="${SourceDir}/package/sign_update.sh"

        ${SignScript} ${BuildDir}/${ZipFile} ${DSAPrivateKey} > DSASignHash.txt
        ZipDSAHash=$(tr '\n' ' ' < DSASignHash.txt | sed '$s/ //g')
        echo "Get Zip DSA Hash: ${ZipDSAHash}"

        ${SignScript} ${BuildDir}/${ZipFile} ${EDDSAPrivateKey} > EDDSASignHash.txt
        ZipEDDSAHash=$(tr '\n' ' ' < EDDSASignHash.txt | sed '$s/ //g')
        echo "Get Zip EDDSA Hash: ${ZipEDDSAHash}"

        ZipLength=$(stat -f%z ${BuildDir}/${ZipFile})

        URL=$(curl -s https://pag.qq.com/server.html)
        if [ "${isBetaVersion}" == true ];
        then
            URL="${URL}beta/"
        fi
        URL="${URL}${ZipFile}"

        cp -f "${SourceDir}/package/templates/PagAppcast.xml" "${BuildDir}/PagAppcast.xml"

        sed -i '' "s|~url~|${URL}|g" "${BuildDir}/PagAppcast.xml"
        sed -i '' "s|~time~|${RFCTime}|g" "${BuildDir}/PagAppcast.xml"
        sed -i '' "s|~version~|${AppVersion}|g" "${BuildDir}/PagAppcast.xml"
        sed -i '' "s|~zipDSAHash~|${ZipDSAHash}|g" "${BuildDir}/PagAppcast.xml"
        sed -i '' "s|~zipEDDSAHash~|${ZipEDDSAHash}|g" "${BuildDir}/PagAppcast.xml"
        sed -i '' "s|~zipLength~|${ZipLength}|g" "${BuildDir}/PagAppcast.xml"
    )
fi

# 5.2 生成dmg
print "[ 生成dmg ]"
if [ -d "${BuildDir}/dmg_content" ];
then
    rm -rf "${BuildDir}/dmg_content"
fi
mkdir -p "${BuildDir}/dmg_content"
cp -R -P "${AppDir}" "${BuildDir}/dmg_content"

CreateDmgTool="${SourceDir}/tools/create-dmg/create-dmg"

for ((i=1; i < 4; i++));
do
    createDmg "${CreateDmgTool}" "${BuildDir}/dmg_content" "${BuildDir}/PAGViewer.dmg" "${SourceDir}/images/dmgIcon.icns"
    if [  $? -eq 0 ];
    then
        echo "create dmg success"
        break
    else
        echo "create dmg failed"
        continue;
    fi
done

