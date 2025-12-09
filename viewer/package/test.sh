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

FolderPath="$(realpath "$0")"
FolderPath="$(dirname "${FolderPath}")"
BuildViewerScriptPath="${FolderPath}/build_mac.sh"

print "[ Build Enterprise Version ]"

export PAG_DeployQt_Path="/Users/markffan/Qt6/6.8.1/macos/bin/macdeployqt"
export PAG_Qt_Path="/Users/markffan/Qt6/6.8.1/macos"
export PAG_AE_SDK_Path="/Users/markffan/Downloads/AfterEffectsSDK/Examples"


depsync
"${BuildViewerScriptPath}"
