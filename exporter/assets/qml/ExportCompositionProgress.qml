import QtQuick
import QtQuick.Controls.Universal

PAGWindow {
    id: window

    property int windowWidth: 400

    property int windowHeight: 128

    property string displayTitle: qsTr("PAG Export Progress")

    property string displayTip: qsTr("Exporting PAG")

    title: displayTitle
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    maximumWidth: windowWidth
    maximumHeight: windowHeight
    visible: true
    isWindows: true
    canResize: false
    canMaxSize: false
    maxBtnVisible: false
    minBtnVisible: false
    resizeHandleSize: 5
    titleBarHeight: 32
    windowBackgroundColor: "#14141E"
    titlebarBackgroundColor: "#14141E"
    titleFontSize: 14

    Rectangle {
        id: progressContainer
        radius: 2
        anchors.fill: parent
        color: "transparent"

        ProgressBar {
            id: progressBar
            anchors.top: parent.top
            anchors.topMargin: 27
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 12
            to: progressModel ? progressModel.totalProgress : 1.0
            value: progressModel ? progressModel.currentProgress : 0.0
            height: 8

            background: Rectangle {
                visible: progressBar.visualPosition < 1.0
                color: "#2B2B33"
                border.width: 0
                radius: progressBar.height / 2
                implicitWidth: progressBar.width
                implicitHeight: progressBar.height
            }

            contentItem: Item {
                Rectangle {
                    visible: progressBar.visualPosition !== 0
                    width: progressBar.visualPosition * progressBar.width
                    height: progressBar.height
                    radius: progressBar.height / 2
                    color: "#4082E4"
                }
            }
        }

        Text {
            id: tipText
            text: window.displayTip
            font.pixelSize: 14
            font.family: "PingFang SC"
            font.bold: true
            color: "#FFFFFF"
            renderType: Text.NativeRendering
            anchors.top: progressBar.bottom
            anchors.topMargin: 16
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    onClosing: function (closeEvent) {
        closeEvent.accepted = true;
        if (exportWindow !== null) {
            exportWindow.onWindowClosing();
        }
    }

    Connections {
        target: progressModel || null
        function onExportFinished() {
            window.close();
        }

        function onExportFailed() {
            window.close();
        }
    }
}
