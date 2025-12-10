import PAG
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "components"

SplitView {
    id: splitView
    required property int resizeHandleSize
    property bool hasPAGFile: pagView.filePath !== ""

    property bool isBackgroundOn: false

    property bool isEditPanelOpen: false

    property bool isTextListOpen: true

    property bool isImageListOpen: true

    property int minPlayerWidth: 360

    property int minPanelWidth: 300

    property int splitHandleWidth: 3

    property int splitHandleHeight: splitView.height

    property int controlFormHeight: 76

    property alias pagView: pagView

    property alias dropArea: dropArea

    property alias centerItem: centerItem

    property alias rightItemLoader: rightItemLoader

    property alias controlForm: controlForm
    anchors.fill: parent
    orientation: Qt.Horizontal
    handle: Rectangle {
        id: splitHandle
        implicitWidth: splitHandleWidth
        implicitHeight: splitHandleHeight
        color: "#000000"
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
        PAGView {
            id: pagView
            x: 0
            y: 0
            width: parent.width
            height: splitView.height - controlFormHeight
            objectName: "pagView"
        }
        ControlForm {
            id: controlForm
            pagView: pagView
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
                pagView.isPlaying = !pagView.isPlaying;
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

            Column {
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
                        text: qsTr("File Structure")
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

                    height: parent.height - tabBar.height - performance.height
                    anchors.left: parent.left
                    anchors.leftMargin: 0
                    anchors.right: parent.right
                    anchors.rightMargin: 0

                    /* Layer Editing Area */
                    Rectangle {
                        color: "#20202A"
                        anchors.fill: tabContents.alignment

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
                                    Text {
                                        color: "#80ffffff"
                                        text: qsTr("No layer was editable")
                                        font.pixelSize: 12
                                        verticalAlignment: Text.AlignVCenter
                                        horizontalAlignment: Text.AlignHCenter
                                        anchors.fill: parent
                                    }
                                }

                                Rectangle {
                                    id: textListContainer
                                    width: parent.width
                                    height: isTextListOpen ? (pagView.editableTextLayerCount * 40 + 44) : 32
                                    visible: pagView.editableTextLayerCount > 0
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
                                        height: pagView.editableTextLayerCount * 40
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
                                    height: isImageListOpen ? (pagView.editableImageLayerCount * 60 + 44) : 32
                                    visible: pagView.editableImageLayerCount > 0
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
                                        height: pagView.editableImageLayerCount * 60
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

                    /* File Structure Area */
                    Rectangle {
                        color: "#20202A"
                        anchors.fill: tabContents.alignment

                        ScrollView {
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
                    }
                }

                Item {
                    width: parent.width
                    height: 1
                }

                PAGRectangle {
                    id: performance
                    color: "#16161D"
                    height: profilerForm.defaultHeight
                    clip: true
                    anchors.right: parent.right
                    anchors.rightMargin: 0
                    anchors.left: parent.left
                    anchors.leftMargin: 0
                    radius: 5
                    leftTopRadius: false
                    rightTopRadius: false
                    leftBottomRadius: false

                    Profiler {
                        id: profilerForm
                        anchors.fill: parent
                    }
                }
            }
        }
    }
}
