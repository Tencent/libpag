import QtQuick 2.13
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.13
import QtQml.Models 2.13
import PAG 1.0

//import QtQuick.Dialogs 1.3
PSplitView {
    id: splitView
    property bool hasPAGFile: false
    property bool isEditPanelOpen: false
    property bool isBackgroundOn: false
    property int playerDefaultMinWidth: 342
    property int playerMinWidth: 342
    property int panelDefaultWidth: 840
    property int splitHandleWidth: 0
    property bool imageListOpen: true
    property bool textListOpen: true

    property alias pagViewer: pagViewer
    property alias controlForm: controlForm
    property alias rightItem: rightItem
    property alias centerItem: centerItem
    property alias rightErrorPanel: rightErrorPanel

    anchors.fill: parent
    orientation: Qt.Horizontal

    handleDelegate: Rectangle {
        id: splitHandle
        implicitWidth: splitHandleWidth
        implicitHeight: splitHandleWidth
        color: "#000000"
    }

    RectangleWithRadius {
        id: centerItem
        width: 342
        height: parent.height
        // SplitView.fillWidth: true
        // SplitView.minimumWidth: playerMinWidth
        Layout.minimumWidth: playerMinWidth
        Layout.fillWidth: true
        color: "#000000"
        radius: 5
        leftTop: false
        rightTop: false
        rightBottom: !controlForm.btnPanels.checked

        Image {
            id: backgroundTiles
            visible: hasPAGFile && !isBackgroundOn
            smooth: false
            source: "qrc:/images/tiles.png"
            fillMode: Image.Tile
            anchors.fill: parent
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 62
            sourceSize.width: 32
            sourceSize.height: 32
        }

        ControlForm {
            id: controlForm
            height: 62
            z: 1
            progress: 0.0
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
        }

        PAGQuickItem {
            id: pagViewer
            x: 0
            y: 0
            width: parent.width
            height: parent.height
            objectName: "pagViewer"
            progress: controlForm.progress
        }

        DropArea {
            id: dropArea
            anchors.fill: parent
            z: 3
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            z: 2
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 85
            onClicked: pagViewer.isPlaying = !pagViewer.isPlaying
        }

        Rectangle {
            visible: !hasPAGFile
            color: "#16161d"
            anchors.bottomMargin: 62
            anchors.fill: parent
            Text {
                color: "#80ffffff"
                text: qsTr("点击菜单，或拖放到这里打开一个 PAG 文件")
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                anchors.fill: parent
                font.pixelSize: 20
            }
        }

        Loader {
            id: popup
            visible: false
            active: false
            sourceComponent: PApplicationWindow {
                property alias popProgress: popProgress
                property var task
                width: parent.width
                height: 64
                visible: true
                ProgressBar {
                    id: popProgress
                    width: parent.width - 24
                    height: 10
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    value: 0
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
    }

    // 错误列表
     RectangleWithRadius {
        id: rightErrorPanel
        visible: errorListModel.hasErrorData()
        x: 343
        width: 497
        height: parent.height
        implicitWidth: 496
        Layout.minimumWidth: 496
        radius: 5
        leftTop: false
        rightTop: false
        leftBottom: false

        ErrorListView{
            id:errorView
            width:parent.width
            height:parent.height
            anchors.fill: parent
        }
    }

    RectangleWithRadius {
        id: rightItem
        visible: isEditPanelOpen
        x: 600
        width: 300
        height: 600
        color: "#16161D"
        implicitWidth: panelDefaultWidth
        // SplitView.minimumWidth: 250
        Layout.minimumWidth: panelDefaultWidth
        radius: 5
        leftTop: false
        rightTop: false
        leftBottom: false
    }

}




/*##^## Designer {
    D{i:0;autoSize:true;height:600;width:800}D{i:41;anchors_height:200;anchors_width:200}
}
 ##^##*/
