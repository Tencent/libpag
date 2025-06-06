#!/bin/bash

plugiPath=$1
script_path=$(realpath "$0")
script_dir=$(dirname "$script_path")
rootPath=$(realpath "$script_dir/../..")
if ! [ -w '/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/PAGExporter.plugin' ]; then
	${rootPath}/scripts/mac/takeControl
fi
rm -r -f /Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/PAGExporter.plugin
cp -r -f ${plugiPath} /Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/
