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
    copyResouceToAEApp "$1" "Frameworks"
    copyResouceToAEApp "$1" "PlugIns"
    copyResouceToAEApp "$1" "Resources"
}

# Code sign all copied files to prevent macOS Gatekeeper crashes in AE
function signCopiedFiles() {
    echo "Signing copied Qt components..."
    
    PLUGIN_FRAMEWORKS="$AE_EXPORTER_PATH/Contents/Frameworks"
    PLUGIN_PLUGINS="$AE_EXPORTER_PATH/Contents/PlugIns"
    PLUGIN_QML="$AE_EXPORTER_PATH/Contents/Resources/qml"
    
    # Remove extended attributes to avoid signing issues
    xattr -cr "$AE_EXPORTER_PATH" 2>/dev/null || true
    
    # Change ownership to current user (ensure signing permissions)
    # Note: When executed via osascript with administrator privileges, runs as root
    # But signing requires write permissions to files
    chown -R "$(logname 2>/dev/null || echo $USER):admin" "$AE_EXPORTER_PATH" 2>/dev/null || true
    
    # Sign all Qt frameworks
    if [ -d "$PLUGIN_FRAMEWORKS" ]; then
        for fw in "$PLUGIN_FRAMEWORKS"/*.framework; do
            if [ -d "$fw" ]; then
                fwName=$(basename "$fw")
                if codesign --force --deep --sign - "$fw" 2>&1; then
                    echo "  ✓ Signed $fwName"
                else
                    echo "  ✗ Failed to sign $fwName (exit code: $?)"
                fi
            fi
        done
        
        # Sign libffaudio.dylib
        if [ -f "$PLUGIN_FRAMEWORKS/libffaudio.dylib" ]; then
            if codesign --force --sign - "$PLUGIN_FRAMEWORKS/libffaudio.dylib" 2>&1; then
                echo "  ✓ Signed libffaudio.dylib"
            else
                echo "  ✗ Failed to sign libffaudio.dylib (exit code: $?)"
            fi
        fi
    fi
    
    # Sign dylibs in PlugIns directory
    if [ -d "$PLUGIN_PLUGINS" ]; then
        find "$PLUGIN_PLUGINS" -name "*.dylib" -exec codesign --force --sign - {} \; 2>&1
        echo "  ✓ Signed PlugIns directory"
    fi
    
    # Sign dylibs in QML directory
    if [ -d "$PLUGIN_QML" ]; then
        find "$PLUGIN_QML" -name "*.dylib" -exec codesign --force --sign - {} \; 2>&1
        echo "  ✓ Signed Resources/qml directory"
    fi
    
    # Finally sign the entire plugin
    if codesign --force --deep --sign - "$AE_EXPORTER_PATH" 2>&1; then
        echo "  ✓ Signed PAGExporter.plugin"
    else
        echo "  ✗ Failed to sign PAGExporter.plugin (exit code: $?)"
    fi
    
    echo "Signing completed"
}

copyQtFromViewerToPlugin
copyQtResouceToAEApp "$AE_EXPORTER_PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/replace_qt_path.sh" ]; then
    sh "$SCRIPT_DIR/replace_qt_path.sh" "$AE_EXPORTER_PATH"
fi

signCopiedFiles
