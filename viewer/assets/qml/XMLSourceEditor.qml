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

    // True when the Source Editor tab is the front tab of the data panel, set by the host
    // (StackLayout hides inactive children by zeroing geometry, so the visible property is
    // not a reliable activation signal).
    property bool isActive: false

    // Lazy loading: documentXmlChanged fires as soon as a file is parsed, but the multi-second
    // chunked load should not compete with the default Layer panel. The text is deferred until
    // the user actually switches to the Source Editor tab; the switch happens instantly and the
    // busy overlay with the progress bar shows while the content streams in.
    property bool needsDocumentLoad: false

    // Viewport position to restore once loading finishes: kept for Discard (stay where the
    // user was editing), zero for fresh file loads.
    property real savedScrollY: 0

    // Chunked loading progress in the 0..1 range, fed by editorLoadProgress.
    property real loadProgress: 0

    // StackLayout zeroes the geometry of inactive tabs. Resizing the editor stack on every tab
    // switch forces the text backend to re-layout and rebuild render nodes, so the last
    // non-zero size is cached and used while the panel is hidden.
    property real cachedWidth: 0
    property real cachedHeight: 0
    onWidthChanged: if (width > 0) cachedWidth = width
    onHeightChanged: if (height > 0) cachedHeight = height
    readonly property real layoutWidth: width > 0 ? width : (cachedWidth > 0 ? cachedWidth : 1)
    readonly property real layoutHeight: height > 0 ? height : (cachedHeight > 0 ? cachedHeight : 1)

    // Single-line height in pixels; falls back to the Menlo 13px metric until the first
    // cursor rectangle is available. Constant for the fixed-pitch NoWrap editor.
    readonly property real lineHeight: textArea.cursorRectangle.height > 0
                                           ? textArea.cursorRectangle.height : 17

    // Cached line count. QQuickTextEdit::lineCount walks every document block on each read
    // (O(blockCount)), so it must never be referenced from per-line bindings: with the line
    // number pool that meant an O(blocks * visibleLines) storm on every scroll frame, which
    // froze the editor. This single binding is re-evaluated only on lineCountChanged.
    readonly property int documentLineCount: textArea.lineCount

    readonly property bool hasDocument: textArea.length > 0

    color: backgroundColor

    // Entry point for every "the document XML changed" notification. Loads immediately when
    // the Source Editor tab is front, otherwise just raises the deferred flag.
    function requestDocumentLoad() {
        if (isActive && viewModel) {
            loadXml(viewModel.documentXml);
        } else {
            needsDocumentLoad = true;
        }
    }

    onIsActiveChanged: {
        if (isActive && needsDocumentLoad && viewModel) {
            needsDocumentLoad = false;
            loadXml(viewModel.documentXml);
        }
    }

    function loadXml(xml, keepScrollPosition) {
        baselineXml = xml;
        maxLineWidth = 0;
        loadProgress = 0;
        savedScrollY = (keepScrollPosition === true) ? flick.contentY : 0;
        if (keepScrollPosition !== true) {
            flick.contentX = 0;
            flick.contentY = 0;
        }
        if (viewModel && typeof viewModel.loadEditorText === "function") {
            // Large texts are appended asynchronously in chunks; keyboard input is blocked
            // while busy (see Keys.onPressed in textArea) so Apply/Save never see partial
            // content. readOnly is deliberately NOT used: setting it makes Qt move the
            // cursor to the document end, which scrolls the Flickable to the bottom.
            // The capability check guards a view-type switch: reset() may run after the host
            // has rebound viewModel to a non-PAGX one that has no loadEditorText.
            busy = true;
            viewModel.loadEditorText(textArea.textDocument, xml);
        } else {
            textArea.text = xml;
        }
    }

    // Clears the editor, e.g. when the view type switches away from PAGX.
    function reset() {
        needsDocumentLoad = false;
        loadXml("");
    }

    function handleDiscard() {
        if (!dirty) {
            showToast(qsTr("Nothing to discard"), true);
            return;
        }
        // Fast path: the undo stack starts at the baseline (cleared after loading), so
        // undoing all edits restores it in time proportional to the edit size, keeping the
        // caret and viewport in place. Falls back to a chunked reload if the stack is gone.
        if (viewModel && viewModel.discardToBaseline(textArea.textDocument)) {
            dirty = false;
            showToast(qsTr("Changes discarded"), true);
            return;
        }
        loadXml(baselineXml, true);
        showToast(qsTr("Changes discarded"), true);
    }

    function handleApply() {
        if (!viewModel) {
            return;
        }
        // Read the text once: each access copies the whole document out of the text backend.
        const editorText = textArea.text;
        // Folded long data lines must round-trip back to their original content; a modified
        // marker cannot be restored safely, so refuse instead of silently losing data.
        if (viewModel.elideBroken(editorText)) {
            showToast(qsTr("A folded data line was modified. Discard to restore it."), false);
            return;
        }
        const text = viewModel.restoreElidedLines(editorText);
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
        const editorText = textArea.text;
        if (viewModel.elideBroken(editorText)) {
            showToast(qsTr("A folded data line was modified. Discard to restore it."), false);
            return;
        }
        const text = viewModel.restoreElidedLines(editorText);
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
            requestDocumentLoad();
        }
        function onEditorLoadProgress(progress) {
            root.loadProgress = progress;
        }

        function onEditorLoadFinished(maxLineWidth) {
            root.maxLineWidth = maxLineWidth;
            // Clearing the document pulls the caret to position 0, which drags the viewport
            // to the top; restore the saved position after the content is complete.
            flick.contentX = 0;
            flick.contentY = root.savedScrollY;
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

    // Remembers the view model already wired up so the initial binding evaluation and the
    // onCompleted pass do not both trigger a (duplicate) content load.
    property var connectedViewModel: null

    function connectViewModel() {
        if (!viewModel || !textArea || connectedViewModel === viewModel) {
            return;
        }
        connectedViewModel = viewModel;
        viewModel.attachHighlighter(textArea.textDocument);
        if (viewModel.documentXml !== "") {
            requestDocumentLoad();
        }
    }

    // Editor area: line-number gutter plus the scrollable text content.
    Item {
        id: editorArea
        width: root.layoutWidth
        height: root.layoutHeight - buttonBar.height
        clip: true

        // Line-number gutter. Numbers are a small pool of reused Text items positioned by the
        // scroll offset: scrolling only updates text and y bindings, so glyphs come from the
        // scene graph's texture atlas and no canvas texture is re-uploaded per frame (a full
        // Canvas redraw per scroll frame was a major contributor to GPU buffer churn).
        Item {
            id: gutter
            width: 50
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left

            Rectangle {
                anchors.fill: parent
                color: root.backgroundColor
            }

            Rectangle {
                width: 1
                height: parent.height
                anchors.right: parent.right
                color: root.separatorColor
            }

            // One extra line above/below the viewport keeps numbers ahead of fast scrolls.
            readonly property int visibleLineCount: Math.ceil(height / root.lineHeight) + 2
            readonly property int firstLine: Math.max(0, Math.floor(flick.contentY / root.lineHeight) - 1)
            // cursorRectangle is in content coordinates, so it identifies the caret line.
            readonly property int caretLine: Math.round(textArea.cursorRectangle.y / root.lineHeight)

            Repeater {
                model: gutter.visibleLineCount

                Text {
                    x: 0
                    width: gutter.width - 10
                    height: root.lineHeight
                    y: (gutter.firstLine + index) * root.lineHeight - flick.contentY
                    text: gutter.firstLine + index < root.documentLineCount
                              ? String(gutter.firstLine + index + 1) : ""
                    color: (gutter.firstLine + index) === gutter.caretLine
                               ? root.gutterActiveTextColor : root.gutterTextColor
                    font.family: "Menlo"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
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
            contentHeight: Math.max(height, root.documentLineCount * root.lineHeight)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalAndVerticalFlick

            // NOTE: the TextArea is a plain child of the Flickable's contentItem, NOT attached
            // via "TextArea.flickable". The attached property pipes the document's ideal width
            // straight into Flickable.contentWidth, so an edit that changes the widest line
            // resizes the TextArea, invalidates every text block's layout, and forces a full
            // document re-shape (~1s on 10k+ lines). With a plain child the width stays pinned
            // to maxLineWidth and edits only relayout the edited blocks.

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
                height: Math.max(flick.height, root.documentLineCount * root.lineHeight)

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

                onTextChanged: {
                    root.dirty = true;
                }

                // Block keyboard editing while a document is loading; Keys runs before the
                // text control sees the event. readOnly is not an option: Qt moves the caret
                // to the document end whenever it is toggled, scrolling the view to bottom.
                Keys.onPressed: function(event) {
                    if (root.busy) {
                        event.accepted = true;
                        return;
                    }
                }

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

    // Blocking overlay while a document is loading: the content is filled chunk by chunk
    // over several seconds and the viewport is restored afterwards, and showing that raw
    // process (lines growing in, view jumping around) reads as glitching. The centered panel
    // mirrors the web player's visual language (dark rounded panel, 4px rounded progress
    // rail with a white fill, 13px white text).
    Rectangle {
        anchors.fill: editorArea
        color: root.backgroundColor
        visible: root.busy

        // Swallow mouse events while loading; without a grabber, clicks and drag-selection
        // fall through to the TextArea underneath (only keyboard input is blocked elsewhere).
        MouseArea {
            anchors.fill: parent
        }

        Rectangle {
            anchors.centerIn: parent
            width: 280
            height: 96
            radius: 8
            color: "#20202A"

            Column {
                anchors.centerIn: parent
                spacing: 16

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.loadProgress > 0
                              ? qsTr("Loading source... %1%").arg(Math.round(root.loadProgress * 100))
                              : qsTr("Loading source...")
                    color: "#FFFFFF"
                    font.pixelSize: 13
                }

                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 200
                    height: 4

                    Rectangle {
                        anchors.fill: parent
                        radius: 2
                        color: Qt.rgba(1, 1, 1, 0.18)
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * root.loadProgress
                        radius: 2
                        color: "#FFFFFF"
                    }
                }
            }
        }
    }

    // Always-visible action bar, mirroring the web editor's button row.
    Rectangle {
        id: buttonBar
        height: 48
        width: root.layoutWidth
        y: root.layoutHeight - height
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
