AE_PLUGIN_PATH='/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore'
AE_EXPORTER_PATH="$AE_PLUGIN_PATH/PAGExporter.plugin"
QT_FRAMEWORK_DRI="$AE_EXPORTER_PATH/Contents/Frameworks"
QT_PLUGINS_DRI="$AE_EXPORTER_PATH/Contents/PlugIns"
QT_RESOURCE_DRI="$AE_EXPORTER_PATH/Contents/Resources"

function doDeleteFileInApps() {
    aeAppPath="$1"
    if [ -d "$aeAppPath" ]; then
      deletePath="$aeAppPath/Contents/$parentDir/$deleteFile"
      if [ -d "$deletePath" ]; then
        echo "delete dir:$deletePath"
        rm -fr "$deletePath"
      elif [ -f "$deletePath" ]; then
        echo "delete file:$deletePath"
        rm -f "$deletePath"
      else
        echo "$deletePath not exist"
      fi
    fi
}

function deleteFileInAEApps() {
  deleteFile="$1"
  parentDir="$2"
  for((i=2017;i<=2030;i++));
  do
    aeAppPath="/Applications/Adobe After Effects $i/Adobe After Effects $i.app"
    doDeleteFileInApps "$aeAppPath"

    aeAppPath2="/Applications/Adobe After Effects CC $i/Adobe After Effects CC $i.app"
    doDeleteFileInApps "$aeAppPath2"
  done
}

function deleteFileInMEApps() {
  deleteFile="$1"
  parentDir="$2"
  for((i=2017;i<=2030;i++));
  do
    aeAppPath="/Applications/Adobe Media Encoder $i/Adobe Media Encoder $i.app"
    doDeleteFileInApps "$aeAppPath"

    aeAppPath2="/Applications/Adobe Media Encoder CC $i/Adobe Media Encoder CC $i.app"
    doDeleteFileInApps "$aeAppPath2"
  done
}

# 遍历文件夹和删除对应 AE APP 里面文件
function traversalAndDeleteFiles() {
    cd "$1"
    for file in *
    do
      deleteFileInAEApps $file "$2"
#       deleteFileInMEApps $file "$2"
    done
}

function deleteOldQtResources() {
  if [ ! -d "$AE_EXPORTER_PATH" ]; then
      echo "$AE_EXPORTER_PATH not exist"
      return
  fi
  echo "deleteOldFrameWorks"
  traversalAndDeleteFiles "$QT_FRAMEWORK_DRI" "Frameworks"
  echo "deleteOldPlugins"
  traversalAndDeleteFiles "$QT_PLUGINS_DRI" "PlugIns"
  echo "deleteOldResources"
  traversalAndDeleteFiles "$QT_RESOURCE_DRI" "Resources"
#  echo "deletePAGExporter"
#  rm -fr "$AE_EXPORTER_PATH"
}

deleteOldQtResources