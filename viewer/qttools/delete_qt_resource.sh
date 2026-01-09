#!/bin/bash

# Shared directory for PAGExporter Qt resources
SHARED_QT_DIR="/Library/Application Support/PAGExporter"

echo "Cleaning up PAGExporter Qt resources..."

# Remove the shared Qt directory
if [ -d "$SHARED_QT_DIR" ]; then
    echo "  Removing shared Qt directory: $SHARED_QT_DIR"
    sudo rm -rf "$SHARED_QT_DIR"
    echo "Shared Qt directory removed"
else
    echo "  Shared Qt directory not found: $SHARED_QT_DIR"
fi

echo "PAGExporter Qt resources cleanup completed"
