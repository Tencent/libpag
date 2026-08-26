import PAG
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "components"

SplitView {
    id: splitView
    required property int resizeHandleSize

    property var contentView: contentViewLoader.item
    property bool hasPAGFile: contentView && contentView.viewModel.filePath !== ""
    property bool hasAnimation: contentView && contentView.viewModel.hasAnimation

    property bool isBackgroundOn: false

    property bool isEditPanelOpen: false

    property bool isTextListOpen: true

    property bool isImageListOpen: true

    property int minPlayerWidth: 360

    property int minPanelWidth: 300

    property int splitHandleWidth: 3

    property int splitHandleHeight: splitView.height

    property int controlFormHeight: 76

    // Minimum height reserved for the editing area while the side panel is open, so the
    // profiler never squeezes it to an unusable size when the window is short.
    property int minEditAreaHeight: 120

    property alias contentViewLoader: contentViewLoader

    property alias dropArea: dropArea

    property alias centerItem: centerItem

    property alias rightItemLoader: rightItemLoader

    property alias controlForm: controlForm

    property string currentViewType: initialViewType

    property string pendingFilePath: ""

    // Reset XML editor when switching view types
    onCurrentViewTypeChanged: {
        if (rightItemLoader.item && rightItemLoader.item.xmlSourceEditor) {
            rightItemLoader.item.xmlSourceEditor.reset();
        }
    }

    // Mirror of the web playground's bare-L shortcut that toggles the source editor panel.
    // Disabled while the editor holds focus so typing "l" in the text is never swallowed.
    Shortcut {
        sequence: "L"
        enabled: currentViewType === "pagx" && hasPAGFile &&
                  !(rightItemLoader.item && rightItemLoader.item.xmlSourceEditor &&
                    rightItemLoader.item.xmlSourceEditor.editorFocused)
        onActivated: tabBar.currentIndex = tabBar.currentIndex === 1 ? 0 : 1
    }

    anchors.fill: parent
    orientation: Qt.Horizontal
    handle: Rectangle {
        id: splitHandle
        implicitWidth: splitHandleWidth
        implicitHeight: splitHandleHeight
        color: "#000000"
    }

    function loadFile(filePath) {
        let lowerPath = filePath.toLowerCase();
        let newViewType = lowerPath.endsWith(".pagx") ? "pagx" : "pag";

        if (currentViewType !== newViewType) {
            if (contentView) {
                contentView.prepareForRemoval();
            }
            pendingFilePath = filePath;
            currentViewType = newViewType;
            return true;
        }

        // Same view type - just load the file directly
        if (contentViewLoader.status === Loader.Ready && contentView) {
            return contentView.viewModel.loadFile(filePath);
        }
        // Loader not ready yet, queue the file for loading after initialization
        pendingFilePath = filePath;
        return true;
    }

    PAGRectangle {
        id: centerItem
        clip: true
        SplitView.minimumWidth: minPlayerWidth
        SplitView.fillWidth: true
        color: "#000000"
        radius: 5
        leftTopRadius: false
        rightTopRadius: false
        rightBottomRadius: !controlForm.panelsButton.checked

        Image {
            id: backgroundTiles
            visible: hasPAGFile && !isBackgroundOn
            smooth: false
            source: "qrc:/images/tiles.png"
            fillMode: Image.Tile
            anchors.fill: parent
            anchors.bottom: parent.bottom
            anchors.bottomMargin: controlFormHeight
            sourceSize.width: 32
            sourceSize.height: 32
        }

        Loader {
            id: contentViewLoader
            x: 0
            y: 0
            width: parent.width
            height: splitView.height - controlFormHeight

            sourceComponent: currentViewType === "pagx" ? pagxViewComponent : pagViewComponent

            onLoaded: {
                pagWindow.notifyContentViewChanged(item);

                if (pendingFilePath !== "") {
                    let filePath = pendingFilePath;
                    pendingFilePath = "";
                    Qt.callLater(function () {
                        if (item) {
                            item.viewModel.loadFile(filePath);
                        }
                    });
                }
            }

            Component {
                id: pagViewComponent
                PAGView {
                    objectName: "contentView"
                }
            }

            Component {
                id: pagxViewComponent
                PAGXView {
                    objectName: "contentView"
                }
            }
        }

        ControlForm {
            id: controlForm
            contentView: splitView.contentView
            height: controlFormHeight
            z: 1
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0
        }
        MouseArea {
            id: mouseArea
            z: 2
            anchors.fill: parent
            anchors.leftMargin: resizeHandleSize
            anchors.rightMargin: resizeHandleSize
            anchors.bottom: parent.bottom
            anchors.bottomMargin: controlFormHeight + 9
            acceptedButtons: Qt.LeftButton
            cursorShape: pressed && dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property real pressX: 0
            property real pressY: 0
            property real lastX: 0
            property real lastY: 0
            property bool dragging: false

            onPressed: function (mouse) {
                pressX = mouse.x;
                pressY = mouse.y;
                lastX = mouse.x;
                lastY = mouse.y;
                dragging = false;
            }
            onPositionChanged: function (mouse) {
                if (!contentView || !pressed) {
                    return;
                }
                if (!dragging) {
                    var distance = Math.hypot(mouse.x - pressX, mouse.y - pressY);
                    if (distance < 4) {
                        return;
                    }
                    dragging = true;
                    contentView.panBy(mouse.x - pressX, mouse.y - pressY);
                } else {
                    contentView.panBy(mouse.x - lastX, mouse.y - lastY);
                }
                lastX = mouse.x;
                lastY = mouse.y;
            }
            onReleased: function (mouse) {
                if (!dragging && contentView) {
                    contentView.viewModel.isPlaying = !contentView.viewModel.isPlaying;
                }
                dragging = false;
            }
            onCanceled: dragging = false
            onWheel: function (wheel) {
                if (!contentView) {
                    return;
                }
                if (pinchHandler.active) {
                    wheel.accepted = true;
                    return;
                }
                var pixelDeltaX = wheel.pixelDelta.x !== 0 ? wheel.pixelDelta.x : wheel.angleDelta.x / 4;
                var pixelDeltaY = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y / 4;
                if (wheel.modifiers & Qt.ControlModifier || wheel.modifiers & Qt.MetaModifier) {
                    var anchor = mouseArea.mapToItem(contentView, wheel.x, wheel.y);
                    var rawDelta = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y;
                    var ratio = wheel.pixelDelta.y !== 0 ? 240 : 600;
                    contentView.zoomAt(Math.exp(rawDelta / ratio), anchor.x, anchor.y);
                } else if (wheel.modifiers & Qt.ShiftModifier) {
                    contentView.panBy(pixelDeltaY, 0);
                } else {
                    contentView.panBy(pixelDeltaX, pixelDeltaY);
                }
                wheel.accepted = true;
            }
        }
        // macOS trackpad pinch-to-zoom (two-finger pinch gesture).
        PinchHandler {
            id: pinchHandler
            target: null
            acceptedDevices: PointerDevice.TouchPad | PointerDevice.TouchScreen

            property real lastScale: 1.0

            onScaleChanged: {
                if (!active || !contentView) {
                    return;
                }
                var factor = scale / pinchHandler.lastScale;
                pinchHandler.lastScale = scale;
                if (factor > 0 && isFinite(factor) && factor !== 1) {
                    contentView.zoomAt(factor, centroid.position.x, centroid.position.y);
                }
            }
            onActiveChanged: {
                pinchHandler.lastScale = scale;
            }
        }
        Row {
            id: viewControls
            z: 4
            visible: hasPAGFile && contentView
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.bottom: controlForm.top
            anchors.bottomMargin: 16
            spacing: 8

            Rectangle {
                id: zoomScaleBadge
                width: zoomScaleText.implicitWidth + 24
                height: 32
                radius: 8
                color: "#99000000"

                Text {
                    id: zoomScaleText
                    anchors.centerIn: parent
                    color: "#cccccc"
                    font.pixelSize: 12
                    text: contentView ? Math.round(contentView.viewModel.zoomScale * 100) + "%" : ""
                }
            }

            Button {
                id: resetViewButton
                width: 32
                height: 32
                hoverEnabled: true
                padding: 7
                onClicked: {
                    if (contentView) {
                        contentView.resetView();
                    }
                }

                background: Rectangle {
                    radius: 8
                    color: resetViewButton.pressed ? "#cc000000" : resetViewButton.hovered ? "#b3000000" : "#99000000"
                }

                contentItem: Image {
                    source: "qrc:/images/reset-view.svg"
                    fillMode: Image.PreserveAspectFit
                }

                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reset Zoom")
                ToolTip.delay: 500
            }
        }
        DropArea {
            id: dropArea
            z: 3
            anchors.fill: parent
        }
        Rectangle {
            visible: !hasPAGFile
            color: "#16161d"
            anchors.fill: parent
            anchors.bottomMargin: controlFormHeight

            Text {
                color: "#80ffffff"
                text: qsTr("Click the menu or drag-drop here to open a PAG file")
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                anchors.fill: parent
                font.pixelSize: 20
                wrapMode: Text.WordWrap
            }
        }
    }

    Loader {
        id: rightItemLoader
        active: isEditPanelOpen
        visible: isEditPanelOpen
        SplitView.minimumWidth: minPanelWidth
        SplitView.preferredWidth: minPanelWidth
        sourceComponent: PAGRectangle {
            id: rightItem
            visible: true
            width: parent.width
            height: parent.height
            color: "#16161d"
            radius: 5
            leftTopRadius: false
            rightTopRadius: false
            rightBottomRadius: false

            // Expose xmlSourceEditor for external access
            property alias xmlSourceEditor: xmlSourceEditor

            // Check if Source Editor tab is selected for PAGX
            property bool isSourceEditorActive: currentViewType === "pagx" && tabBar.currentIndex === 1

            Column {
                id: rightColumn
                spacing: 0
                height: parent.height
                width: parent.width
                anchors.fill: parent

                Item {
                    width: parent.width
                    height: 1
                }

                TabBar {
                    id: tabBar

                    height: 38
                    anchors.left: parent.left
                    anchors.leftMargin: 0
                    anchors.right: parent.right
                    anchors.rightMargin: 0

                    background: Rectangle {
                        color: "#16161D"
                    }

                    PAGTabButton {
                        id: editLayerButton
                        text: qsTr("Edit Layer")
                    }

                    PAGTabButton {
                        id: fileStructureButton
                        text: currentViewType === "pagx" ? qsTr("Source Editor") : qsTr("File Structure")
                    }

                    PAGTabButton {
                        id: spaceButton
                        text: ""
                        enabled: false
                    }
                }

                StackLayout {
                    id: tabContents

                    currentIndex: tabBar.currentIndex

                    height: parent.height - tabBar.height - (isSourceEditorActive ? 0 : performance.height)
                    anchors.left: parent.left
                    anchors.leftMargin: 0
                    anchors.right: parent.right
                    anchors.rightMargin: 0

                    /* Layer Editing Area */
                    Rectangle {
                        color: "#20202A"
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ScrollView {
                            id: editArea
                            anchors.fill: parent
                            clip: true

                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            ScrollBar.vertical.background: Rectangle {
                                color: "#00000000"
                            }
                            ScrollBar.vertical.contentItem: Rectangle {
                                implicitWidth: 9
                                implicitHeight: 100
                                color: "#00000000"

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 4
                                    anchors.right: parent.right
                                    anchors.rightMargin: 2
                                    color: "#AA4B4B5A"
                                    visible: editArea.ScrollBar.vertical.size < 1.0
                                }
                            }

                            Column {
                                spacing: 0
                                width: editArea.width

                                Rectangle {
                                    width: parent.width
                                    height: editArea.height
                                    visible: !textListContainer.visible && !imageListContainer.visible
                                    color: "#20202A"

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 8

                                        Text {
                                            color: "#80ffffff"
                                            text: currentViewType === "pagx" ? qsTr("PAGX files do not support layer editing") : qsTr("No layer was editable")
                                            font.pixelSize: 12
                                            anchors.horizontalCenter: parent.horizontalCenter
                                        }

                                        Text {
                                            visible: currentViewType === "pagx"
                                            color: "#448EF9"
                                            text: qsTr("Go to Source Editor →")
                                            font.pixelSize: 12
                                            font.underline: linkMouseArea.containsMouse
                                            anchors.horizontalCenter: parent.horizontalCenter

                                            MouseArea {
                                                id: linkMouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    tabBar.currentIndex = 1;
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    id: textListContainer
                                    width: parent.width
                                    height: isTextListOpen ? ((contentView ? contentView.viewModel.editableTextLayerCount : 0) * 40 + 44) : 32
                                    visible: contentView && contentView.viewModel.editableTextLayerCount > 0
                                    color: "#20202A"

                                    Row {
                                        id: textListTitle
                                        spacing: 0
                                        width: parent.width
                                        height: 21
                                        anchors.top: parent.top
                                        anchors.topMargin: 5

                                        Item {
                                            width: 5
                                            height: 1
                                        }

                                        CheckBox {
                                            id: textListCheckBox
                                            width: 20
                                            height: 21
                                            anchors.top: parent.top
                                            checked: isTextListOpen
                                            rotation: isTextListOpen ? 0 : -90

                                            indicator: Image {
                                                width: parent.width
                                                height: parent.height
                                                source: "qrc:/images/icon-collapse.png"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onPressed: function (mouse) {
                                                        mouse.accepted = false;
                                                    }
                                                }
                                            }

                                            onClicked: {
                                                splitView.isTextListOpen = !splitView.isTextListOpen;
                                            }
                                        }

                                        Item {
                                            width: 5
                                            height: 1
                                        }

                                        Text {
                                            id: textListTitleText
                                            height: 20
                                            anchors.top: parent.top
                                            text: qsTr("Edit Text")
                                            font.pixelSize: 12
                                            renderType: Text.NativeRendering
                                            color: "#9B9B9B"
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    TextListView {
                                        id: textListView
                                        height: (contentView ? contentView.viewModel.editableTextLayerCount : 0) * 40
                                        textHeight: 40
                                        textModel: textLayerModel
                                        visible: isTextListOpen && height > 0
                                        anchors.top: textListTitle.bottom
                                        anchors.topMargin: 5
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 10
                                        anchors.left: parent.left
                                        anchors.leftMargin: 15
                                        anchors.right: parent.right
                                        anchors.rightMargin: 15
                                        clip: true
                                    }
                                }

                                Rectangle {
                                    id: imageListContainer
                                    width: parent.width
                                    height: isImageListOpen ? ((contentView ? contentView.viewModel.editableImageLayerCount : 0) * 60 + 44) : 32
                                    visible: contentView && contentView.viewModel.editableImageLayerCount > 0
                                    color: "#20202A"

                                    Row {
                                        id: imageListTitle
                                        spacing: 0
                                        width: parent.width
                                        height: 21
                                        anchors.top: parent.top
                                        anchors.topMargin: 5

                                        Item {
                                            width: 5
                                            height: 1
                                        }

                                        CheckBox {
                                            id: imageListCheckBox
                                            width: 20
                                            height: 21
                                            anchors.top: parent.top
                                            checked: isImageListOpen
                                            rotation: isImageListOpen ? 0 : -90

                                            indicator: Image {
                                                width: parent.width
                                                height: parent.height
                                                source: "qrc:/images/icon-collapse.png"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onPressed: function (mouse) {
                                                        mouse.accepted = false;
                                                    }
                                                }
                                            }

                                            onClicked: {
                                                splitView.isImageListOpen = !splitView.isImageListOpen;
                                            }
                                        }

                                        Item {
                                            width: 5
                                            height: 1
                                        }

                                        Text {
                                            id: imageListTitleText
                                            height: 20
                                            anchors.top: parent.top
                                            text: qsTr("Edit Image")
                                            font.pixelSize: 12
                                            renderType: Text.NativeRendering
                                            color: "#9B9B9B"
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    ImageListView {
                                        id: imageListView
                                        height: (contentView ? contentView.viewModel.editableImageLayerCount : 0) * 60
                                        imageHeight: 60
                                        imageModel: imageLayerModel
                                        visible: isImageListOpen && height > 0
                                        anchors.top: imageListTitle.bottom
                                        anchors.topMargin: 5
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 10
                                        anchors.left: parent.left
                                        anchors.leftMargin: 15
                                        anchors.right: parent.right
                                        anchors.rightMargin: 15
                                        clip: true
                                    }
                                }
                            }
                        }
                    }

                    /* File Structure / Source Editor Area */
                    Rectangle {
                        color: currentViewType === "pagx" ? "#1E1E1E" : "#20202A"
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        // File Structure TreeView (for PAG files)
                        ScrollView {
                            anchors.fill: parent
                            clip: true
                            visible: currentViewType !== "pagx"

                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            ScrollBar.vertical.background: Rectangle {
                                color: "#00000000"
                            }
                            ScrollBar.vertical.contentItem: Rectangle {
                                implicitWidth: 9
                                implicitHeight: 100
                                color: "#00000000"

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 4
                                    anchors.right: parent.right
                                    anchors.rightMargin: 2
                                    color: "#AA4B4B5A"
                                }
                            }

                            TreeView {
                                id: fileTreeView

                                property int myCurrentRow: -1

                                anchors.fill: parent
                                model: treeViewModel
                                delegate: FileTreeViewDelegate {
                                    treeView: fileTreeView
                                }
                            }
                        }

                        // XML Source Editor (for PAGX files)
                        XMLSourceEditor {
                            id: xmlSourceEditor
                            width: parent.width
                            height: parent.height
                            visible: currentViewType === "pagx"
                            isActive: rightItem.isSourceEditorActive
                            viewModel: contentView ? contentView.viewModel : null
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: isSourceEditorActive ? 0 : 1
                    visible: !isSourceEditorActive
                }

                PAGRectangle {
                    id: performance
                    color: "#16161D"
                    // Reserve minEditAreaHeight for the editing area above, so the profiler never
                    // squeezes it to an unusable size when the window is short. The profiler
                    // scrolls internally, so no data is lost.
                    height: isSourceEditorActive ? 0 : Math.min(profilerForm.contentHeight,
                                                                Math.max(0, parent.height - tabBar.height - 40 - minEditAreaHeight))
                    visible: !isSourceEditorActive
                    clip: true
                    anchors.right: parent.right
                    anchors.rightMargin: 0
                    anchors.left: parent.left
                    anchors.leftMargin: 0
                    radius: 5
                    leftTopRadius: false
                    rightTopRadius: false
                    leftBottomRadius: false

                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        contentHeight: profilerForm.contentHeight
                        contentWidth: width

                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical.policy: profilerForm.contentHeight > performance.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                        ScrollBar.vertical.background: Rectangle {
                            color: "#00000000"
                        }
                        ScrollBar.vertical.contentItem: Rectangle {
                            implicitWidth: 9
                            implicitHeight: 100
                            color: "#00000000"

                            Rectangle {
                                anchors.fill: parent
                                radius: 4
                                anchors.right: parent.right
                                anchors.rightMargin: 2
                                color: "#AA4B4B5A"
                            }
                        }

                        Profiler {
                            id: profilerForm
                            width: performance.width
                            height: contentHeight
                            contentView: splitView.contentView
                        }
                    }
                }
            }
        }
    }
}
