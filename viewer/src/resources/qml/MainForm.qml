import PAG
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls

PSplitView {
    id: splitView

    property bool hasPAGFile: false
    property bool textListOpen: true
    property bool imageListOpen: true
    property bool isBackgroundOn: false
    property bool isEditPanelOpen: false
    property int playerMinWidth: 360
    property int playerDefaultMinWidth: 360
    property int panelDefaultWidth: 300
    property int splitHandleWidth: 0

    property alias popup: popup
    property alias dropArea: dropArea
    property alias rightItem: rightItem
    property alias centerItem: centerItem
    property alias pagViewer: pagViewer
    // TODO
    // property alias taskFactory: taskFactory
    property alias controlForm: controlForm
    property alias performance: performance
    property alias profilerForm: profilerForm
    property alias textListView: textListView
    property alias imageListView: imageListView
    property alias pagFileTreeView: pagFileTreeView
    property alias backgroundTiles: backgroundTiles
    property alias btnToggleTextList: btnToggleTextList
    property alias btnToggleImageList: btnToggleImageList
    property alias imageListContainer: imageListContainer
    property alias textListContainer: textListContainer

    anchors.fill: parent
    orientation: Qt.Horizontal

    handleDelegate: Rectangle {
        id: splitHandle

        implicitWidth: splitHandleWidth
        implicitHeight: splitHandleWidth
        color: "#000000"
    }

    /* Main Area */
    RectangleWithRadius {
        id: centerItem

        width: 600
        height: 600
        radius: 5
        leftTop: false
        rightTop: false
        Layout.minimumWidth: playerMinWidth
        Layout.fillWidth: true
        color: "#000000"
        rightBottom: !controlForm.btnPanels.checked

        Image {
            id: backgroundTiles

            visible: hasPAGFile && !isBackgroundOn
            smooth: false
            source: "qrc:/image/tiles.png"
            fillMode: Image.Tile
            anchors.fill: parent
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 76
            sourceSize.width: 32
            sourceSize.height: 32
        }

        /* Control Area */
        ControlForm {
            id: controlForm

            height: 76
            z: 1
            progress: 0.0
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
        }

        /* Display Area */
        PAGViewWindow {
            id: pagViewer

            x: 0
            y: 0
            width: 300
            height: 300
            objectName: "pagViewer"
            progress: controlForm.progress
        }

        /* Mouse Drag Area */
        DropArea {
            id: dropArea

            z: 3
            anchors.fill: parent
        }

        /* Mouse Click Area */
        MouseArea {
            id: mouseArea

            z: 2
            anchors.fill: parent
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 85

            onClicked: {
                pagViewer.isPlaying = !pagViewer.isPlaying
            }
        }

        Rectangle {
            visible: !hasPAGFile
            color: "#16161d"
            anchors.bottomMargin: 76
            anchors.fill: parent

            Text {
                color: "#80ffffff"
                text: qsTr("点击菜单，或拖放到这里打开一个 PAG 文件")
                font.pixelSize: 20
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                anchors.fill: parent
                wrapMode: Text.WordWrap
            }
        }

        /* Progress Bar */
        Loader {
            id: popup

            visible: false
            active: false

            sourceComponent: PApplicationWindow {
                property alias popProgress: popProgress
                property var task

                width: 300
                height: 64
                visible: true

                ProgressBar {
                    id: popProgress

                    width: parent.width - 24
                    height: 10
                    value: 0
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenter: parent.horizontalCenter

                    contentItem: Rectangle {
                        color: "#448EF9"
                        width: popProgress.visualPosition * parent.width
                    }

                    background: Rectangle {
                        color: "#DDDDDD"
                    }
                }
            }
        }

        // TODO
        // FileTaskFactory {
        //     id: taskFactory
        //
        //     objectName: "taskFactory"
        // }
    }

    /* The Right Area */
    RectangleWithRadius {
        id: rightItem

        x: 600
        width: 300
        height: 600
        radius: 5
        color: "#16161D"
        visible: isEditPanelOpen
        implicitWidth: panelDefaultWidth
        Layout.minimumWidth: panelDefaultWidth

        leftTop: false
        rightTop: false
        leftBottom: false

        TabBar {
            id: tabBar

            height: 38
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.top: parent.top
            anchors.topMargin: 1

            background: Rectangle {
                color: "#16161D"
            }

            TabButton {
                id: tabButton

                height: 36
                text: qsTr("图层编辑")
                font.pixelSize: 13
                anchors.top: parent.top
                anchors.topMargin: 0

                background: Rectangle {
                    color: tabButton.checked ? "#2D2D37" : "#20202A"
                }

                contentItem: Text {
                    text: tabButton.text
                    font: tabButton.font
                    color: tabButton.checked ? "#FFFFFF" : "#9B9B9B"
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            TabButton {
                id: tab2Button

                height: 36
                text: qsTr("文件结构")
                font.pixelSize: 13
                anchors.top: parent.top
                anchors.topMargin: 0

                background: Rectangle {
                    color: tab2Button.checked ? "#2D2D37" : "#20202A"
                }

                contentItem: Text {
                    text: tab2Button.text
                    font: tab2Button.font
                    color: tab2Button.checked ? "#FFFFFF" : "#9B9B9B"
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            TabButton {
                id: tab3Button

                height: 36
                text: qsTr("")
                font.pixelSize: 14
                font.bold: true
                enabled: false
                anchors.top: parent.top
                anchors.topMargin: 0

                background: Rectangle {
                    color: tab3Button.checked ? "#2D2D37" : "#20202A"
                }

                contentItem: Text {
                    text: tab3Button.text
                    font: tab3Button.font
                    color: tab3Button.checked ? "#FFFFFF" : "#9B9B9B"
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }

        StackLayout {
            id: tabContents

            currentIndex: tabBar.currentIndex

            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.top: parent.top
            anchors.topMargin: 38
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 277+25+1

            /* Layer Editing Area */
            Rectangle {
                color: "#20202A"
                anchors.fill: tabContents.alignment

                ScrollView {
                    id: editArea

                    clip: true
                    anchors.fill: parent

                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ScrollBar.vertical.background: Rectangle {
                        color: "#20202A"
                    }
                    ScrollBar.vertical.contentItem: Rectangle {
                        implicitWidth: 9
                        implicitHeight: 100
                        color: "#20202A"

                        Rectangle {
                            radius: 4
                            anchors.fill: parent
                            anchors.right: parent.right
                            anchors.rightMargin: 2
                            color: parent.parent.size === 1 ? "#00000000" : "#2D2D37"
                        }
                    }

                    Column {
                        spacing: 1
                        width: rightItem.width

                        Rectangle {
                            visible: !(textListContainer.visible || imageListContainer.visible)
                            color: "#20202A"
                            width: rightItem.width
                            height: editArea.height

                            Text {
                                color: "#80ffffff"
                                text: qsTr("没有可以编辑的图层")
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter
                                anchors.fill: parent
                            }
                        }

                        Rectangle {
                            id: textListContainer

                            width: parent.width
                            height: textListOpen ? (pagViewer.textCount * 40 + 44) : 32
                            color: "#20202A"
                            anchors.right: parent.right
                            anchors.rightMargin: 0
                            anchors.left: parent.left
                            anchors.leftMargin: 0

                            CheckBox {
                                id: btnToggleTextList

                                width: 20
                                height: 21
                                rotation: textListOpen ? 0 : -90
                                anchors.topMargin: 6
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.leftMargin: 5
                                checked: textListOpen

                                indicator: Image {
                                    width: 20
                                    height: 21
                                    source: "qrc:/image/icon-collapse.png"

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onPressed: function(mouse) {
                                            mouse.accepted = false
                                        }
                                    }
                                }
                            }

                            Text {
                                id: titleText

                                height: 20
                                text: qsTr("文本编辑")
                                font.pixelSize: 12
                                renderType: Text.NativeRendering
                                verticalAlignment: Text.AlignVCenter
                                color: "#9B9B9B"
                                anchors.top: parent.top
                                anchors.topMargin: 6
                                anchors.right: parent.right
                                anchors.rightMargin: 62
                                anchors.left: parent.left
                                anchors.leftMargin: 30
                            }

                            TextListView {
                                id: textListView

                                height: pagViewer.textCount * 40
                                visible: textListOpen && height > 0
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.topMargin: 30
                                anchors.leftMargin: 14
                                anchors.rightMargin: 0
                                anchors.bottomMargin: 14
                                clip: true
                            }
                        }

                        Rectangle {
                            id: imageListContainer

                            width: parent.width
                            height: imageListOpen ? (pagViewer.imageCount * 60 + 44) : 32
                            color: "#20202A"
                            anchors.right: parent.right
                            anchors.rightMargin: 0
                            anchors.left: parent.left
                            anchors.leftMargin: 0
                            Layout.fillWidth: true

                            CheckBox {
                                id: btnToggleImageList

                                width: 20
                                height: 21
                                rotation: btnToggleImageList.checked ? 0 : -90
                                anchors.top: parent.top
                                anchors.topMargin: 6
                                anchors.left: parent.left
                                anchors.leftMargin: 5
                                checked: imageListOpen

                                indicator: Image {
                                    source: "qrc:/image/icon-collapse.png"
                                    width: 20
                                    height: 21

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        onPressed: function(mouse) {
                                            mouse.accepted = false
                                        }
                                    }
                                }
                            }

                            Text {
                                id: titleImage

                                height: 20
                                text: qsTr("图片编辑")
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                anchors.top: parent.top
                                anchors.topMargin: 6
                                anchors.right: parent.right
                                anchors.left: parent.left
                                anchors.leftMargin: 30
                                color: "#9B9B9B"
                            }

                            ImageListView {
                                id: imageListView
                                height: pagViewer.imageCount * 60
                                visible: imageListOpen && height > 0
                                anchors.top: parent.top
                                anchors.topMargin: 30
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 14
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.right: parent.right
                                anchors.rightMargin: 0
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
                        id: pagFileTreeView

                        property int myCurrentRow: -1

                        anchors.fill: parent
                        model: pagTreeViewModel
                        delegate: PTreeViewDelegate {
                        }
                    }
                }
            }
        }

        /* Performance Display Area */
        RectangleWithRadius {
            id: performance

            color: "#16161D"
            clip: true
            anchors.top: tabContents.bottom
            anchors.topMargin: 0
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0
            radius: 5
            leftTop: false
            rightTop: false
            leftBottom: false

            Profiler {
                id: profilerForm

                anchors.fill: parent
            }
        }
    }

}
