import QtQuick
import QtQuick.Controls

PAGWindow {
    id: window

    property int windowWidth: 600

    property int windowHeight: 300

    property string displayTitle: qsTr("Creating PAG")

    title: displayTitle
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    visible: true
    isWindows: true
    canResize: true
    canMaxSize: false
    maxBtnVisible: false
    minBtnVisible: false
    resizeHandleSize: 5
    titleBarHeight: 32
    windowBackgroundColor: "#14141E"
    titlebarBackgroundColor: "#14141E"
    titleFontSize: 14

    Rectangle {
        id: contentContainer
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.bottom: buttonContainer.top
        anchors.bottomMargin: 15
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "#22222c"
        clip: true
        radius: 1

        Rectangle {
            id: tipContainer
            height: 30
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            color: "transparent"

            Text {
                id: tipText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                text: qsTr("%1/%2 Complete").arg(progressListModel.exportNum).arg(progressListModel.totalExportNum)
                font.pixelSize: 14
                font.family: "PingFang SC"
                color: "#FFFFFF"
            }
        }

        Rectangle {
            id: divider
            height: 1
            width: parent.width
            anchors.top: tipContainer.bottom
            color: "#14141E"
        }

        Rectangle {
            id: progressListViewContainer
            anchors.top: divider.bottom
            anchors.bottom: contentContainer.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: "transparent"

            ProgressListView {
                id: progressListView
                anchors.fill: parent
                model: progressListModel
            }
        }
    }

    Rectangle {
        id: buttonContainer
        height: 40
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "transparent"

        Rectangle {
            id: finishButton

            width: 100
            height: parent.height
            color: "#1982EB"
            radius: 2
            anchors.right: parent.right
            opacity: enabled ? 1.0 : 0.3

            Text {
                text: qsTr("Finish")
                font.pixelSize: 16
                font.bold: true
                font.family: "PingFang SC"
                anchors.centerIn: parent
                color: "#FFFFFF"
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    window.close();
                }
            }
        }
    }

    onClosing: function (closeEvent) {
        closeEvent.accepted = true;
        if (exportCompositionsWindow !== null) {
            exportCompositionsWindow.onWindowClosing();
        }
    }
}
