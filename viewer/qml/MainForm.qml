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

    property int minPlayerWidth: 360

    property int minPanelWidth: 300

    property int splitHandleWidth: 0

    property int splitHandleHeight: 0

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
            height: splitView.height - controlFormHeight
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
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
        width: minPanelWidth
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
                width: parent.height
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

                    Rectangle {
                        color: "#20202A"
                        anchors.fill: tabContents.alignment
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
