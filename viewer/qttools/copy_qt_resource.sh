#!/bin/bash
AE_PLUGIN_PATH='/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore'
AE_EXPORTER_PATH="$AE_PLUGIN_PATH/PAGExporter.plugin"

PAGVIEWER_APP=""
if [ -d "/Applications/PAGViewer.app" ]; then
    PAGVIEWER_APP="/Applications/PAGViewer.app"
elif [ -d "$HOME/Applications/PAGViewer.app" ]; then
    PAGVIEWER_APP="$HOME/Applications/PAGViewer.app"
else
    PAGVIEWER_APP=$(mdfind "kMDItemDisplayName == 'PAGViewer.app' && kMDItemContentType == 'com.apple.application-bundle'" | head -n 1)
fi

if [ -z "$PAGVIEWER_APP" ] || [ ! -d "$PAGVIEWER_APP" ]; then
    exit 1
fi

echo "✓ 找到 PAGViewer: $PAGVIEWER_APP"

function copyQtFromViewerToPlugin() {
    VIEWER_FRAMEWORKS="$PAGVIEWER_APP/Contents/Frameworks"
    VIEWER_PLUGINS="$PAGVIEWER_APP/Contents/PlugIns"
    VIEWER_RESOURCES="$PAGVIEWER_APP/Contents/Resources"
    
    PLUGIN_FRAMEWORKS="$AE_EXPORTER_PATH/Contents/Frameworks"
    PLUGIN_PLUGINS="$AE_EXPORTER_PATH/Contents/PlugIns"
    PLUGIN_RESOURCES="$AE_EXPORTER_PATH/Contents/Resources"

    mkdir -p "$PLUGIN_FRAMEWORKS"
    mkdir -p "$PLUGIN_PLUGINS"
    mkdir -p "$PLUGIN_RESOURCES"

    if [ -d "$VIEWER_FRAMEWORKS" ]; then
        for framework in "$VIEWER_FRAMEWORKS"/Qt*.framework; do
            if [ -d "$framework" ]; then
                frameworkName=$(basename "$framework")
                echo "  - $frameworkName"
                cp -R "$framework" "$PLUGIN_FRAMEWORKS/"
            fi
        done
    fi

    if [ -d "$VIEWER_PLUGINS" ]; then
        for plugin in "$VIEWER_PLUGINS"/*; do
            if [ -d "$plugin" ]; then
                pluginName=$(basename "$plugin")
                echo "  - $pluginName"
                cp -R "$plugin" "$PLUGIN_PLUGINS/"
            fi
        done
    fi

    if [ -d "$VIEWER_RESOURCES" ]; then
        for resource in "$VIEWER_RESOURCES"/*.qm; do
            if [ -f "$resource" ]; then
                resourceName=$(basename "$resource")
                echo "  - $resourceName"
                cp "$resource" "$PLUGIN_RESOURCES/"
            fi
        done
    fi

    # 为插件生成 qt.conf 到 MacOS 目录
    # 插件作为动态库加载时，Qt 在库所在目录查找 qt.conf
    # 相对路径从 MacOS 目录计算，需要使用 ../ 访问同级目录
    PLUGIN_MACOS="$AE_EXPORTER_PATH/Contents/MacOS"
    PLUGIN_QT_CONF="$PLUGIN_MACOS/qt.conf"
    echo "  - qt.conf (生成到 MacOS 目录)"
    cat > "$PLUGIN_QT_CONF" << 'EOF'
[Paths]
Plugins = ../PlugIns
Imports = ../Resources/qml
QmlImports = ../Resources/qml
EOF

    VIEWER_QML="$VIEWER_RESOURCES/qml"
    PLUGIN_QML="$PLUGIN_RESOURCES/qml"
    if [ -d "$VIEWER_QML" ]; then
        mkdir -p "$PLUGIN_QML"
        cp -R "$VIEWER_QML/"* "$PLUGIN_QML/"
        QML_COUNT=$(find "$PLUGIN_QML" -type d | wc -l | tr -d ' ')
    fi

    QT_FW_COUNT=$(find "$PLUGIN_FRAMEWORKS" -maxdepth 1 -name "Qt*.framework" -type d | wc -l | tr -d ' ')
}

function doCopyFileToAEApp() {
  aeAppPath="$1"
  if [ -d "$aeAppPath" ]; then
        targetPath="$aeAppPath/Contents/$dirName/"
        if [ ! -d "$targetPath" ]; then
          echo "mkdir $targetPath"
          mkdir "$targetPath"
        fi

        for file in *
          do
            echo "copy $file to $targetPath"
            cp -r -f "$file" "$targetPath"
          done
  fi
}

function copyResouceToAEApp() {
  dirName="$2"
  dirPath="$1/Contents/$2"
  if [ -d "$dirPath" ]; then
    cd "$dirPath"
    for((i=2017;i<=2030;i++));
      do
        aeAppPath="/Applications/Adobe After Effects $i/Adobe After Effects $i.app"
        doCopyFileToAEApp "$aeAppPath"

        aeAppPath2="/Applications/Adobe After Effects CC $i/Adobe After Effects CC $i.app"
        doCopyFileToAEApp "$aeAppPath2"
      done
  else
    echo "$dirPath not exit"
  fi
  echo -e "\n"
}

function copyQtResouceToAEApp() {
    
    exporterPath="$1"
    copyResouceToAEApp "$1" "Frameworks"
    copyResouceToAEApp "$1" "PlugIns"
    copyResouceToAEApp "$1" "Resources"
}

function signCopiedFiles() {
    echo "签名复制的Qt组件..."
    
    PLUGIN_FRAMEWORKS="$AE_EXPORTER_PATH/Contents/Frameworks"
    PLUGIN_PLUGINS="$AE_EXPORTER_PATH/Contents/PlugIns"
    PLUGIN_QML="$AE_EXPORTER_PATH/Contents/Resources/qml"
    
    # 先清除扩展属性，避免签名问题
    xattr -cr "$AE_EXPORTER_PATH" 2>/dev/null || true
    
    # 修改所有文件的所有者为当前用户（确保有权限签名）
    # 注意：此脚本在 osascript with administrator privileges 上下文中执行时以 root 运行
    # 但签名操作需要对文件有写入权限
    chown -R "$(logname 2>/dev/null || echo $USER):admin" "$AE_EXPORTER_PATH" 2>/dev/null || true
    
    # 签名所有 Qt Frameworks
    if [ -d "$PLUGIN_FRAMEWORKS" ]; then
        for fw in "$PLUGIN_FRAMEWORKS"/*.framework; do
            if [ -d "$fw" ]; then
                fwName=$(basename "$fw")
                if codesign --force --deep --sign - "$fw" 2>&1; then
                    echo "  ✓ 签名 $fwName"
                else
                    echo "  ✗ 签名失败 $fwName (exit code: $?)"
                fi
            fi
        done
        
        # 签名 libffaudio.dylib
        if [ -f "$PLUGIN_FRAMEWORKS/libffaudio.dylib" ]; then
            if codesign --force --sign - "$PLUGIN_FRAMEWORKS/libffaudio.dylib" 2>&1; then
                echo "  ✓ 签名 libffaudio.dylib"
            else
                echo "  ✗ 签名失败 libffaudio.dylib (exit code: $?)"
            fi
        fi
    fi
    
    # 签名 PlugIns 目录下的 dylib
    if [ -d "$PLUGIN_PLUGINS" ]; then
        find "$PLUGIN_PLUGINS" -name "*.dylib" -exec codesign --force --sign - {} \; 2>&1
        echo "  ✓ 签名 PlugIns 目录"
    fi
    
    # 签名 QML 目录下的 dylib
    if [ -d "$PLUGIN_QML" ]; then
        find "$PLUGIN_QML" -name "*.dylib" -exec codesign --force --sign - {} \; 2>&1
        echo "  ✓ 签名 Resources/qml 目录"
    fi
    
    # 最后签名整个插件
    if codesign --force --deep --sign - "$AE_EXPORTER_PATH" 2>&1; then
        echo "  ✓ 签名 PAGExporter.plugin"
    else
        echo "  ✗ 签名失败 PAGExporter.plugin (exit code: $?)"
    fi
    
    echo "签名完成"
}

copyQtFromViewerToPlugin
copyQtResouceToAEApp "$AE_EXPORTER_PATH"


SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/replace_qt_path.sh" ]; then
    sh "$SCRIPT_DIR/replace_qt_path.sh" "$AE_EXPORTER_PATH"
fi

# 签名所有复制的组件，避免 macOS Gatekeeper 导致 AE 崩溃
signCopiedFiles