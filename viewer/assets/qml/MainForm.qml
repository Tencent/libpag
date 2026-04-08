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

    property alias contentViewLoader: contentViewLoader

    property alias dropArea: dropArea

    property alias centerItem: centerItem

    property alias rightItemLoader: rightItemLoader

    property alias controlForm: controlForm

    property string currentViewType: "pag"

    property string pendingFilePath: ""

    // Reset XML editor when switching view types
    onCurrentViewTypeChanged: {
        if (rightItemLoader.item && rightItemLoader.item.xmlSourceEditor) {
            rightItemLoader.item.xmlSourceEditor.reset("");
        }
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
            onClicked: {
                if (contentView) {
                    contentView.viewModel.isPlaying = !contentView.viewModel.isPlaying;
                }
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
                        Column {
                            id: xmlEditorContainer
                            anchors.fill: parent
                            visible: currentViewType === "pagx"
                            spacing: 0

                            // Monitor lineCount changes for file switching detection
                            Connections {
                                target: contentView && contentView.viewModel.linesModel ? contentView.viewModel.linesModel : null
                                function onLineCountChanged() {
                                    // lineCount change indicates a new file was loaded
                                    // Reset editor to show new content from the beginning
                                    if (!xmlSourceEditor.editMode) {
                                        xmlSourceEditor.reset("");
                                    }
                                }
                            }

                            XMLSourceEditor {
                                id: xmlSourceEditor
                                width: parent.width
                                height: parent.height - (xmlSourceEditor.editMode ? xmlButtonBar.height : 0)
                                linesModel: contentView && contentView.viewModel.linesModel ? contentView.viewModel.linesModel : null

                                // Initialize when component is created
                                Component.onCompleted: {
                                    if (linesModel && linesModel.lineCount > 0) {
                                        reset("");
                                    }
                                }

                                onContentEdited: function(newText) {
                                    // Content is being edited - handled in apply
                                }
                            }

                            // Button bar for edit mode
                            Rectangle {
                                id: xmlButtonBar
                                width: parent.width
                                height: xmlSourceEditor.editMode ? 48 : 0
                                color: "#16161D"
                                visible: height > 0

                                Behavior on height {
                                    NumberAnimation { duration: 150 }
                                }

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 12

                                    Button {
                                        id: discardButton
                                        text: qsTr("Discard")
                                        scale: hovered ? 1.05 : 1.0

                                        Behavior on scale {
                                            NumberAnimation { duration: 100 }
                                        }

                                        background: Rectangle {
                                            implicitWidth: 80
                                            implicitHeight: 32
                                            color: discardButton.hovered ? "#5C5C6A" : "#3C3C3C"
                                            border.color: discardButton.hovered ? "#8B8B9A" : "#4B4B5A"
                                            border.width: 1
                                            radius: 4
                                        }

                                        contentItem: Text {
                                            text: discardButton.text
                                            color: "#FFFFFF"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            font.pixelSize: 12
                                        }

                                        onClicked: {
                                            // Reset to original content - reload from file would be needed
                                            // For now, just exit edit mode (content was already committed line by line)
                                            xmlSourceEditor.exitEditMode(false);
                                            xmlSourceEditor.showToast(qsTr("Changes discarded"), true);
                                        }
                                    }

                                    Button {
                                        id: applyButton
                                        text: qsTr("Apply")
                                        scale: hovered ? 1.05 : 1.0

                                        Behavior on scale {
                                            NumberAnimation { duration: 100 }
                                        }

                                        background: Rectangle {
                                            implicitWidth: 80
                                            implicitHeight: 32
                                            color: applyButton.hovered ? "#5BA3FF" : "#448EF9"
                                            border.color: applyButton.hovered ? "#8BC4FF" : "#5BA3FF"
                                            border.width: 1
                                            radius: 4
                                        }

                                        contentItem: Text {
                                            text: applyButton.text
                                            color: "#FFFFFF"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            font.pixelSize: 12
                                        }

                                        onClicked: {
                                            if (!contentView || !contentView.viewModel)
                                                return;
                                            let newXml = xmlSourceEditor.getText();
                                            let error = contentView.viewModel.applyXmlChanges(newXml);
                                            if (error === "") {
                                                xmlSourceEditor.exitEditMode(false);
                                                xmlSourceEditor.showToast(qsTr("Changes applied"), true);
                                            } else {
                                                xmlSourceEditor.showToast(error, false);
                                            }
                                        }
                                    }

                                    Button {
                                        id: saveButton
                                        text: qsTr("Save")
                                        scale: hovered ? 1.05 : 1.0

                                        Behavior on scale {
                                            NumberAnimation { duration: 100 }
                                        }

                                        background: Rectangle {
                                            implicitWidth: 80
                                            implicitHeight: 32
                                            color: saveButton.hovered ? "#4CAF50" : "#388E3C"
                                            border.color: saveButton.hovered ? "#81C784" : "#4CAF50"
                                            border.width: 1
                                            radius: 4
                                        }

                                        contentItem: Text {
                                            text: saveButton.text
                                            color: "#FFFFFF"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            font.pixelSize: 12
                                        }

                                        onClicked: {
                                            let newXml = xmlSourceEditor.getText();
                                            // First apply changes to preview
                                            let applyError = contentView.viewModel.applyXmlChanges(newXml);
                                            if (applyError === "") {
                                                // Then save to file
                                                let saveError = contentView.viewModel.saveXmlToFile(newXml);
                                                xmlSourceEditor.exitEditMode(false);
                                                if (saveError === "") {
                                                    xmlSourceEditor.showToast(qsTr("File saved"), true);
                                                } else {
                                                    xmlSourceEditor.showToast(saveError, false);
                                                }
                                            } else {
                                                xmlSourceEditor.showToast(applyError, false);
                                            }
                                        }
                                    }
                                }
                            }
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
                    height: isSourceEditorActive ? 0 : Math.min(profilerForm.contentHeight, parent.height - tabBar.height - 40)
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
