#!/bin/bash
AE_PLUGIN_PATH='/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore'
AE_EXPORTER_PATH="$AE_PLUGIN_PATH/PAGExporter.plugin"

# Locate PAGViewer.app
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

echo "✓ Found PAGViewer: $PAGVIEWER_APP"

# Copy Qt resources from PAGViewer to PAGExporter plugin
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

    # Copy Qt frameworks
    if [ -d "$VIEWER_FRAMEWORKS" ]; then
        for framework in "$VIEWER_FRAMEWORKS"/Qt*.framework; do
            if [ -d "$framework" ]; then
                frameworkName=$(basename "$framework")
                echo "  - $frameworkName"
                cp -R "$framework" "$PLUGIN_FRAMEWORKS/"
            fi
        done
    fi

    # Copy Qt plugins
    if [ -d "$VIEWER_PLUGINS" ]; then
        for plugin in "$VIEWER_PLUGINS"/*; do
            if [ -d "$plugin" ]; then
                pluginName=$(basename "$plugin")
                echo "  - $pluginName"
                cp -R "$plugin" "$PLUGIN_PLUGINS/"
            fi
        done
    fi

    # Copy Qt translation files
    if [ -d "$VIEWER_RESOURCES" ]; then
        for resource in "$VIEWER_RESOURCES"/*.qm; do
            if [ -f "$resource" ]; then
                resourceName=$(basename "$resource")
                echo "  - $resourceName"
                cp "$resource" "$PLUGIN_RESOURCES/"
            fi
        done
    fi

    # Generate qt.conf for plugin
    # When plugin loads as dylib, Qt searches for qt.conf in library directory
    # Relative paths calculated from MacOS directory, need ../ to access sibling directories
    PLUGIN_MACOS="$AE_EXPORTER_PATH/Contents/MacOS"
    PLUGIN_QT_CONF="$PLUGIN_MACOS/qt.conf"
    echo "  - qt.conf (generated in MacOS directory)"
    cat > "$PLUGIN_QT_CONF" << 'EOF'
[Paths]
Plugins = ../PlugIns
Imports = ../Resources/qml
QmlImports = ../Resources/qml
EOF

    # Copy QML resources
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
          mkdir "$targetPath"
        fi

        for file in *
          do
            cp -r -f "$file" "$targetPath"
          done
  fi
}

function copyResourceToAEApp() {
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
  fi
}

function copyQtResouceToAEApp() {
    exporterPath="$1"
    copyResourceToAEApp "$1" "Frameworks"
    copyResourceToAEApp "$1" "PlugIns"
    copyResourceToAEApp "$1" "Resources"
}

# Execute the main function to copy Qt resources
copyQtFromViewerToPlugin
