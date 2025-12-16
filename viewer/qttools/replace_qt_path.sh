AE_PLUGIN_PATH='/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore'
AE_EXPORTER_PATH="$AE_PLUGIN_PATH/PAGExporter.plugin"
QT_FRAMEWORK_DIR="$AE_EXPORTER_PATH/Contents/Frameworks"
TARGET_QML_DIR="$AE_EXPORTER_PATH/Contents/Resources/qml"

# Replace paths in executable/library files
function startReplacePathInExecuteFile(){
    executeName=$1
    isQML="$2"
    currentPath=`pwd`

    otool -L $executeName
    for linkPath in `otool -L $executeName | tr " " "\?"`
    do
        # Replace executable paths
        if [[ "$linkPath" =~ ^@executable_path/\.\./Frameworks ]]; then
           oldPath=`echo "$linkPath" | grep -e "^@executable_path/\.\./Frameworks/[A-Z|a-z|0-9|/|\.]*" -o`
           newPath=`echo "$oldPath" | sed -e "s/^@executable_path\/\.\.\/Frameworks//"`
           newPath="@rpath$newPath"
           install_name_tool -change "$oldPath" "$newPath" $executeName
        fi
    done

    # Replace library ID
    idName=`otool -l $executeName | grep -A 10 LC_ID_DYLIB | grep name`
    if [ -n "$idName" ]; then
      idName=`echo ${idName#*name }`
      idName=`echo ${idName% (*}`

      if [ "$isQML" == "true" ]; then
        oldIdName=`echo ${idName#*qml}`
        newIdName="@rpath$oldIdName"
      else
        oldIdName=`echo "$idName" | grep -e "^@executable_path/\.\./Frameworks/[A-Z|a-z|0-9|/|\
        .]*" -o`
        newIdName=`echo "$oldIdName" | sed -e "s/^@executable_path\/\.\.\/Frameworks//"`
        newIdName="@rpath$newIdName"
      fi

      install_name_tool -id "$newIdName" $executeName
    fi

    # Remove old rpath
    install_name_tool -delete_rpath "@executable_path/../Frameworks" $executeName 2>/dev/null || true

    # Add new rpath
    if [ "$isQML" == "true" ]; then
      install_name_tool -add_rpath "$TARGET_QML_DIR" $executeName
      install_name_tool -add_rpath "$QT_FRAMEWORK_DRI" $executeName
    else
      install_name_tool -add_rpath "$QT_FRAMEWORK_DRI" $executeName
    fi
}

# Replace paths in Qt frameworks
function replacePathInFrameworks(){
    cd "$1"
    for file in *
    do
        if [ "${file##*.}"x = "framework"x ]
        then
            executeName=${file%.*}
            cd $file
            startReplacePathInExecuteFile "$executeName"
            cp -f "$executeName" "Versions/A/"
            cp -f "$executeName" "Versions/Current/"
            cd ..
        fi
    done
}

# Replace paths in PAGExporter main executable
replacePathInExporter(){
  cd "$1/Contents/MacOS"
  executeName="PAGExporter"
  startReplacePathInExecuteFile "$executeName"
}

# Replace paths in dylib files recursively
replacePathInDylib(){
  cd "$1"
  for file in *
    do
      if [ -d "$file" ]; then
        replacePathInDylib $file $2
      elif [ "${file##*.}"x = "dylib"x ]; then
        startReplacePathInExecuteFile "$file" $2
      fi
    done
    cd ..
}

ExporterPath="$1"
ExporterFrameWorkPath="$ExporterPath/Contents/Frameworks"
ExporterPluginPath="$ExporterPath/Contents/PlugIns"
ExporterResourcePath="$ExporterPath/Contents/Resources/qml"

replacePathInFrameworks "$ExporterFrameWorkPath"
replacePathInDylib "$ExporterPluginPath"
replacePathInDylib "$ExporterResourcePath" true
replacePathInExporter "$ExporterPath"
