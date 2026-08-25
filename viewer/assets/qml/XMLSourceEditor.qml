import QtQuick
import QtQuick.Controls

// Free-form PAGX XML source editor, aligned with the web playground's CodeMirror panel:
// modeless editing (no enter/exit edit mode), a real Discard baseline, well-formedness
// validation on Apply, an always-visible Discard/Apply/Save bar, and per-block syntax
// highlighting provided by the C++ XmlDocumentHighlighter attached through the viewModel.
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

    readonly property bool modified: textArea.text !== baselineXml
    readonly property bool hasDocument: textArea.text.length > 0
    // Exposed so the host's plain-"L" toggle shortcut can avoid swallowing keystrokes while typing.
    readonly property bool editorFocused: textArea.activeFocus

    color: backgroundColor

    function loadXml(xml) {
        baselineXml = xml;
        textArea.text = xml;
        flick.contentX = 0;
        flick.contentY = 0;
    }

    // Clears the editor, e.g. when the view type switches away from PAGX.
    function reset() {
        loadXml("");
    }

    function getText() {
        return textArea.text;
    }

    function handleDiscard() {
        if (!modified) {
            showToast(qsTr("Nothing to discard"), true);
            return;
        }
        textArea.text = baselineXml;
        showToast(qsTr("Changes discarded"), true);
    }

    function handleApply() {
        if (!viewModel) {
            return;
        }
        const validationError = viewModel.validateXml(textArea.text);
        if (validationError !== "") {
            showToast(validationError, false);
            return;
        }
        const error = viewModel.applyXmlChanges(textArea.text);
        if (error === "") {
            // Advance the baseline so a later Discard restores what the canvas now shows.
            baselineXml = textArea.text;
            showToast(qsTr("Changes applied"), true);
        } else {
            showToast(error, false);
        }
    }

    function handleSave() {
        if (!viewModel) {
            return;
        }
        const validationError = viewModel.validateXml(textArea.text);
        if (validationError !== "") {
            showToast(validationError, false);
            return;
        }
        const applyError = viewModel.applyXmlChanges(textArea.text);
        if (applyError !== "") {
            showToast(applyError, false);
            return;
        }
        const saveError = viewModel.saveXmlToFile(textArea.text);
        if (saveError === "") {
            baselineXml = textArea.text;
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

                const lineHeight = textArea.cursorRectangle.height;
                if (lineHeight <= 0 || textArea.lineCount <= 0) {
                    return;
                }
                // One extra line above/below the viewport keeps numbers ahead of fast scrolls.
                const firstLine = Math.max(0, Math.floor(flick.contentY / lineHeight) - 1);
                const lastLine = Math.min(textArea.lineCount,
                                          Math.ceil((flick.contentY + height) / lineHeight) + 1);
                // cursorRectangle is in content coordinates, so it identifies the caret line.
                const caretLine = Math.round(textArea.cursorRectangle.y / lineHeight);

                context.font = "12px Menlo";
                context.textAlign = "right";
                for (let line = firstLine; line < lastLine; ++line) {
                    const y = line * lineHeight - flick.contentY;
                    context.fillStyle = (line === caretLine) ? root.gutterActiveTextColor
                                                             : root.gutterTextColor;
                    context.fillText(String(line + 1), width - 10, y + lineHeight * 0.5 + 4);
                }
            }
        }

        Flickable {
            id: flick
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: gutter.right
            anchors.right: parent.right

            contentWidth: Math.max(width, textArea.contentWidth)
            contentHeight: Math.max(height, textArea.contentHeight)
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
                width: Math.max(flick.width, contentWidth)
                height: Math.max(flick.height, contentHeight)

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
                enabled: root.hasDocument
                onClicked: root.handleDiscard()
            }

            EditorButton {
                label: qsTr("Apply")
                normalColor: "#448EF9"
                hoverColor: "#8BC4FF"
                enabled: root.hasDocument
                onClicked: root.handleApply()
            }

            EditorButton {
                label: qsTr("Save")
                normalColor: "#388E3C"
                hoverColor: "#81C784"
                enabled: root.hasDocument
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
