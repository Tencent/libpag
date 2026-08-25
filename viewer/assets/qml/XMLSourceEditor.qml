import QtQuick
import QtQuick.Controls

// Free-form PAGX XML source editor, aligned with the web playground's CodeMirror panel:
// modeless editing (no enter/exit edit mode), a real Discard baseline, well-formedness
// validation on Apply, an always-visible Discard/Apply/Save bar, and per-block syntax
// highlighting provided by the C++ XmlDocumentHighlighter attached through the viewModel.
//
// Performance notes for large documents: content width/height are estimated from
// lineCount * lineHeight and a pre-measured maxLineWidth instead of the document layout
// (whose size queries force a full layout of every block), and modified state is tracked
// with a dirty flag instead of comparing the full text on every change notification.
Rectangle {
    id: root

    readonly property color backgroundColor: "#1E1E1E"
    readonly property color buttonBarColor: "#16161D"
    readonly property color separatorColor: "#3C3C3C"
    readonly property color gutterTextColor: "#6E7681"
    readonly property color gutterActiveTextColor: "#FFFFFF"

    property var viewModel: null

    // Last accepted content: the file load or the latest successful Apply. Discard restores it.
    property string baselineXml: ""

    // True after any edit since the last accepted baseline.
    property bool dirty: false

    // Widest line in pixels, measured by the view model while loading. Drives the content
    // width so the document layout's ideal width is never queried.
    property real maxLineWidth: 0

    // Set while a large document is being loaded into the editor chunk by chunk.
    property bool busy: false

    // Exposed so the host's plain-"L" toggle shortcut can avoid swallowing keystrokes while typing.
    readonly property bool editorFocused: textArea.activeFocus

    // Single-line height in pixels; falls back to the Menlo 13px metric until the first
    // cursor rectangle is available. Constant for the fixed-pitch NoWrap editor.
    readonly property real lineHeight: textArea.cursorRectangle.height > 0
                                           ? textArea.cursorRectangle.height : 17

    readonly property bool modified: dirty
    readonly property bool hasDocument: textArea.length > 0

    color: backgroundColor

    function loadXml(xml) {
        baselineXml = xml;
        maxLineWidth = 0;
        flick.contentX = 0;
        flick.contentY = 0;
        if (viewModel) {
            // Large texts are appended asynchronously in chunks; the editor stays read-only
            // until editorLoadFinished() arrives so Apply/Save never see a partial document.
            busy = true;
            textArea.readOnly = true;
            viewModel.loadEditorText(textArea.textDocument, xml);
        } else {
            textArea.text = xml;
        }
    }

    // Clears the editor, e.g. when the view type switches away from PAGX.
    function reset() {
        loadXml("");
    }

    function getText() {
        return textArea.text;
    }

    function handleDiscard() {
        if (!dirty) {
            showToast(qsTr("Nothing to discard"), true);
            return;
        }
        // Restoring through the chunked loader too: assigning the text directly would make
        // the highlighter rehighlight the whole baseline synchronously.
        busy = true;
        textArea.readOnly = true;
        viewModel.loadEditorText(textArea.textDocument, baselineXml);
        showToast(qsTr("Changes discarded"), true);
    }

    function handleApply() {
        if (!viewModel) {
            return;
        }
        // Read the text once: each access copies the whole document out of the text backend.
        const text = textArea.text;
        const validationError = viewModel.validateXml(text);
        if (validationError !== "") {
            showToast(validationError, false);
            return;
        }
        const error = viewModel.applyXmlChanges(text);
        if (error === "") {
            // Advance the baseline so a later Discard restores what the canvas now shows.
            baselineXml = text;
            dirty = false;
            showToast(qsTr("Changes applied"), true);
        } else {
            showToast(error, false);
        }
    }

    function handleSave() {
        if (!viewModel) {
            return;
        }
        // Read the text once: each access copies the whole document out of the text backend.
        const text = textArea.text;
        const validationError = viewModel.validateXml(text);
        if (validationError !== "") {
            showToast(validationError, false);
            return;
        }
        const applyError = viewModel.applyXmlChanges(text);
        if (applyError !== "") {
            showToast(applyError, false);
            return;
        }
        const saveError = viewModel.saveXmlToFile(text);
        if (saveError === "") {
            baselineXml = text;
            dirty = false;
            showToast(qsTr("File saved"), true);
        } else {
            showToast(saveError, false);
        }
    }

    Connections {
        target: viewModel
        function onDocumentXmlChanged() {
            loadXml(viewModel.documentXml);
        }
        function onEditorLoadFinished(maxLineWidth) {
            root.maxLineWidth = maxLineWidth;
            textArea.readOnly = false;
            dirty = false;
            busy = false;
        }
    }

    // This panel outlives the view model: switching the view type rebuilds the content view
    // (and its view model) while the editor persists, so re-attach the highlighter and pick up
    // any already-loaded content whenever the viewModel reference changes. The initial binding
    // evaluation happens before textArea exists, hence the null guard and the onCompleted pass.
    onViewModelChanged: connectViewModel()
    Component.onCompleted: connectViewModel()

    function connectViewModel() {
        if (!viewModel || !textArea) {
            return;
        }
        viewModel.attachHighlighter(textArea.textDocument);
        if (viewModel.documentXml !== "") {
            loadXml(viewModel.documentXml);
        }
    }

    // Editor area: line-number gutter plus the scrollable text content.
    Item {
        id: editorArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: buttonBar.top
        clip: true

        Canvas {
            id: gutter
            width: 50
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            Connections {
                target: flick
                function onContentYChanged() {
                    gutter.requestPaint();
                }
            }

            Connections {
                target: textArea
                function onLineCountChanged() {
                    gutter.requestPaint();
                }
                function onCursorRectangleChanged() {
                    gutter.requestPaint();
                }
            }

            onPaint: {
                const context = getContext("2d");
                context.fillStyle = root.backgroundColor;
                context.fillRect(0, 0, width, height);
                context.fillStyle = root.separatorColor;
                context.fillRect(width - 1, 0, 1, height);

                const lineH = root.lineHeight;
                if (lineH <= 0 || textArea.lineCount <= 0) {
                    return;
                }
                // One extra line above/below the viewport keeps numbers ahead of fast scrolls.
                const firstLine = Math.max(0, Math.floor(flick.contentY / lineH) - 1);
                const lastLine = Math.min(textArea.lineCount,
                                          Math.ceil((flick.contentY + height) / lineH) + 1);
                // cursorRectangle is in content coordinates, so it identifies the caret line.
                const caretLine = Math.round(textArea.cursorRectangle.y / lineH);

                context.font = "12px Menlo";
                context.textAlign = "right";
                for (let line = firstLine; line < lastLine; ++line) {
                    const y = line * lineH - flick.contentY;
                    context.fillStyle = (line === caretLine) ? root.gutterActiveTextColor
                                                             : root.gutterTextColor;
                    context.fillText(String(line + 1), width - 10, y + lineH * 0.5 + 4);
                }
            }
        }

        Flickable {
            id: flick
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: gutter.right
            anchors.right: parent.right

            // Estimated from line metrics: reading the document's content size here would
            // force a full layout pass over every block and freeze the UI on large files.
            contentWidth: Math.max(width, root.maxLineWidth)
            contentHeight: Math.max(height, textArea.lineCount * root.lineHeight)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalAndVerticalFlick

            // Let TextArea handle selection drags while the Flickable handles scroll gestures.
            TextArea.flickable: textArea

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                background: Rectangle {
                    color: "#00000000"
                }
                contentItem: Rectangle {
                    implicitWidth: 8
                    implicitHeight: 100
                    radius: 4
                    color: "#AA4B4B5A"
                }
            }

            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AsNeeded
                background: Rectangle {
                    color: "#00000000"
                }
                contentItem: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 8
                    radius: 4
                    color: "#AA4B4B5A"
                }
            }

            TextArea {
                id: textArea
                width: Math.max(flick.width, root.maxLineWidth)
                height: Math.max(flick.height, textArea.lineCount * root.lineHeight)

                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.NoWrap
                selectByMouse: true
                persistentSelection: true
                color: "#D4D4D4"
                selectionColor: "#264F78"
                selectedTextColor: "#FFFFFF"
                font.family: "Menlo"
                font.pixelSize: 13
                padding: 0
                leftInset: 0
                rightInset: 0
                topInset: 0
                bottomInset: 0
                background: null

                onTextChanged: root.dirty = true

                onCursorRectangleChanged: {
                    // Keep the caret inside the viewport after big deletions or cursor jumps;
                    // the TextArea.flickable combination does not scroll to the caret on its
                    // own. Skip while the panel is still being laid out to avoid dragging the
                    // viewport to a bogus position derived from a zero-sized viewport.
                    if (flick.height <= 0 || flick.width <= 0) {
                        return;
                    }
                    const rect = textArea.cursorRectangle;
                    if (rect.y < flick.contentY) {
                        flick.contentY = Math.max(0, rect.y);
                    } else if (rect.y + rect.height > flick.contentY + flick.height) {
                        flick.contentY = Math.max(0, rect.y + rect.height - flick.height);
                    }
                    if (rect.x < flick.contentX) {
                        flick.contentX = Math.max(0, rect.x - 24);
                    } else if (rect.x + rect.width > flick.contentX + flick.width) {
                        flick.contentX = Math.max(0, rect.x + rect.width - flick.width + 24);
                    }
                }
            }
        }
    }

    // Always-visible action bar, mirroring the web editor's button row.
    Rectangle {
        id: buttonBar
        height: 48
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: root.buttonBarColor

        Row {
            anchors.centerIn: parent
            spacing: 12

            EditorButton {
                label: qsTr("Discard")
                normalColor: "#3C3C3C"
                hoverColor: "#8B8B9A"
                enabled: root.hasDocument && !root.busy
                onClicked: root.handleDiscard()
            }

            EditorButton {
                label: qsTr("Apply")
                normalColor: "#448EF9"
                hoverColor: "#8BC4FF"
                enabled: root.hasDocument && !root.busy
                onClicked: root.handleApply()
            }

            EditorButton {
                label: qsTr("Save")
                normalColor: "#388E3C"
                hoverColor: "#81C784"
                enabled: root.hasDocument && !root.busy
                onClicked: root.handleSave()
            }
        }
    }

    component EditorButton: Button {
        id: button

        required property string label
        required property color normalColor
        required property color hoverColor

        text: button.label
        scale: hovered ? 1.05 : 1.0

        Behavior on scale {
            NumberAnimation {
                duration: 100
            }
        }

        background: Rectangle {
            implicitWidth: 80
            implicitHeight: 32
            color: button.hovered ? Qt.lighter(button.normalColor, 1.25) : button.normalColor
            border.color: button.hovered ? button.hoverColor : Qt.lighter(button.normalColor, 1.5)
            border.width: 1
            radius: 4
        }

        contentItem: Text {
            text: button.text
            color: "#FFFFFF"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 12
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
