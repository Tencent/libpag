#!/bin/bash

AE_PLUGIN_PATH="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
plugiPath=$1
script_path=$(realpath "$0")
script_dir=$(dirname "$script_path")
rootPath=$(realpath "$script_dir/../..")
if ! [ -w '${AE_PLUGIN_PATH}/PAGExporter.plugin' ]; then
	${rootPath}/scripts/mac/takeControl
fi
rm -r -f "${AE_PLUGIN_PATH}/PAGExporter.plugin"
cp -r -f ${plugiPath} "${AE_PLUGIN_PATH}/"
