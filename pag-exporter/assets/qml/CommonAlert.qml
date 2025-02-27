import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Window 2.13

PApplicationWindow {
    property var windowWidth : 402
    property var windowHeight: 205
    property var isPreview: false
    property var fontFamily: "PingFang SC"
    property var title: qsTr("是否安装PAGViewer?")
    property var description: qsTr("安装完成后,可以通过PAGViewer进行效果预览")
    property var desVisible: true
    property var cancelVisible: true

    id: wind
    objectName: alertWindow
    visible: true
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    windowBackground: Qt.rgba(20/255,20/255,30/255,1)
    canResize: false
    isWindows: true
    titleFontSize: 14
    maxBtnVisible: false
    minBtnVisible: false
    canMaxSize: false
    titlebarVisible: false

    onClosing: function(closeEvent) {
        console.log("Alert window close")
    }

    Rectangle{
        anchors.fill: parent
        anchors.leftMargin: 25
        anchors.rightMargin: 20
        color: "transparent"

        Text{
            id: titleText
            anchors.top: parent.top
            anchors.topMargin: 15
            anchors.left: parent.left
            anchors.right: parent.right
            font.pixelSize: 20
            font.family: fontFamily
            text: title
            color: "#FFFFFF"
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Text{
            id: desText
            visible: desVisible
            anchors.top: titleText.bottom
            anchors.topMargin: 20
            anchors.left: parent.left
            anchors.right: parent.right
            font.pixelSize: 15
            font.family: fontFamily
            text: description
            color: "#FFFFFF"
            wrapMode: Text.WordWrap
        }

        Rectangle{
            id: sureBtn
            width: 87
            height: 36
            color: "#1982EB"
            radius: 2
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20

            Text{
                font.pixelSize: 15
                font.family: "PingFang SC"
                anchors.centerIn: parent
                text: qsTr("确定")
                font.bold: true
                color: "#FFFFFF"
            }

            MouseArea{
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    commonAlert.handleClickOK()
                }
            }
        }

        Rectangle{
            id: cancelBtn
            visible: cancelVisible
            width: 87
            height: 36
            radius: 2
            color: "#282832"
            anchors.right: sureBtn.left
            anchors.rightMargin: 8
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20

            Text{
                font.pixelSize: 15
                font.family: "PingFang SC"
                anchors.centerIn: parent
                text: qsTr("取消")
                font.bold: true
                color: "#FFFFFF"
            }

            MouseArea{
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    wind.close()
                }
            }
        }
    }

    function setTitle(msg) {
       title=msg
    }

    function setDescription(msg) {
       description=msg
    }

    function setDesVisible(visible) {
        desVisible=visible
    }

    function setCancelVisible(visible) {
        cancelVisible=visible
    }
}