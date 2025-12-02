AE_PLUGIN_PATH='/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore'
AE_EXPORTER_PATH="$AE_PLUGIN_PATH/PAGExporter.plugin"
QT_FRAMEWORK_DRI="$AE_EXPORTER_PATH/Contents/Frameworks"
TARGET_QML_DIR="$AE_EXPORTER_PATH/Contents/Resources/qml"

function startReplacePathInExecuteFile(){
    executeName=$1
    isQML="$2"
    echo "executeName: $executeName"
    currentPath=`pwd`
    echo "currentPath : $currentPath"


    otool -L $executeName
    for linkPath in `otool -L $executeName | tr " " "\?"`
    do
        # 替换 execute Path
        if [[ "$linkPath" =~ ^@executable_path/\.\./Frameworks ]]; then
           oldPath=`echo "$linkPath" | grep -e "^@executable_path/\.\./Frameworks/[A-Z|a-z|0-9|/|\.]*" -o`
           newPath=`echo "$oldPath" | sed -e "s/^@executable_path\/\.\.\/Frameworks//"`
           newPath="@rpath$newPath"
           install_name_tool -change "$oldPath" "$newPath" $executeName
           echo "change oldPath:$oldPath to newPath:$newPath"
        fi
    done

    # 替换 id
    idName=`otool -l $executeName | grep -A 10 LC_ID_DYLIB | grep name`
    echo "idName:$idName"
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
      echo "change id name [$idName] to [$newIdName]"
    else
      echo "has no id name"
    fi

    # 替换 id
#    idPath=`echo "$currentPath" | sed -e "s/.*PAGExporter\.plugin//"`
#    idPath="$AE_EXPORTER_PATH$idPath/$executeName"
#    echo "newIdPath:$idPath"
#    install_name_tool -id "$idPath" $executeName

    # 删除原来 rpath
    install_name_tool -delete_rpath "@executable_path/../Frameworks" $executeName

    if [ "$isQML" == "true" ]; then
      install_name_tool -add_rpath "$TARGET_QML_DIR" $executeName
      install_name_tool -add_rpath "$QT_FRAMEWORK_DRI" $executeName
    else
      install_name_tool -add_rpath "$QT_FRAMEWORK_DRI" $executeName
    fi

    echo -e "\n"
}

# 修改 Qt 依赖的 Framewrok 路径
function replacePathInFrameworks(){
    echo "replacePathInFrameworks"
    cd "$1"
    pwd
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
        else
            echo "$file is not framework"
        fi
    done
}

# 修改 Exporter 主工程中依赖路径
replacePathInExporter(){
  echo "replacePathInExporter"
  cd "$1/Contents/MacOS"
  pwd
  executeName="PAGExporter"
  startReplacePathInExecuteFile "$executeName"
}

# 修改 Resource 下文件依赖路径
replacePathInDylib(){
  cd "$1"
  pwd
  for file in *
    do
      if [ -d "$file" ]; then
        echo "$file is a directory, go into it"
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
echo "ExporterPath: $ExporterPath"

replacePathInFrameworks "$ExporterFrameWorkPath"
echo -e "\n"
echo "repalcePathInPlugins"
replacePathInDylib "$ExporterPluginPath"
echo "replacePathInResource"
replacePathInDylib "$ExporterResourcePath" true
echo -e "\n"
replacePathInExporter "$ExporterPath"






