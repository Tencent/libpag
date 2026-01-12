#!/bin/bash

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
    local backgroundPath=${5}

    ${creatDmg} \
    --volname "PAGViewer" \
    --volicon "${iconPath}" \
    --background "${backgroundPath}" \
    --window-pos 200 120 \
    --window-size 800 400 \
    --hide-extension "PAGViewer.app" \
    --icon-size 150 \
    --icon "PAGViewer.app" 200 180 \
    --app-drop-link 600 180 \
    "${dmgPath}" \
    "${sourcePath}"
}

# 1 Initialize variables
print "[ Initialize variables ]"
AppVersion=${MajorVersion}.${MinorVersion}.${BuildNumber}
CurrentTime=$(date +"%Y-%m-%d %H:%M:%S")
RFCTime=$(date -R)
SourceDir=$(dirname "$(dirname "$(realpath "$0")")")
BuildDir="${SourceDir}/build_viewer"

echo "Build Time: ${CurrentTime}"

if [ -z "${PAG_DeployQt_Path}" ] || [ -z "${PAG_Qt_Path}" ] || [ -z "${PAG_AE_SDK_Path}" ];
then
  echo "Please set [PAG_DeployQt_Path], [PAG_Qt_Path] and [PAG_AE_SDK_Path] before build on mac"
  exit 1
fi

Deployqt="${PAG_DeployQt_Path}"
QtPath="${PAG_Qt_Path}"
QtCMakePath="${QtPath}/lib/cmake"
AESDKPath="${PAG_AE_SDK_Path}"

# 2 Compile
print "[ Compile ]"
PAGPath=""
if declare -F GetPAGPath > /dev/null;
then
    PAGPath=$(GetPAGPath)
    echo "Get PAGPath: ${PAGPath}"
fi

PAGOptions=""
if declare -F GetPAGOptions > /dev/null;
then
    PAGOptions=$(GetPAGOptions)
    echo "Get PAGOptions: ${PAGOptions}"
fi

x86_64BuildDir="${BuildDir}/build_x86_64"
arm64BuildDir="${BuildDir}/build_arm64"

# 2.1 Compile PAGExporter-x86_64
print "[ Compile PAGExporter-x86_64 ]"
PluginSourceDir="$(dirname "${SourceDir}")/exporter"
x86_64BuildDirForPlugin="${x86_64BuildDir}/Plugin"

cmake -S ${PluginSourceDir} -B ${x86_64BuildDirForPlugin} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="${QtCMakePath}" -DAE_SDK_PATH="${AESDKPath}"
if [ $? -ne 0 ];
then
    echo "Build PAGExporter-x86_64 failed"
    exit 1
fi

cmake --build ${x86_64BuildDirForPlugin} --target PAGExporter -j 8
if [ $? -ne 0 ];
then
    echo "Compile PAGExporter-x86_64 failed"
    exit 1
fi

# 2.2 Compile PAGExporter-arm64
print "[ Compile PAGExporter-arm64 ]"
arm64BuildDirForPlugin="${arm64BuildDir}/Plugin"

cmake -S ${PluginSourceDir} -B ${arm64BuildDirForPlugin} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_PREFIX_PATH="${QtCMakePath}" -DAE_SDK_PATH="${AESDKPath}"
if [ $? -ne 0 ];
then
    echo "Build PAGExporter-arm64 failed"
    exit 1
fi

cmake --build ${arm64BuildDirForPlugin} --target PAGExporter -j 8
if [ $? -ne 0 ];
then
    echo "Compile PAGExporter-arm64 failed"
    exit 1
fi

# 2.3 Compile PAGViewer-x86_64
print "[ Compile x86_64 ]"
x86_64BuildDir="${BuildDir}/build_x86_64"

cmake -S ${SourceDir} -B ${x86_64BuildDir} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="${QtCMakePath}" -DPAG_PATH="${PAGPath}" -DPAG_OPTIONS="${PAGOptions}"
if [ $? -ne 0 ];
then
    echo "Build PAGViewer-x86_64 failed"
    exit 1
fi

cmake --build ${x86_64BuildDir} --target PAGViewer -j 8
if [ $? -ne 0 ];
then
    echo "Compile PAGViewer-x86_64 failed"
    exit 1
fi

# 2.4 Compile PAGViewer-arm
print "[ Compile arm64 ]"
arm64BuildDir="${BuildDir}/build_arm64"

cmake -S ${SourceDir} -B ${arm64BuildDir} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_PREFIX_PATH="${QtCMakePath}" -DPAG_PATH="${PAGPath}" -DPAG_OPTIONS="${PAGOptions}"
if [ $? -ne 0 ];
then
    echo "Build PAGViewer-arm64 failed"
    exit 1
fi

cmake --build ${arm64BuildDir} --target PAGViewer -j 8
if [ $? -ne 0 ];
then
    echo "Compile PAGViewer-arm64 failed"
    exit 1
fi

# 2.5 Compile H264EncoderTools
print "[ Compile H264EncoderTools ]"
EncoderToolSourceDir="${SourceDir}/third_party/H264EncoderTools"
EncoderToolBuildDir="${BuildDir}/EncoderTools"

xcodebuild clean -project "${EncoderToolSourceDir}/H264EncoderTools.xcodeproj" -scheme H264EncoderTools -configuration Release -quiet > /dev/null 2>&1
xcodebuild -project "${EncoderToolSourceDir}/H264EncoderTools.xcodeproj" -scheme H264EncoderTools -configuration Release SYMROOT="${EncoderToolBuildDir}" CODE_SIGN_IDENTITY="" ARCHS="arm64 x86_64" ONLY_ACTIVE_ARCH=NO CODE_SIGNING_ALLOWED=NO -quiet > /dev/null 2>&1
if [ $? -ne 0 ];
then
    echo "Build H264EncoderTools failed"
    exit 1
fi

# 3 Organize resources
# 3.1 Merge PAGViewer
print "[ Merge PAGViewer ]"
AppDir="${BuildDir}/PAGViewer.app"
ExeDir="${AppDir}/Contents/MacOS"
ExePath="${ExeDir}/PAGViewer"
x86_64ExePath="${x86_64BuildDir}/PAGViewer"
arm64ExePath="${arm64BuildDir}/PAGViewer"

install_name_tool -delete_rpath "${SourceDir}/vendor/ffmovie/mac/x64" ${x86_64ExePath}
install_name_tool -delete_rpath "${SourceDir}/vendor/ffmovie/mac/arm64" ${arm64ExePath}

mkdir -p ${ExeDir}
lipo -create ${x86_64ExePath} ${arm64ExePath} -output ${ExePath}

# 3.1.1 Generate dSYM files for PAGViewer
print "[ Generate dSYM files for PAGViewer ]"

# Generate dSYM for each architecture in their respective build directories
x86_64DsymPath="${x86_64BuildDir}/PAGViewer.dSYM"
arm64DsymPath="${arm64BuildDir}/PAGViewer.dSYM"
UniversalDsymPath="${BuildDir}/PAGViewer.dSYM"

dsymutil ${x86_64ExePath} -o ${x86_64DsymPath}
if [ $? -ne 0 ];
then
    echo "Generate PAGViewer x86_64 dSYM failed"
    exit 1
fi

dsymutil ${arm64ExePath} -o ${arm64DsymPath}
if [ $? -ne 0 ];
then
    echo "Generate PAGViewer arm64 dSYM failed"
    exit 1
fi

# Create universal dSYM by copying structure and merging DWARF
cp -R ${x86_64DsymPath} ${UniversalDsymPath}
lipo -create \
    ${x86_64DsymPath}/Contents/Resources/DWARF/PAGViewer \
    ${arm64DsymPath}/Contents/Resources/DWARF/PAGViewer \
    -output ${UniversalDsymPath}/Contents/Resources/DWARF/PAGViewer

if [ $? -ne 0 ];
then
    echo "Merge PAGViewer dSYM failed"
    exit 1
fi

echo "PAGViewer dSYM files generated:"
echo "  Universal: ${UniversalDsymPath}"
echo "  x86_64: ${x86_64DsymPath}"
echo "  arm64: ${arm64DsymPath}"

# 3.2 Obtain the dependencies of PAGViewer
print "[ Obtain the dependencies of PAGViewer ]"
${Deployqt} ${AppDir} -qmldir=${SourceDir}/assets/qml
${Deployqt} ${AppDir} -qmldir=${PluginSourceDir}/assets/qml
if [ $? -ne 0 ];
then
    echo "Obtain the dependencies of PAGViewer failed"
    exit 1
fi

# 3.2.1 Copy Sparkle
print "[ Copy Sparkle.framework ]"
FrameworkDir="${AppDir}/Contents/Frameworks"
SparklePath="${SourceDir}/vendor/sparkle/mac/Sparkle.framework"
cp -f -R -P ${SparklePath} ${FrameworkDir}

# 3.2.2 Merge and copy ffmovie
print "[ Merge and copy ffmovie.dylib ]"
x64FfmoviePath="${SourceDir}/vendor/ffmovie/mac/x64/libffmovie.dylib"
arm64FfmoviePath="${SourceDir}/vendor/ffmovie/mac/arm64/libffmovie.dylib"
FfmoviePath="${FrameworkDir}/libffmovie.dylib"
lipo -create ${x64FfmoviePath} ${arm64FfmoviePath} -output ${FfmoviePath}

# 3.3 Obtain public and private keys
print "[ Obtain public and private keys ]"

# 3.3.1 Obtain DSA public and private keys
print "[ Obtain DSA public and private keys ]"
SignForUpdate=false
DSAPublicKey=""
DSAPrivateKey=""
if declare -F GetDSAPublicKeyPath > /dev/null;
then
    DSAPublicKey=$(GetDSAPublicKeyPath)
    print "Get DSAPublicKey: ${DSAPublicKey}"
fi

if declare -F GetDSAPrivateKeyPath > /dev/null;
then
    DSAPrivateKey=$(GetDSAPrivateKeyPath)
    print "Get DSAPrivateKey: ${DSAPrivateKey}"
fi

# 3.3.2 Obtain EDDSA public and private keys
print "[ Obtain EDDSA public and private keys ]"
EDDSAPublicKey=""
EDDSAPrivateKey=""
if declare -F GetEDDSAPublicKeyPath > /dev/null;
then
    EDDSAPublicKey=$(GetEDDSAPublicKeyPath)
    print "Get EDDSAPublicKey: ${EDDSAPublicKey}"
fi

if declare -F GetEDDSAPrivateKeyPath > /dev/null;
then
    EDDSAPrivateKey=$(GetEDDSAPrivateKeyPath)
    print "Get EDDSAPrivateKey: ${EDDSAPrivateKey}"
fi

if [ -n "${DSAPublicKey}" ] && [ -n "${DSAPrivateKey}" ] && [ -n "${EDDSAPublicKey}" ] && [ -n "${EDDSAPrivateKey}" ];
then
    SignForUpdate=true
fi

# 3.4 Copy resources
print "[ Copy resources ]"
ContentsDir="${AppDir}/Contents"
PlistPath=${SourceDir}/package/templates/Info.plist
cp -f ${PlistPath} ${ContentsDir}

ResourcesDir="${AppDir}/Contents/Resources"
cp -f ${SourceDir}/assets/images/appIcon.icns ${ResourcesDir}
cp -f ${SourceDir}/assets/images/pagIcon.icns ${ResourcesDir}
if [ -n "${DSAPublicKey}" ] && [ -f "${DSAPublicKey}" ];
then
    cp -f ${DSAPublicKey} ${ResourcesDir}
fi

# 3.5 Merge PAGExporter and copy related tools
print "[ Merge PAGExporter and copy related tools ]"

# 3.5.1 Merge PAGExporter
print "[ Merge PAGExporter ]"
PluginPath="${ResourcesDir}/PAGExporter.plugin"
PluginExePath="${PluginPath}/Contents/MacOS/PAGExporter"
x86_64PluginPath="${x86_64BuildDirForPlugin}/PAGExporter.plugin"
arm64PluginPath="${arm64BuildDirForPlugin}/PAGExporter.plugin"
x86_64PluginExePath="${x86_64PluginPath}/Contents/MacOS/PAGExporter"
arm64PluginExePath="${arm64PluginPath}/Contents/MacOS/PAGExporter"

install_name_tool -delete_rpath "${PluginSourceDir}/vendor/ffaudio/mac/x64" ${x86_64PluginExePath}
install_name_tool -delete_rpath "${PluginSourceDir}/vendor/ffaudio/mac/arm64" ${arm64PluginExePath}

cp -fr ${x86_64PluginPath} ${PluginPath}
lipo -create ${x86_64PluginExePath} ${arm64PluginExePath} -output ${PluginExePath}

# 3.5.1.1 Generate dSYM files for PAGExporter
print "[ Generate dSYM files for PAGExporter ]"

# Generate dSYM for each architecture in their respective build directories
x86_64PluginDsymPath="${x86_64BuildDirForPlugin}/PAGExporter.dSYM"
arm64PluginDsymPath="${arm64BuildDirForPlugin}/PAGExporter.dSYM"
UniversalPluginDsymPath="${BuildDir}/PAGExporter.dSYM"

dsymutil ${x86_64PluginExePath} -o ${x86_64PluginDsymPath}
if [ $? -ne 0 ];
then
    echo "Generate PAGExporter x86_64 dSYM failed"
    exit 1
fi

dsymutil ${arm64PluginExePath} -o ${arm64PluginDsymPath}
if [ $? -ne 0 ];
then
    echo "Generate PAGExporter arm64 dSYM failed"
    exit 1
fi

# Create universal dSYM by copying structure and merging DWARF
cp -R ${x86_64PluginDsymPath} ${UniversalPluginDsymPath}
lipo -create \
    ${x86_64PluginDsymPath}/Contents/Resources/DWARF/PAGExporter \
    ${arm64PluginDsymPath}/Contents/Resources/DWARF/PAGExporter \
    -output ${UniversalPluginDsymPath}/Contents/Resources/DWARF/PAGExporter

if [ $? -ne 0 ];
then
    echo "Merge PAGExporter dSYM failed"
    exit 1
fi

echo "PAGExporter dSYM files generated:"
echo "  Universal: ${UniversalPluginDsymPath}"
echo "  x86_64: ${x86_64PluginDsymPath}"
echo "  arm64: ${arm64PluginDsymPath}"

# 3.5.2 Obtain the dependencies of PAGExporter
print "[ Obtain the dependencies of PAGExporter ]"
${Deployqt} ${PluginPath} -qmldir=${PluginSourceDir}/assets/qml
if [ $? -ne 0 ];
then
    echo "Obtain the dependencies of PAGExporter failed"
    exit 1
fi
cp -fRP "${PluginPath}/Contents/Frameworks/*" "${AppDir}/Contents/Frameworks/"
cp -fRP "${PluginPath}/Contents/Plugins/*" "${AppDir}/Contents/Plugins/"
rm -rf "${PluginPath}/Contents/Frameworks"
rm -rf "${PluginPath}/Contents/Plugins"
rm -rf "${PluginPath}/Contents/Resources/qml"

# 3.5.2 Merge and copy ffaudio
print "[ Merge and copy ffaudio ]"
x64FfaudioPath="${PluginSourceDir}/vendor/ffaudio/mac/x64/libffaudio.dylib"
arm64FfaudioPath="${PluginSourceDir}/vendor/ffaudio/mac/arm64/libffaudio.dylib"
PluginFrameworksDir="${PluginPath}/Contents/Frameworks"
FfaudioPath="${PluginFrameworksDir}/libffaudio.dylib"
mkdir -p ${PluginFrameworksDir}
lipo -create ${x64FfaudioPath} ${arm64FfaudioPath} -output ${FfaudioPath}

# 3.5.3 Copy related tools
print "[ Copy related tools ]"
EncoderToolsPath="${EncoderToolBuildDir}/Release/H264EncoderTools"
cp -f ${EncoderToolsPath} ${ResourcesDir}

# 3.5.4 Copy Qt deployment scripts
print "[ Copy Qt deployment scripts ]"
cp -f "${SourceDir}/qttools/copy_qt_resource.sh" "${ResourcesDir}/"
cp -f "${SourceDir}/qttools/delete_qt_resource.sh" "${ResourcesDir}/"
cp -f "${SourceDir}/qttools/replace_qt_path.sh" "${ResourcesDir}/"

# 3.5.5 Generate qt.conf for PAGExporter plugin
# Use absolute path to load Qt resources from external shared directory
# This avoids modifying plugin bundle contents which would break code signature
# Note: qt.conf in Resources/ has higher priority than MacOS/ on macOS
print "[ Generate qt.conf for PAGExporter ]"
PLUGIN_RESOURCES_DIR="${ResourcesDir}/PAGExporter.plugin/Contents/Resources"
PLUGIN_QT_CONF="${PLUGIN_RESOURCES_DIR}/qt.conf"
mkdir -p "${PLUGIN_RESOURCES_DIR}"
cat > "${PLUGIN_QT_CONF}" << 'EOF'
[Paths]
Prefix = /Library/Application Support/PAGExporter
Plugins = PlugIns
Imports = Resources/qml
QmlImports = Resources/qml
EOF

# 3.6 Update plist
print "[ Update plist ]"
DSAPublicKeyName=$(basename "${DSAPublicKey}")
SUPublicEDKey=""
if [ -n "${EDDSAPublicKey}" ] && [ -f "${EDDSAPublicKey}" ];
then
    SUPublicEDKey=$(awk '/-----BEGIN PUBLIC KEY-----/{flag=1; next} /-----END PUBLIC KEY-----/{flag=0} flag' "${EDDSAPublicKey}")
fi
/usr/libexec/Plistbuddy -c "Set CFBundleVersion ${AppVersion}" "${ContentsDir}/Info.plist"
/usr/libexec/Plistbuddy -c "Set CFBundleShortVersionString ${AppVersion}" "${ContentsDir}/Info.plist"
/usr/libexec/Plistbuddy -c "Set SUPublicDSAKeyFile ${DSAPublicKeyName}" "${ContentsDir}/Info.plist"
/usr/libexec/Plistbuddy -c "Set SUPublicEDKey ${SUPublicEDKey}" "${ContentsDir}/Info.plist"

/usr/libexec/Plistbuddy -c "Set CFBundleVersion ${AppVersion}" "${PluginPath}/Contents/Info.plist"
/usr/libexec/Plistbuddy -c "Set CFBundleShortVersionString ${AppVersion}" "${PluginPath}/Contents/Info.plist"
/usr/libexec/Plistbuddy -c "Set CFBundleIdentifier im.pag.exporter" "${PluginPath}/Contents/Info.plist"

# 3.7 Set rpath
print "[ Set rpath ]"
# todo delete redundant paths
install_name_tool -delete_rpath "${QtPath}/lib" ${ExePath}
install_name_tool -delete_rpath "${SourceDir}/vendor/sparkle/mac" ${ExePath}
install_name_tool -add_rpath "@executable_path/../Frameworks" ${ExePath}

install_name_tool -delete_rpath "${QtPath}/lib" ${PluginExePath}
install_name_tool -add_rpath "@executable_path/../Frameworks" ${PluginExePath}
install_name_tool -add_rpath "@loader_path/../Frameworks" ${PluginExePath}
# Add rpath to load Qt frameworks from external shared directory
install_name_tool -add_rpath "/Library/Application Support/PAGExporter/Frameworks" ${PluginExePath}

# 4 Sign
print "[ Sign ]"
SignCertName=""
if declare -F GetSignCertName > /dev/null;
then
    SignCertName=$(GetSignCertName)
    echo "Get SignCertName: ${SignCertName}"
fi

if security find-certificate -c "${SignCertName}" >/dev/null 2>&1;
then
    # 4.1 Sign PAGViewer.app
    print "[ Sign PAGViewer.app ]"
    EntitlementsPath="${SourceDir}/package/templates/PAGViewer.entitlements"
    NeedSignFiles=(
        "${ExePath}"
        "${ResourcesDir}/H264EncoderTools"
        "${ResourcesDir}/PAGExporter.plugin/Contents/MacOS/PAGExporter"
        "${ResourcesDir}/PAGExporter.plugin"
        "${FrameworkDir}/Sparkle.framework/Versions/Current/Autoupdate"
        "${FrameworkDir}/Sparkle.framework/Versions/Current/Sparkle"
        "${FrameworkDir}/Sparkle.framework/Versions/Current/XPCServices/Downloader.xpc"
        "${FrameworkDir}/Sparkle.framework/Versions/Current/XPCServices/Installer.xpc"
        "${FrameworkDir}/Sparkle.framework/Versions/Current/Updater.app/Contents/MacOS/Updater"
        "${AppDir}"
    )

    xattr -rc ${AppDir}
    xattr -rc ${PluginPath}

    for NeedSignFile in ${NeedSignFiles[@]};
    do
        codesign --deep --force --entitlements ${EntitlementsPath} --timestamp --options "runtime" --sign "${SignCertName}" "${NeedSignFile}"
    done

    # 4.2 Verify signature
    print "[ Verify signature ]"
    codesign -vvv --deep "${AppDir}"

    # 4.3 Notarize PAGViewer.app
    (
        print "[ Notarize PAGViewer ]"

        # 4.3.1 Compress PAGViewer.app
        print "[ Compress PAGViewer ]"
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

        # 4.3.2 Submit PAGViewer.app
        print "[ Submit PAGViewer ]"
        xcrun notarytool submit --keychain-profile "${KeychainProfile}" --wait "${TempZip}" 2>&1 | tee notarize.log
        cat notarize.log
        if [ $? -ne 0 ];
        then
            echo "Failed to submit app for notarization."
            exit 1
        fi

        UUID=$(cat notarize.log | grep -Eo '\w{8}-(\w{4}-){3}\w{12}' | head -n 1)
        echo "Submit app successfully. UUID: ${UUID}"

        # 4.3.3 Verify notarization
        print "[ Verify notarization ]"
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

        # 4.3.4 Attach notarize ticket
        print "[ Add notarize ticket ]"
        xcrun stapler staple "${AppDir}"

        # 4.3.5 Ensure ticket is attached
        print "[ Ensure ticket is attached ]"
        xcrun stapler validate "${AppDir}"
    )
fi


# 5 Package PAGViewer.dmg
print "[ Package PAGViewer.dmg ]"

# 5.1 Update Appcast.xml
print "[ Update Appcast.xml ]"
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
        chmod +x "${SignScript}"

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

# 5.2 Generate dmg
print "[ Generate dmg ]"
if [ -d "${BuildDir}/dmg_content" ];
then
    rm -rf "${BuildDir}/dmg_content"
fi
mkdir -p "${BuildDir}/dmg_content"
cp -R -P "${AppDir}" "${BuildDir}/dmg_content"

CreateDmgTool="${SourceDir}/tools/create-dmg/create-dmg"

createDmg "${CreateDmgTool}" "${BuildDir}/dmg_content" "${BuildDir}/PAGViewer.dmg" "${SourceDir}/assets/images/dmgIcon.icns" "${SourceDir}/assets/images/dmg-background.png"
if [  $? -eq 0 ];
then
    echo "create dmg success"
else
    echo "create dmg failed"
    exit 1
fi

