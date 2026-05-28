#!/bin/bash

# Color definitions
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

SECTION_WIDTH=60
TOTAL_SECTIONS=5

function printSection() {
    local index="$1"
    local title="$2"
    local header="  ▶ [${index}/${TOTAL_SECTIONS}] ${title}"
    local line=$(printf '═%.0s' $(seq 1 ${SECTION_WIDTH}))
    echo ""
    echo -e "${BOLD}${BLUE}${line}${NC}"
    echo -e "${BOLD}${BLUE}${header}${NC}"
    echo -e "${BOLD}${BLUE}${line}${NC}"
}

function printStep() {
    echo -e "${BLUE}  → $1${NC}"
}

function logInfo() {
    echo -e "${BLUE}    [INFO] $1${NC}"
}

function logSuccess() {
    echo -e "${GREEN}    [OK] $1${NC}"
}

function logWarning() {
    echo -e "${YELLOW}    [WARN] $1${NC}"
}

function logError() {
    echo -e "${RED}    [ERROR] $1${NC}"
}

function exitWithError() {
    logError "$1"
    exit 1
}

function cmakeConfigure() {
    local sourceDir="$1"
    local buildDir="$2"
    local target="$3"
    shift 3
    cmake -S "${sourceDir}" -B "${buildDir}" "$@"
    if [ $? -ne 0 ];
    then
        exitWithError "CMake configure failed for ${target}"
    fi
}

function cmakeBuild() {
    local buildDir="$1"
    local target="$2"
    cmake --build "${buildDir}" --target "${target}" -j 8
    if [ $? -ne 0 ];
    then
        exitWithError "CMake build failed for ${target}"
    fi
}

function generateUniversalDsym() {
    local binaryName="$1"
    local x86Dir="$2"
    local armDir="$3"
    local outputDir="$4"
    local x86Exe="$5"
    local armExe="$6"

    local x86Dsym="${x86Dir}/${binaryName}.dSYM"
    local armDsym="${armDir}/${binaryName}.dSYM"
    local universalDsym="${outputDir}/${binaryName}.dSYM"

    dsymutil "${x86Exe}" -o "${x86Dsym}"
    if [ $? -ne 0 ];
    then
        exitWithError "Generate ${binaryName} x86_64 dSYM failed"
    fi

    dsymutil "${armExe}" -o "${armDsym}"
    if [ $? -ne 0 ];
    then
        exitWithError "Generate ${binaryName} arm64 dSYM failed"
    fi

    cp -R "${x86Dsym}" "${universalDsym}"
    lipo -create \
        "${x86Dsym}/Contents/Resources/DWARF/${binaryName}" \
        "${armDsym}/Contents/Resources/DWARF/${binaryName}" \
        -output "${universalDsym}/Contents/Resources/DWARF/${binaryName}"
    if [ $? -ne 0 ];
    then
        exitWithError "Merge ${binaryName} dSYM failed"
    fi

    logSuccess "${binaryName} dSYM files generated:"
    logInfo "  Universal: ${universalDsym}"
    logInfo "  x86_64: ${x86Dsym}"
    logInfo "  arm64: ${armDsym}"
}

function cleanAbsoluteRpaths() {
    local binary="$1"
    otool -l "${binary}" | grep -A2 "LC_RPATH" | grep "path " | sed 's/.*path \(.*\) (offset.*/\1/' | while IFS= read -r rpath; do
        case "${rpath}" in
            @*|/Library/*) ;;
            *) install_name_tool -delete_rpath "${rpath}" "${binary}" 2>/dev/null || true ;;
        esac
    done
}

function createDmg()
{
    local creatDmg="${1}"
    local sourcePath="${2}"
    local dmgPath="${3}"
    local iconPath="${4}"
    local backgroundPath="${5}"

    "${creatDmg}" \
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
BUILD_START_TIME=$(date +%s)
printSection 1 "Initialize variables"
# Default to 1.0.0 when version env vars are not provided (e.g. local dev builds).
# CI populates MajorVersion / MinorVersion / BuildNumber from the release pipeline.
MajorVersion=${MajorVersion:-1}
MinorVersion=${MinorVersion:-0}
BuildNumber=${BuildNumber:-0}
AppVersion=${MajorVersion}.${MinorVersion}.${BuildNumber}
# Normalize isBetaVersion to a strict lowercase "true" so later string compares are predictable.
case "${isBetaVersion:-}" in
    [Tt][Rr][Uu][Ee]|1) IsBetaVersion="true" ;;
    *)                  IsBetaVersion="false" ;;
esac
CurrentTime=$(date +"%Y-%m-%d %H:%M:%S")
RFCTime=$(date -R)
SourceDir=$(dirname "$(dirname "$(realpath "$0")")")
BuildDir="${SourceDir}/build_viewer"

logInfo "Build Time: ${CurrentTime}"

if [ -z "${PAG_DeployQt_Path}" ] || [ -z "${PAG_Qt_Path}" ] || [ -z "${PAG_AE_SDK_Path}" ];
then
    exitWithError "Please set [PAG_DeployQt_Path], [PAG_Qt_Path] and [PAG_AE_SDK_Path] before build on mac"
fi

Deployqt="${PAG_DeployQt_Path}"
QtPath="${PAG_Qt_Path}"
QtCMakePath="${QtPath}/lib/cmake"
AESDKPath="${PAG_AE_SDK_Path}"

# 2 Compile
printSection 2 "Compile"
PAGPath=""
if declare -F GetPAGPath > /dev/null;
then
    PAGPath=$(GetPAGPath)
    logInfo "Get PAGPath: ${PAGPath}"
fi

PAGOptions=""
if declare -F GetPAGOptions > /dev/null;
then
    PAGOptions=$(GetPAGOptions)
    logInfo "Get PAGOptions: ${PAGOptions}"
fi

x86_64BuildDir="${BuildDir}/build_x86_64"
arm64BuildDir="${BuildDir}/build_arm64"

# 2.1 Compile PAGExporter-x86_64
printStep "PAGExporter-x86_64"
PluginSourceDir="$(dirname "${SourceDir}")/exporter"
x86_64BuildDirForPlugin="${x86_64BuildDir}/Plugin"

cmakeConfigure "${PluginSourceDir}" "${x86_64BuildDirForPlugin}" "PAGExporter-x86_64" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="${QtCMakePath}" -DAE_SDK_PATH="${AESDKPath}"
cmakeBuild "${x86_64BuildDirForPlugin}" "PAGExporter"

# 2.2 Compile PAGExporter-arm64
printStep "PAGExporter-arm64"
arm64BuildDirForPlugin="${arm64BuildDir}/Plugin"

cmakeConfigure "${PluginSourceDir}" "${arm64BuildDirForPlugin}" "PAGExporter-arm64" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="${QtCMakePath}" -DAE_SDK_PATH="${AESDKPath}"
cmakeBuild "${arm64BuildDirForPlugin}" "PAGExporter"

# 2.3 Compile PAGViewer-x86_64
printStep "PAGViewer-x86_64"

cmakeConfigure "${SourceDir}" "${x86_64BuildDir}" "PAGViewer-x86_64" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="${QtCMakePath}" -DPAG_PATH="${PAGPath}" -DPAG_OPTIONS="${PAGOptions}"
cmakeBuild "${x86_64BuildDir}" "PAGViewer"

# 2.4 Compile PAGViewer-arm64
printStep "PAGViewer-arm64"

cmakeConfigure "${SourceDir}" "${arm64BuildDir}" "PAGViewer-arm64" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="${QtCMakePath}" -DPAG_PATH="${PAGPath}" -DPAG_OPTIONS="${PAGOptions}"
cmakeBuild "${arm64BuildDir}" "PAGViewer"

# 2.5 Compile H264EncoderTools
printStep "H264EncoderTools"
EncoderToolSourceDir="${SourceDir}/third_party/H264EncoderTools"
EncoderToolBuildDir="${BuildDir}/EncoderTools"

xcodebuild clean -project "${EncoderToolSourceDir}/H264EncoderTools.xcodeproj" -scheme H264EncoderTools -configuration Release -quiet > /dev/null 2>&1
if [ $? -ne 0 ];
then
    exitWithError "Clean H264EncoderTools failed"
fi

BUILD_OUTPUT=$(xcodebuild -project "${EncoderToolSourceDir}/H264EncoderTools.xcodeproj" -scheme H264EncoderTools -configuration Release SYMROOT="${EncoderToolBuildDir}" CODE_SIGN_IDENTITY="" ARCHS="arm64 x86_64" ONLY_ACTIVE_ARCH=NO CODE_SIGNING_ALLOWED=NO -quiet 2>&1)
BUILD_STATUS=$?
if [ ${BUILD_STATUS} -ne 0 ];
then
    echo "${BUILD_OUTPUT}"
    exitWithError "Build H264EncoderTools failed"
fi

# 3 Organize resources
# 3.1 Merge PAGViewer
printSection 3 "Organize resources"
printStep "Merge PAGViewer"
AppDir="${BuildDir}/PAGViewer.app"
ExeDir="${AppDir}/Contents/MacOS"
ExePath="${ExeDir}/PAGViewer"
x86_64ExePath="${x86_64BuildDir}/PAGViewer"
arm64ExePath="${arm64BuildDir}/PAGViewer"

# Clean previous app bundle to avoid incremental build issues
rm -rf "${AppDir}"

install_name_tool -delete_rpath "${SourceDir}/vendor/ffmovie/mac/x64" "${x86_64ExePath}" 2>/dev/null || true
install_name_tool -delete_rpath "${SourceDir}/vendor/ffmovie/mac/arm64" "${arm64ExePath}" 2>/dev/null || true

mkdir -p "${ExeDir}"
lipo -create "${x86_64ExePath}" "${arm64ExePath}" -output "${ExePath}"
if [ $? -ne 0 ];
then
    exitWithError "Failed to merge PAGViewer universal binary at ${ExePath}"
fi

# 3.1.1 Generate dSYM files for PAGViewer
printStep "Generate dSYM for PAGViewer"
generateUniversalDsym "PAGViewer" "${x86_64BuildDir}" "${arm64BuildDir}" "${BuildDir}" "${x86_64ExePath}" "${arm64ExePath}"

# 3.2 Obtain the dependencies of PAGViewer
printStep "Deploy Qt dependencies"
"${Deployqt}" "${AppDir}" -qmldir="${SourceDir}/assets/qml"
if [ $? -ne 0 ];
then
    exitWithError "Deploy Qt dependencies (viewer qml) failed"
fi
"${Deployqt}" "${AppDir}" -qmldir="${PluginSourceDir}/assets/qml"
if [ $? -ne 0 ];
then
    exitWithError "Deploy Qt dependencies (exporter qml) failed"
fi

# 3.2.1 Copy Sparkle
printStep "Copy Sparkle.framework"
FrameworkDir="${AppDir}/Contents/Frameworks"
SparklePath="${SourceDir}/vendor/sparkle/mac/Sparkle.framework"
cp -f -R -P "${SparklePath}" "${FrameworkDir}"

# 3.2.2 Merge and copy ffmovie
printStep "Merge ffmovie.dylib"
x64FfmoviePath="${SourceDir}/vendor/ffmovie/mac/x64/libffmovie.dylib"
arm64FfmoviePath="${SourceDir}/vendor/ffmovie/mac/arm64/libffmovie.dylib"
FfmoviePath="${FrameworkDir}/libffmovie.dylib"
lipo -create "${x64FfmoviePath}" "${arm64FfmoviePath}" -output "${FfmoviePath}"
if [ $? -ne 0 ];
then
    exitWithError "Failed to merge ffmovie universal dylib at ${FfmoviePath}"
fi

# 3.3 Obtain public and private keys
printStep "Obtain signing keys"

# 3.3.1 Obtain DSA public and private keys
SignForUpdate=false
DSAPublicKey=""
DSAPrivateKey=""
if declare -F GetDSAPublicKeyPath > /dev/null;
then
    DSAPublicKey=$(GetDSAPublicKeyPath)
    logInfo "Get DSAPublicKey: ${DSAPublicKey}"
fi

if declare -F GetDSAPrivateKeyPath > /dev/null;
then
    DSAPrivateKey=$(GetDSAPrivateKeyPath)
    logInfo "Get DSAPrivateKey: ${DSAPrivateKey}"
fi

# 3.3.2 Obtain EDDSA public and private keys
EDDSAPublicKey=""
EDDSAPrivateKey=""
if declare -F GetEDDSAPublicKeyPath > /dev/null;
then
    EDDSAPublicKey=$(GetEDDSAPublicKeyPath)
    logInfo "Get EDDSAPublicKey: ${EDDSAPublicKey}"
fi

if declare -F GetEDDSAPrivateKeyPath > /dev/null;
then
    EDDSAPrivateKey=$(GetEDDSAPrivateKeyPath)
    logInfo "Get EDDSAPrivateKey: ${EDDSAPrivateKey}"
fi

if [ -n "${DSAPublicKey}" ] && [ -n "${DSAPrivateKey}" ] && [ -n "${EDDSAPublicKey}" ] && [ -n "${EDDSAPrivateKey}" ];
then
    SignForUpdate=true
fi

# 3.4 Copy resources
printStep "Copy resources"
ContentsDir="${AppDir}/Contents"
PlistPath="${SourceDir}/package/templates/Info.plist"
cp -f "${PlistPath}" "${ContentsDir}"

ResourcesDir="${AppDir}/Contents/Resources"
cp -f "${SourceDir}/assets/images/appIcon.icns" "${ResourcesDir}"
cp -f "${SourceDir}/assets/images/pagIcon.icns" "${ResourcesDir}"
if [ -n "${DSAPublicKey}" ] && [ -f "${DSAPublicKey}" ];
then
    cp -f "${DSAPublicKey}" "${ResourcesDir}"
fi

# 3.5 Merge PAGExporter and copy related tools
printStep "Merge PAGExporter"

# 3.5.1 Merge PAGExporter
PluginPath="${ResourcesDir}/PAGExporter.plugin"
PluginExePath="${PluginPath}/Contents/MacOS/PAGExporter"
x86_64PluginPath="${x86_64BuildDirForPlugin}/PAGExporter.plugin"
arm64PluginPath="${arm64BuildDirForPlugin}/PAGExporter.plugin"
x86_64PluginExePath="${x86_64PluginPath}/Contents/MacOS/PAGExporter"
arm64PluginExePath="${arm64PluginPath}/Contents/MacOS/PAGExporter"

# Remove absolute build rpaths from each architecture before merging
cleanAbsoluteRpaths "${x86_64PluginExePath}"
cleanAbsoluteRpaths "${arm64PluginExePath}"

cp -fr "${x86_64PluginPath}" "${PluginPath}"
lipo -create "${x86_64PluginExePath}" "${arm64PluginExePath}" -output "${PluginExePath}"

# 3.5.1.1 Generate dSYM files for PAGExporter
printStep "Generate dSYM for PAGExporter"
generateUniversalDsym "PAGExporter" "${x86_64BuildDirForPlugin}" "${arm64BuildDirForPlugin}" "${BuildDir}" "${x86_64PluginExePath}" "${arm64PluginExePath}"

# 3.5.2 Obtain the dependencies of PAGExporter
printStep "Deploy PAGExporter Qt dependencies"
"${Deployqt}" "${PluginPath}" -qmldir="${PluginSourceDir}/assets/qml"
if [ $? -ne 0 ];
then
    exitWithError "Obtain the dependencies of PAGExporter failed"
fi
# Remove libraries from plugin that already exist in the main app (to avoid overwriting universal with single-arch)
if [ -d "${PluginPath}/Contents/Frameworks" ];
then
    for existingLib in "${FrameworkDir}"/*.dylib; do
        libName=$(basename "${existingLib}")
        rm -f "${PluginPath}/Contents/Frameworks/${libName}"
    done
    # Copy remaining plugin Frameworks into the main app.
    # Use process substitution so exitWithError aborts the whole script;
    # find -exec swallows cp failures and a piped while runs in a subshell.
    while IFS= read -r -d '' Item; do
        if ! cp -fRP "${Item}" "${AppDir}/Contents/Frameworks/";
        then
            exitWithError "Failed to copy plugin framework item: ${Item}"
        fi
    done < <(find "${PluginPath}/Contents/Frameworks" -mindepth 1 -maxdepth 1 -print0)
fi
if [ -d "${PluginPath}/Contents/Plugins" ];
then
    cp -fRP "${PluginPath}/Contents/Plugins/"* "${AppDir}/Contents/Plugins/"
fi
rm -rf "${PluginPath}/Contents/Frameworks"
rm -rf "${PluginPath}/Contents/Plugins"
rm -rf "${PluginPath}/Contents/Resources/qml"

# 3.5.3 Copy related tools
printStep "Copy tools and scripts"
EncoderToolsPath="${EncoderToolBuildDir}/Release/H264EncoderTools"
cp -f "${EncoderToolsPath}" "${ResourcesDir}"
cp -f "${SourceDir}/qttools/copy_qt_resource.sh" "${ResourcesDir}/"
cp -f "${SourceDir}/qttools/delete_qt_resource.sh" "${ResourcesDir}/"
cp -f "${SourceDir}/qttools/replace_qt_path.sh" "${ResourcesDir}/"

# 3.5.5 Generate qt.conf for PAGExporter plugin
printStep "Generate qt.conf for PAGExporter"
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
printStep "Update plist"
SUPublicEDKey=""
if [ -n "${EDDSAPublicKey}" ] && [ -f "${EDDSAPublicKey}" ];
then
    SUPublicEDKey=$(awk '/-----BEGIN PUBLIC KEY-----/{flag=1; next} /-----END PUBLIC KEY-----/{flag=0} flag' "${EDDSAPublicKey}")
fi
/usr/libexec/Plistbuddy -c "Set CFBundleVersion ${AppVersion}" "${ContentsDir}/Info.plist" \
    || exitWithError "Failed to update CFBundleVersion in ${ContentsDir}/Info.plist"
/usr/libexec/Plistbuddy -c "Set CFBundleShortVersionString ${AppVersion}" "${ContentsDir}/Info.plist" \
    || exitWithError "Failed to update CFBundleShortVersionString in ${ContentsDir}/Info.plist"
if [ -n "${DSAPublicKey}" ];
then
    DSAPublicKeyName=$(basename "${DSAPublicKey}")
    /usr/libexec/Plistbuddy -c "Set SUPublicDSAKeyFile ${DSAPublicKeyName}" "${ContentsDir}/Info.plist" \
        || exitWithError "Failed to update SUPublicDSAKeyFile in ${ContentsDir}/Info.plist"
fi
/usr/libexec/Plistbuddy -c "Set SUPublicEDKey ${SUPublicEDKey}" "${ContentsDir}/Info.plist" \
    || exitWithError "Failed to update SUPublicEDKey in ${ContentsDir}/Info.plist"

/usr/libexec/Plistbuddy -c "Set CFBundleVersion ${AppVersion}" "${PluginPath}/Contents/Info.plist" \
    || exitWithError "Failed to update CFBundleVersion in ${PluginPath}/Contents/Info.plist"
/usr/libexec/Plistbuddy -c "Set CFBundleShortVersionString ${AppVersion}" "${PluginPath}/Contents/Info.plist" \
    || exitWithError "Failed to update CFBundleShortVersionString in ${PluginPath}/Contents/Info.plist"
/usr/libexec/Plistbuddy -c "Set CFBundleIdentifier im.pag.exporter" "${PluginPath}/Contents/Info.plist" \
    || exitWithError "Failed to update CFBundleIdentifier in ${PluginPath}/Contents/Info.plist"

# 3.7 Set rpath
printStep "Set rpath"
install_name_tool -delete_rpath "${QtPath}/lib" "${ExePath}" 2>/dev/null || true
install_name_tool -delete_rpath "${SourceDir}/vendor/sparkle/mac" "${ExePath}" 2>/dev/null || true
install_name_tool -add_rpath "@executable_path/../Frameworks" "${ExePath}" 2>/dev/null || true

install_name_tool -delete_rpath "${QtPath}/lib" "${PluginExePath}" 2>/dev/null || true
install_name_tool -add_rpath "@executable_path/../Frameworks" "${PluginExePath}" 2>/dev/null || true
install_name_tool -add_rpath "@loader_path/../Frameworks" "${PluginExePath}" 2>/dev/null || true
install_name_tool -add_rpath "/Library/Application Support/PAGExporter/Frameworks" "${PluginExePath}" 2>/dev/null || true

# 4 Sign & Notarize
printSection 4 "Sign & Notarize"
SignCertName="Developer ID Application: Tencent Technology (Shanghai) Company Limited (FN2V63AD2J)"
if declare -F GetSignCertName > /dev/null;
then
    SignCertName=$(GetSignCertName)
    logInfo "Get SignCertName: ${SignCertName}"
fi

HasSignCert=false
if security find-certificate -c "${SignCertName}" >/dev/null 2>&1;
then
    HasSignCert=true
fi

if [ "${HasSignCert}" = true ];
then
    # 4.1 Sign PAGViewer.app
    printStep "Sign PAGViewer.app"
    EntitlementsPath="${SourceDir}/package/templates/PAGViewer.entitlements"

    xattr -rc "${AppDir}"
    xattr -rc "${PluginPath}"

    # Sign all dylibs in Frameworks/ directory first.
    # Use process substitution so exitWithError aborts the whole script;
    # a piped `while` would only exit the subshell.
    logInfo "Signing all libraries in Frameworks/..."
    while IFS= read -r dylib; do
        SIGN_OUTPUT=$(codesign --force --timestamp --options "runtime" --sign "${SignCertName}" "${dylib}" 2>&1)
        SIGN_STATUS=$?
        if [ ${SIGN_STATUS} -ne 0 ];
        then
            echo "${SIGN_OUTPUT}"
            exitWithError "Failed to sign dylib: ${dylib}"
        fi
    done < <(find "${FrameworkDir}" -name "*.dylib" -type f)

    # Sign standalone executables not inside a bundle
    logInfo "Signing: ${ResourcesDir}/H264EncoderTools"
    SIGN_OUTPUT=$(codesign --force --entitlements "${EntitlementsPath}" --timestamp --options "runtime" --sign "${SignCertName}" "${ResourcesDir}/H264EncoderTools" 2>&1)
    SIGN_STATUS=$?
    if [ ${SIGN_STATUS} -ne 0 ];
    then
        echo "${SIGN_OUTPUT}"
        exitWithError "Failed to sign: H264EncoderTools"
    fi

    # Sign bundles with --deep (from inner to outer)
    # --deep recursively signs all executables and frameworks inside the bundle
    NeedSignBundles=(
        "${FrameworkDir}/Sparkle.framework/Versions/Current/XPCServices/Downloader.xpc"
        "${FrameworkDir}/Sparkle.framework/Versions/Current/XPCServices/Installer.xpc"
        "${FrameworkDir}/Sparkle.framework/Versions/Current/Updater.app"
        "${FrameworkDir}/Sparkle.framework"
        "${ResourcesDir}/PAGExporter.plugin"
        "${AppDir}"
    )

    for NeedSignFile in "${NeedSignBundles[@]}";
    do
        logInfo "Signing bundle: ${NeedSignFile}"
        SIGN_OUTPUT=$(codesign --deep --force --entitlements "${EntitlementsPath}" --timestamp --options "runtime" --sign "${SignCertName}" "${NeedSignFile}" 2>&1)
        SIGN_STATUS=$?
        if [ ${SIGN_STATUS} -ne 0 ];
        then
            echo "${SIGN_OUTPUT}"
            exitWithError "Failed to sign: ${NeedSignFile}"
        fi
        if echo "${SIGN_OUTPUT}" | grep -qi "ambiguous\|error\|invalid";
        then
            echo "${SIGN_OUTPUT}"
            exitWithError "Signing issue detected for: ${NeedSignFile}"
        fi
        if [ -n "${SIGN_OUTPUT}" ];
        then
            logWarning "${SIGN_OUTPUT}"
        fi
    done

    # 4.2 Verify signature
    printStep "Verify signature"
    codesign -vvv --deep "${AppDir}"
    if [ $? -ne 0 ];
    then
        exitWithError "Signature verification failed"
    fi
    logSuccess "Signature verification passed"

    # 4.3 Notarize PAGViewer.app
    printStep "Notarize"

    # 4.3.1 Compress PAGViewer.app for notarization
    pushd "${BuildDir}" > /dev/null
    KeychainProfile="AC_PASSWORD"
    TempDir="Applications"
    TempZip="Applications.zip"

    rm -f "${TempZip}"
    rm -rf "${TempDir}"

    mkdir -p "${TempDir}"
    # Ensure notarization temp artifacts are removed even if a later step fails.
    # Use absolute paths so the trap works regardless of the cwd at exit time.
    NotarizeTempDir="${BuildDir}/${TempDir}"
    NotarizeTempZip="${BuildDir}/${TempZip}"
    NotarizeLog="${BuildDir}/notarize.log"
    NotarizeDetail="${BuildDir}/notarize_detail.json"
    trap 'rm -rf "${NotarizeTempDir}" "${NotarizeTempZip}" "${NotarizeLog}" "${NotarizeDetail}" 2>/dev/null || true' EXIT
    cp -fRP "${AppDir}" "${TempDir}"
    zip --symlinks -r -q -X "${TempZip}" "${TempDir}"

    # 4.3.2 Submit PAGViewer.app for notarization
    # Use --wait to block until notarization completes, no need for manual polling
    xcrun notarytool submit --keychain-profile "${KeychainProfile}" --wait "${TempZip}" 2>&1 | tee notarize.log
    SUBMIT_STATUS=${PIPESTATUS[0]}

    if [ ${SUBMIT_STATUS} -ne 0 ];
    then
        logError "Notarization submission failed. Log output:"
        cat notarize.log
        exitWithError "Failed to submit app for notarization."
    fi

    # Check the notarization result from the log
    NOTARIZE_STATUS=$(grep -i "status:" notarize.log | tail -1 | awk '{print $2}')
    logInfo "Notarization status: ${NOTARIZE_STATUS}"

    if [ "$(echo "${NOTARIZE_STATUS}" | tr '[:upper:]' '[:lower:]')" != "accepted" ];
    then
        logError "Notarization was not accepted. Status: ${NOTARIZE_STATUS}"
        # Retrieve the submission ID to fetch the log for debugging
        UUID=$(grep -Eo '[0-9a-f]{8}-([0-9a-f]{4}-){3}[0-9a-f]{12}' notarize.log | head -n 1)
        if [ -n "${UUID}" ];
        then
            logInfo "Fetching notarization log for UUID: ${UUID}"
            xcrun notarytool log "${UUID}" --keychain-profile "${KeychainProfile}" notarize_detail.json 2>/dev/null
            if [ -f notarize_detail.json ];
            then
                cat notarize_detail.json
            fi
        fi
        exitWithError "Notarization failed."
    fi

    logSuccess "Notarization accepted."

    # 4.3.3 Attach notarization ticket to the app bundle
    printStep "Staple ticket"
    xcrun stapler staple "${AppDir}"
    if [ $? -ne 0 ];
    then
        exitWithError "Failed to staple notarization ticket to ${AppDir}"
    fi
    logSuccess "Notarization ticket stapled successfully."

    # 4.3.4 Validate the stapled ticket
    printStep "Validate ticket"
    xcrun stapler validate "${AppDir}"
    if [ $? -ne 0 ];
    then
        exitWithError "Stapler validation failed for ${AppDir}"
    fi
    logSuccess "Stapler validation passed."

    popd > /dev/null
    # Notarization succeeded; run cleanup explicitly and disarm the EXIT trap
    rm -rf "${NotarizeTempDir}" "${NotarizeTempZip}" "${NotarizeLog}" "${NotarizeDetail}" 2>/dev/null || true
    trap - EXIT
else
    logWarning "Certificate '${SignCertName}' not found in keychain, skipping code signing and notarization."
fi

# 5 Package PAGViewer.dmg
printSection 5 "Package"

# 5.1 Update Appcast.xml
printStep "Update Appcast.xml"
if [ "${SignForUpdate}" == true ];
then
    (
        cd "${BuildDir}"

        ZipFile="PAGViewer.zip"
        if [ -f "${ZipFile}" ];
        then
            rm -f "${ZipFile}"
        fi

        zip --symlinks -r -q -X "${ZipFile}" "PAGViewer.app"

        SignScript="${SourceDir}/package/sign_update.sh"
        chmod +x "${SignScript}"

        "${SignScript}" "${BuildDir}/${ZipFile}" "${DSAPrivateKey}" > DSASignHash.txt
        ZipDSAHash=$(tr '\n' ' ' < DSASignHash.txt | sed '$s/ //g')
        logInfo "Get Zip DSA Hash: ${ZipDSAHash}"

        "${SignScript}" "${BuildDir}/${ZipFile}" "${EDDSAPrivateKey}" > EDDSASignHash.txt
        ZipEDDSAHash=$(tr '\n' ' ' < EDDSASignHash.txt | sed '$s/ //g')
        logInfo "Get Zip EDDSA Hash: ${ZipEDDSAHash}"

        ZipLength=$(stat -f%z "${BuildDir}/${ZipFile}")

        URL=$(curl -fsS --retry 3 --connect-timeout 10 https://pag.io/server.html)
        if [ -z "${URL}" ];
        then
            exitWithError "Failed to fetch update URL from pag.io/server.html"
        fi
        if [ "${IsBetaVersion}" == true ];
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
printStep "Generate DMG"

# Remove existing DMG to avoid create-dmg failure
rm -f "${BuildDir}/PAGViewer.dmg"

# Detach any previously mounted PAGViewer volumes to avoid conflicts
for vol in /Volumes/PAGViewer*; do
    if [ -d "$vol" ];
    then
        logWarning "Found mounted volume: $vol, detaching..."
        hdiutil detach "$vol" -force 2>/dev/null || true
    fi
done

if [ -d "${BuildDir}/dmg_content" ];
then
    rm -rf "${BuildDir}/dmg_content"
fi
mkdir -p "${BuildDir}/dmg_content"
cp -R -P "${AppDir}" "${BuildDir}/dmg_content"

CreateDmgTool="${SourceDir}/tools/create-dmg/create-dmg"

createDmg "${CreateDmgTool}" "${BuildDir}/dmg_content" "${BuildDir}/PAGViewer.dmg" "${SourceDir}/assets/images/dmgIcon.icns" "${SourceDir}/assets/images/dmg-background.png"
if [ $? -eq 0 ];
then
    logSuccess "DMG created successfully: ${BuildDir}/PAGViewer.dmg"
else
    exitWithError "Failed to create DMG"
fi

# Summary
BUILD_END_TIME=$(date +%s)
BUILD_DURATION=$((BUILD_END_TIME - BUILD_START_TIME))
BUILD_MINUTES=$((BUILD_DURATION / 60))
BUILD_SECONDS=$((BUILD_DURATION % 60))

DMG_PATH="${BuildDir}/PAGViewer.dmg"
DMG_SIZE=""
if [ -f "${DMG_PATH}" ];
then
    DMG_SIZE=$(du -h "${DMG_PATH}" | awk '{print $1}')
fi

SIGNED="No"
NOTARIZED="No"
if [ "${HasSignCert}" = true ];
then
    SIGNED="Yes (${SignCertName})"
    NOTARIZED="Yes (stapled)"
fi

LINE=$(printf '═%.0s' $(seq 1 ${SECTION_WIDTH}))
echo ""
echo -e "${BOLD}${GREEN}${LINE}${NC}"
echo -e "${BOLD}${GREEN}  ✔ Build Complete — ${BUILD_MINUTES}m ${BUILD_SECONDS}s${NC}"
echo -e "${BOLD}${GREEN}${LINE}${NC}"
logInfo "  Version:    ${AppVersion}"
logInfo "  DMG:        ${DMG_PATH} (${DMG_SIZE})"
logInfo "  dSYM:       ${BuildDir}/PAGViewer.dSYM"
logInfo "              ${BuildDir}/PAGExporter.dSYM"
logInfo "  Signed:     ${SIGNED}"
logInfo "  Notarized:  ${NOTARIZED}"
echo ""
