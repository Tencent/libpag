import QtQuick 2.5
import QtQuick.Controls 2.1
import QtQuick.Window 2.13
import QtQuick.Layouts 1.3

PApplicationWindow {
    property var isPreview: false
    property var windowWidth : 421
    property var windowHeight: 126
    property var borderWidth: 1
    property var windowMarginRight: 12
    property var barHeight: 8

    property var windowColor: Qt.rgba(20/255,20/255,30/255,1)
    property var conentColor: "#22222B"

    id: wind
    visible: true
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight

    windowBackground: Qt.rgba(20/255,20/255,30/255,1)
    titlebarBackground: windowBackground
    canResize: false
    isWindows: true
    titleFontSize: 14
    maxBtnVisible: false
    minBtnVisible: false
    windowBorderWidth: 1

    onClosing: function(closeEvent) {
        console.log("ExportSettings window close")
        progressDialog.windowClose()
    }

    Rectangle{
        id: rootContainer
        anchors.fill: parent
        color: windowColor

        ProgressBar{
            id: progressBar
            anchors.top: parent.top
            anchors.topMargin: 27
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: windowMarginRight
            anchors.rightMargin: windowMarginRight
            from: 0.0
            to: 1.0
            value: 0.0
            height: barHeight

            background: Rectangle{
                radius: barHeight / 2
                color: "#2B2B33"
                border.width: 0
                implicitWidth: progressBar.width
                implicitHeight: progressBar.height
            }

            contentItem: Rectangle{
                visible: progressBar.visualPosition != 0
                width: progressBar.visualPosition * progressBar.width
                height: progressBar.height
                radius: barHeight / 2
                color: "#4082E4"
            }
        }

        Text{
            id: tipText
            font.pixelSize: 14
            font.family: "PingFang SC"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: progressBar.bottom
            anchors.topMargin: 16
            text: qsTr("正在生成预览")
            font.bold: true
            color: "#FFFFFF"
            renderType: Text.NativeRendering
        }
    }

    function setProgress(progress) {
        progressBar.value = progress
    }

    function setProgressTips(tips) {
        tipText.text = tips
    }
}
