#!/bin/bash

# Shared directory for PAGExporter Qt resources (outside plugin bundle to preserve code signature)
SHARED_QT_DIR="/Library/Application Support/PAGExporter"

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
    echo "Error: PAGViewer.app not found!"
    echo "PAGExporter requires PAGViewer to be installed first."
    echo "Please install PAGViewer.app in /Applications/ or ~/Applications/"
    exit 1
fi

echo "Found PAGViewer: $PAGVIEWER_APP"

# Verify PAGViewer has the required Qt resources
VIEWER_FRAMEWORKS="$PAGVIEWER_APP/Contents/Frameworks"
if [ ! -d "$VIEWER_FRAMEWORKS" ] || [ -z "$(ls -A "$VIEWER_FRAMEWORKS"/Qt*.framework 2>/dev/null)" ]; then
    echo "Error: PAGViewer installation appears incomplete (missing Qt frameworks)"
    echo "Please reinstall PAGViewer.app"
    exit 1
fi

# Remove existing shared directory if exists
if [ -d "$SHARED_QT_DIR" ]; then
    echo "Removing existing shared Qt directory..."
    sudo rm -rf "$SHARED_QT_DIR"
fi

# Create shared Qt directory structure
echo "Creating shared Qt directory: $SHARED_QT_DIR"
sudo mkdir -p "$SHARED_QT_DIR/Frameworks"
sudo mkdir -p "$SHARED_QT_DIR/PlugIns"
sudo mkdir -p "$SHARED_QT_DIR/Resources"

# Copy Qt frameworks
echo "Copying Qt frameworks..."
for framework in "$VIEWER_FRAMEWORKS"/Qt*.framework; do
    if [ -d "$framework" ]; then
        frameworkName=$(basename "$framework")
        echo "  - $frameworkName"
        sudo cp -R "$framework" "$SHARED_QT_DIR/Frameworks/"
    fi
done

# Copy Qt plugins
echo "Copying Qt plugins..."
VIEWER_PLUGINS="$PAGVIEWER_APP/Contents/PlugIns"
if [ -d "$VIEWER_PLUGINS" ]; then
    for plugin in "$VIEWER_PLUGINS"/*; do
        if [ -d "$plugin" ]; then
            pluginName=$(basename "$plugin")
            echo "  - $pluginName"
            sudo cp -R "$plugin" "$SHARED_QT_DIR/PlugIns/"
        fi
    done
fi

# Copy QML resources
VIEWER_QML="$PAGVIEWER_APP/Contents/Resources/qml"
if [ -d "$VIEWER_QML" ]; then
    echo "Copying QML resources..."
    sudo cp -R "$VIEWER_QML" "$SHARED_QT_DIR/Resources/"
fi

# Copy Qt translation files
echo "Copying Qt translations..."
VIEWER_RESOURCES="$PAGVIEWER_APP/Contents/Resources"
for resource in "$VIEWER_RESOURCES"/*.qm; do
    if [ -f "$resource" ]; then
        resourceName=$(basename "$resource")
        echo "  - $resourceName"
        sudo cp "$resource" "$SHARED_QT_DIR/Resources/"
    fi
done

echo "Qt resources copied to shared directory: $SHARED_QT_DIR"
