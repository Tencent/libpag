AE_PLUGIN_PATH='/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore'
AE_EXPORTER_PATH="$AE_PLUGIN_PATH/PAGExporter.plugin"
QT_FRAMEWORK_DIR="$AE_EXPORTER_PATH/Contents/Frameworks"
QT_PLUGINS_DIR="$AE_EXPORTER_PATH/Contents/PlugIns"
QT_RESOURCE_DIR="$AE_EXPORTER_PATH/Contents/Resources"

function doDeleteFileInApps() {
    aeAppPath="$1"
    if [ -d "$aeAppPath" ]; then
      deletePath="$aeAppPath/Contents/$parentDir/$deleteFile"
      if [ -d "$deletePath" ]; then
        rm -fr "$deletePath"
      elif [ -f "$deletePath" ]; then
        rm -f "$deletePath"
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

# Traverse directory and delete corresponding files in AE apps
function traversalAndDeleteFiles() {
    cd "$1"
    for file in *
    do

      deleteFileInAEApps "$file" "$2"
    done
}

function deleteOldQtResources() {
  if [ ! -d "$AE_EXPORTER_PATH" ]; then
      return
  fi
  traversalAndDeleteFiles "$QT_FRAMEWORK_DIR" "Frameworks"
  traversalAndDeleteFiles "$QT_PLUGINS_DIR" "PlugIns"
  traversalAndDeleteFiles "$QT_RESOURCE_DIR" "Resources"
}

deleteOldQtResources
