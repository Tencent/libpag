#!/bin/bash

# Shared directory for PAGExporter Qt resources
SHARED_QT_DIR="/Library/Application Support/PAGExporter"

# Legacy paths for cleaning up old Qt resources copied into AE apps
AE_PLUGIN_PATH='/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore'
AE_EXPORTER_PATH="$AE_PLUGIN_PATH/PAGExporter.plugin"

echo "Cleaning up PAGExporter Qt resources..."

# Remove the shared Qt directory (current version)
if [ -d "$SHARED_QT_DIR" ]; then
    echo "  Removing shared Qt directory: $SHARED_QT_DIR"
    sudo rm -rf "$SHARED_QT_DIR"
    echo "  Shared Qt directory removed"
fi

# Clean up legacy Qt resources from AE apps (for users upgrading from old versions)
function deleteFileFromAEApps() {
    local deleteFile="$1"
    local parentDir="$2"
    for ((i=2017; i<=2030; i++)); do
        for appPath in "/Applications/Adobe After Effects $i/Adobe After Effects $i.app" \
                       "/Applications/Adobe After Effects CC $i/Adobe After Effects CC $i.app"; do
            if [ -d "$appPath" ]; then
                local targetPath="$appPath/Contents/$parentDir/$deleteFile"
                if [ -d "$targetPath" ]; then
                    rm -rf "$targetPath"
                elif [ -f "$targetPath" ]; then
                    rm -f "$targetPath"
                fi
            fi
        done
    done
}

function cleanupLegacyQtResources() {
    if [ ! -d "$AE_EXPORTER_PATH" ]; then
        return
    fi

    # Clean up Qt frameworks copied to AE apps
    if [ -d "$AE_EXPORTER_PATH/Contents/Frameworks" ]; then
        for item in "$AE_EXPORTER_PATH/Contents/Frameworks"/*; do
            if [ -e "$item" ]; then
                deleteFileFromAEApps "$(basename "$item")" "Frameworks"
            fi
        done
    fi

    # Clean up Qt plugins copied to AE apps
    if [ -d "$AE_EXPORTER_PATH/Contents/PlugIns" ]; then
        for item in "$AE_EXPORTER_PATH/Contents/PlugIns"/*; do
            if [ -e "$item" ]; then
                deleteFileFromAEApps "$(basename "$item")" "PlugIns"
            fi
        done
    fi

    # Clean up Qt resources copied to AE apps
    if [ -d "$AE_EXPORTER_PATH/Contents/Resources" ]; then
        for item in "$AE_EXPORTER_PATH/Contents/Resources"/*; do
            if [ -e "$item" ]; then
                deleteFileFromAEApps "$(basename "$item")" "Resources"
            fi
        done
    fi
}

cleanupLegacyQtResources

echo "PAGExporter Qt resources cleanup completed"
