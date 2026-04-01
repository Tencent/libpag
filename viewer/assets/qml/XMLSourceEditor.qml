import QtQuick
import QtQuick.Controls

Rectangle {
    id: xmlSourceEditor
    color: "#1E1E1E"

    property string text: ""
    property bool editMode: false
    property var linesModel: null  // XmlLinesModel from C++

    // Current editing line index (-1 means not editing any line)
    property int editingLineIndex: -1

    signal contentEdited(string newText)

    // Reset the editor state (called on file switch)
    function reset(newText) {
        console.log("[XMLSourceEditor] reset called: newText.length =", newText ? newText.length : "undefined");
        exitEditMode(false);  // Exit without saving
        if (newText !== undefined) {
            text = newText;
        }
        listView.positionViewAtBeginning();
    }

    // Enter edit mode for a specific line
    function enterEditModeForLine(lineIndex) {
        if (lineIndex < 0 || !linesModel || lineIndex >= linesModel.lineCount) {
            return;
        }

        console.log("[XMLSourceEditor] enterEditModeForLine:", lineIndex);

        // If already editing another line, save it first
        if (editingLineIndex >= 0 && editingLineIndex !== lineIndex) {
            commitCurrentLine();
        }

        editMode = true;
        editingLineIndex = lineIndex;

        // Ensure the line is visible
        listView.positionViewAtIndex(lineIndex, ListView.Contain);
    }

    // Commit the current editing line and optionally move to next line
    function commitCurrentLine() {
        if (editingLineIndex < 0 || !linesModel) {
            return;
        }

        // The delegate will handle the actual commit via setLineText
        // This function just updates state
        console.log("[XMLSourceEditor] commitCurrentLine:", editingLineIndex);
    }

    // Move to next line for editing
    function moveToNextLine() {
        if (editingLineIndex < 0 || !linesModel) {
            return;
        }

        let nextIndex = editingLineIndex + 1;
        if (nextIndex < linesModel.lineCount) {
            enterEditModeForLine(nextIndex);
        } else {
            // At last line, exit edit mode
            exitEditMode(true);
        }
    }

    // Move to previous line for editing
    function moveToPreviousLine() {
        if (editingLineIndex < 0 || !linesModel) {
            return;
        }

        let prevIndex = editingLineIndex - 1;
        if (prevIndex >= 0) {
            enterEditModeForLine(prevIndex);
        }
    }

    // Exit edit mode
    function exitEditMode(save) {
        if (editingLineIndex >= 0 && save) {
            // Commit will be handled by delegate
        }
        editMode = false;
        editingLineIndex = -1;

        // Update text property from linesModel
        if (linesModel) {
            text = linesModel.allText();
        }
    }

    function getText() {
        if (linesModel) {
            return linesModel.allText();
        }
        return text;
    }

    // Get current scroll position
    function getScrollPosition() {
        return listView.contentY;
    }

    // Set scroll position (used to restore after model reset)
    function setScrollPosition(position) {
        Qt.callLater(function() {
            listView.contentY = position;
        });
    }

    // Line number width calculation
    readonly property int lineNumberWidth: 50  // Fixed width for line numbers

    // ==================== ListView (unified view and edit) ====================
    ListView {
        id: listView
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        model: linesModel
        cacheBuffer: 2000

        property real maxLineWidth: linesModel ? linesModel.maxLineWidth : 0
        contentWidth: Math.max(width, maxLineWidth + xmlSourceEditor.lineNumberWidth + 20)

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: "#00000000" }
            contentItem: Rectangle {
                implicitWidth: 8
                implicitHeight: 100
                radius: 4
                color: "#AA4B4B5A"
            }
        }

        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: "#00000000" }
            contentItem: Rectangle {
                implicitWidth: 100
                implicitHeight: 8
                radius: 4
                color: "#AA4B4B5A"
            }
        }

        delegate: Item {
            id: lineDelegate
            required property int index
            required property int lineNumber
            required property string highlightedText
            required property string plainText

            width: listView.contentWidth
            height: 18

            property bool isEditing: xmlSourceEditor.editingLineIndex === index

            // Line number display
            Text {
                id: lineNumberText
                width: xmlSourceEditor.lineNumberWidth - 8
                height: parent.height
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                text: lineNumber
                font.family: "Menlo"
                font.pixelSize: 12
                color: lineDelegate.isEditing ? "#FFFFFF" : "#6E7681"
            }

            // Separator line
            Rectangle {
                id: separator
                x: xmlSourceEditor.lineNumberWidth - 4
                width: 1
                height: parent.height
                color: "#3C3C3C"
            }

            // Content area (starts after line number)
            Item {
                id: contentArea
                x: xmlSourceEditor.lineNumberWidth
                width: parent.width - xmlSourceEditor.lineNumberWidth
                height: parent.height

                // Highlighted text display (visible when not editing this line)
                Text {
                    id: displayText
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: implicitWidth
                    visible: !lineDelegate.isEditing
                    textFormat: Text.RichText
                    text: highlightedText
                    font.family: "Menlo"
                    font.pixelSize: 13
                }

                // Edit background (visible when editing this line)
                Rectangle {
                    anchors.fill: parent
                    visible: lineDelegate.isEditing
                    color: "#2D2D2D"
                    border.color: "#448EF9"
                    border.width: 1
                }

                // Highlighted text display layer (visible during editing for syntax highlighting)
                Text {
                    id: highlightedEditText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: lineDelegate.isEditing
                    text: highlightedText
                    textFormat: Text.RichText
                    font.family: "Menlo"
                    font.pixelSize: 13
                }

                // TextInput for editing (transparent text, only shows cursor and handles input)
                TextInput {
                    id: lineEditor
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: lineDelegate.isEditing
                    font.family: "Menlo"
                    font.pixelSize: 13
                    color: "transparent"  // Text is invisible, we show highlightedEditText instead
                    selectionColor: "#264F78"
                    selectedTextColor: "#FFFFFF"  // Selection text needs to be visible
                    selectByMouse: true
                    cursorVisible: true

                    // Use a property to track if we're initializing to avoid binding loop
                    property bool initializing: false

                    // Initialize text when entering edit mode (avoids binding loop)
                    Connections {
                        target: lineDelegate
                        function onIsEditingChanged() {
                            if (lineDelegate.isEditing) {
                                lineEditor.initializing = true;
                                lineEditor.text = plainText;
                                lineEditor.initializing = false;
                            }
                        }
                    }

                    // Custom cursor rectangle to make cursor visible
                    cursorDelegate: Rectangle {
                        width: 2
                        color: "#FFFFFF"
                        visible: lineEditor.cursorVisible && lineEditor.focus

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: lineEditor.cursorVisible && lineEditor.focus
                            PropertyAnimation { to: 1.0; duration: 0 }
                            PauseAnimation { duration: 500 }
                            PropertyAnimation { to: 0.0; duration: 0 }
                            PauseAnimation { duration: 500 }
                        }
                    }

                    onVisibleChanged: {
                        if (visible) {
                            forceActiveFocus();
                            // Position cursor at end by default
                            cursorPosition = text.length;
                        }
                    }

                    // Commit changes when editing ends
                    onEditingFinished: {
                        if (lineDelegate.isEditing && linesModel) {
                            linesModel.setLineText(index, text);
                        }
                    }

                    // Handle text changes for real-time preview
                    onTextChanged: {
                        if (lineEditor.initializing) {
                            return;  // Skip during initialization to avoid loops
                        }
                        if (lineDelegate.isEditing && linesModel) {
                            linesModel.setLineText(index, text);
                            xmlSourceEditor.contentEdited(xmlSourceEditor.getText());
                        }
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            // Commit and move to next line
                            if (linesModel) {
                                linesModel.setLineText(index, text);
                            }
                            xmlSourceEditor.moveToNextLine();
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Escape) {
                            // Cancel editing and exit edit mode
                            xmlSourceEditor.exitEditMode(false);
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Up) {
                            // Move to previous line
                            if (linesModel) {
                                linesModel.setLineText(index, text);
                            }
                            xmlSourceEditor.moveToPreviousLine();
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Down) {
                            // Move to next line
                            if (linesModel) {
                                linesModel.setLineText(index, text);
                            }
                            xmlSourceEditor.moveToNextLine();
                            event.accepted = true;
                        }
                    }
                }

                // Mouse area for click/double-click to enter edit mode or switch line
                MouseArea {
                    anchors.fill: parent
                    cursorShape: lineDelegate.isEditing ? Qt.IBeamCursor : Qt.ArrowCursor
                    enabled: !lineDelegate.isEditing
                    
                    // Single click: if already in edit mode, switch to this line
                    onClicked: function(mouse) {
                        if (xmlSourceEditor.editingLineIndex >= 0 && xmlSourceEditor.editingLineIndex !== index) {
                            // Already editing another line, switch to this line
                            xmlSourceEditor.enterEditModeForLine(index);
                            
                            // Position cursor based on click position
                            Qt.callLater(function() {
                                if (lineEditor.visible && linesModel) {
                                    let charOffset = Math.floor((mouse.x + listView.contentX - xmlSourceEditor.lineNumberWidth) / linesModel.charWidth);
                                    charOffset = Math.max(0, Math.min(charOffset, lineEditor.text.length));
                                    lineEditor.cursorPosition = charOffset;
                                    lineEditor.forceActiveFocus();
                                }
                            });
                        }
                    }
                    
                    // Double click: enter edit mode for this line
                    onDoubleClicked: function(mouse) {
                        xmlSourceEditor.enterEditModeForLine(index);

                        // Position cursor based on click position
                        Qt.callLater(function() {
                            if (lineEditor.visible && linesModel) {
                                let charOffset = Math.floor((mouse.x + listView.contentX - xmlSourceEditor.lineNumberWidth) / linesModel.charWidth);
                                charOffset = Math.max(0, Math.min(charOffset, lineEditor.text.length));
                                lineEditor.cursorPosition = charOffset;
                                lineEditor.forceActiveFocus();
                            }
                        });
                    }
                }
            }
        }
    }

    // Click outside to exit edit mode
    MouseArea {
        anchors.fill: parent
        z: -1
        propagateComposedEvents: true
        onClicked: function(mouse) {
            if (editMode) {
                exitEditMode(true);
            }
            mouse.accepted = false;
        }
    }

    // Edit mode indicator
    Rectangle {
        visible: !editMode
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 12
        width: hintText.implicitWidth + 16
        height: 24
        radius: 4
        color: "#3C3C3C"
        z: 10

        Text {
            id: hintText
            anchors.centerIn: parent
            text: qsTr("Double-click to edit")
            color: "#80FFFFFF"
            font.pixelSize: 11
        }
    }

    // Edit mode border indicator
    Rectangle {
        visible: editMode
        anchors.fill: parent
        color: "transparent"
        border.color: "#448EF9"
        border.width: 2
        radius: 2
    }

    // Editing line indicator
    Rectangle {
        visible: editMode
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 12
        width: editHintText.implicitWidth + 16
        height: 24
        radius: 4
        color: "#3C5C3C"
        z: 10

        Text {
            id: editHintText
            anchors.centerIn: parent
            text: qsTr("Editing line %1 (Enter=next, Esc=exit)").arg(editingLineIndex + 1)
            color: "#80FFFFFF"
            font.pixelSize: 11
        }
    }

    // Toast notification
    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 16
        width: toastText.implicitWidth + 32
        height: 36
        radius: 18
        color: toastSuccess ? "#2E7D32" : "#C62828"
        opacity: 0
        visible: opacity > 0
        z: 100

        property bool toastSuccess: true

        Row {
            anchors.centerIn: parent
            spacing: 8

            Text {
                id: toastIcon
                text: toast.toastSuccess ? "✓" : "✗"
                color: "#FFFFFF"
                font.pixelSize: 14
                font.bold: true
            }

            Text {
                id: toastText
                text: ""
                color: "#FFFFFF"
                font.pixelSize: 13
            }
        }

        SequentialAnimation {
            id: toastAnimation
            NumberAnimation {
                target: toast
                property: "opacity"
                to: 0.95
                duration: 200
                easing.type: Easing.OutQuad
            }
            PauseAnimation {
                duration: 2000
            }
            NumberAnimation {
                target: toast
                property: "opacity"
                to: 0
                duration: 300
                easing.type: Easing.InQuad
            }
        }
    }

    function showToast(message, success) {
        toast.toastSuccess = success !== false;
        toastText.text = message;
        toastAnimation.stop();
        toast.opacity = 0;
        toastAnimation.start();
    }
}
