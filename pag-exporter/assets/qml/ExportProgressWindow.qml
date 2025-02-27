import QtQuick 2.5
import QtQuick.Window 2.13

PApplicationWindow {
    property var windowWidth : 624
    property var windowHeight: 341
    property var borderWidth: 2
    property var marginValue: 24
    property var isPreview: false
    property var bgColor: Qt.rgba(20/255,20/255,30/255,1)
    property var containerColor: Qt.rgba(1, 1, 1, 16/255)
    property var transparentColor: Qt.rgba(1, 1, 1, 0)
    property var whiteTextColor: Qt.rgba(1, 1, 1, 1)

    property var progressCount: progressListModel.rowCount()
    property var progressSuccess: 0
    property var cancelBtnText: qsTr("取消")
    property var progressTip: qsTr("已完成 %1/%2")
    property var completeTip: qsTr("完成")
    

    id: exportProgressWindow
    visible: true
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    windowBackground: bgColor
    titlebarBackground: bgColor
    canResize: false
    titleFontSize: 14
    windowRadius: 2
    isWindows: true
    maxBtnVisible: false
    minBtnVisible: false
    canMaxSize: false
    flags: Qt.FramelessWindowHint | Qt.Window | Qt.WindowSystemMenuHint|Qt.WindowMinimizeButtonHint|Qt.WindowMaximizeButtonHint

    onClosing: function(closeEvent) {
        console.log("ExportProgress window close")
        exportProgressListWindow.exit()
    }

    Rectangle{
        id: progressContainer
        width: parent.width
        height: 225
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top : parent.top
        anchors.topMargin: 1
        color: containerColor
        border.color: bgColor
        border.width: borderWidth
        radius: 2
        clip: true
        
        Text {
            id: progressText
            height: 22
            color: whiteTextColor
            text: progressTip.arg(progressSuccess).arg(progressCount)
            anchors.top: parent.top
            anchors.topMargin: marginValue
            anchors.left: parent.left
            anchors.leftMargin: marginValue
            anchors.right:parent.right
            anchors.rightMargin: marginValue
            font.pixelSize: 16
            clip: true
        }

        Connections {
            target: exportProgressListWindow
            onSuccessProgressCountChanged: {
                progressSuccess = progressListModel.successCount();
            }
        }

        Rectangle {
            id: progressListContainer
            visible: true
            color: transparentColor
            width: 576
            height: 133
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top : progressText.bottom
            anchors.leftMargin: marginValue
            anchors.rightMargin: marginValue
            anchors.topMargin: 22
            clip: true

            ExportProgressListView{
                id: progressList
                anchors.fill: parent
                visible: true
                bgColor: transparentColor
            }
        }
    }

    Rectangle{
        id: cancelBtn
        width: 100
        height: 40
        color: "#1982EB"
        radius: 2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: marginValue
        anchors.rightMargin: marginValue

        Text{
            id: buttonText
            font.pixelSize: 18
            font.family: "PingFang SC"
            anchors.centerIn: parent
            text: cancelBtnText
            font.bold: true
            color: whiteTextColor
        }

        MouseArea{
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            onPressed: {
                exportProgressListWindow.onCancelBtnClick();
            }
        }

        Connections {
            target: exportProgressListWindow
            onAllProgressFinished: {
                cancelBtnText = completeTip;
            }
        }
    }

}
