#!/bin/bash

plugiPath=$1
# 获取脚本的绝对路径
script_path=$(realpath "$0")
# 获取脚本所在的目录
script_dir=$(dirname "$script_path")
# 获取上上级目录的绝对路径
rootPath=$(realpath "$script_dir/../..")
echo "pluginPath: ${plugiPath}"
echo "rootPath: ${rootPath}"

echo "~~~~~~~~~~~~~~~~~~~debug PAGExporter.plugin in AE~~~~~~~~~~~~~~~~~~~"
if ! [ -w '/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/PAGExporter.plugin' ]; then
  echo "~~~~~~~~~~~~~~~~~~~take control~~~~~~~~~~~~~~~~~~~"
	${rootPath}/scripts/mac/takeControl
fi
rm -r -f /Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/PAGExporter.plugin
cp -r -f ${plugiPath} /Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/
