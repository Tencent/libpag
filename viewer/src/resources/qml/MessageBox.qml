import QtQuick
import QtQuick.Controls

PApplicationWindow {
    id: window

    property bool showOK: true
    property bool showCancel: false
    property int textSize: 12
    property string message: ""

    signal accepted()
    signal canceled()

    maximumWidth:width
    maximumHeight: height
    minimumWidth:width
    minimumHeight: height
    canResize: false

    RectangleWithRadius {
        id: rectangle

        color: "#2D2D37"
        anchors.fill: parent
        leftTop: false
        rightTop: false
        radius: 5

        Image {
            id: image

            x: 36
            y: 100
            width: 80
            height: 80
            sourceSize.width: 160
            sourceSize.height: 160
            anchors.verticalCenterOffset: 0
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            source: "qrc:/image/appicon.png"
        }

        Text {
            id: element

            height: 60
            color: "#ffffff"
            text: message
            font.pixelSize: textSize
            verticalAlignment: Text.AlignVCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 137
            anchors.right: parent.right
            anchors.rightMargin: 12
        }

        Row {
            id: row

            y: 121
            height: 25
            spacing: 15
            layoutDirection: Qt.RightToLeft
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            anchors.left: parent.left
            anchors.leftMargin: 150
            anchors.right: parent.right
            anchors.rightMargin: 36

            Button {
                id: btnOk

                width: 75
                height: 25
                text: qsTr("确定")
                font.pixelSize: 12
                visible: window.showOK

                background: Rectangle {
                    radius: 3
                    color: "#3485F6"
                }

                contentItem: Text {
                    text: btnOk.text
                    font: btnOk.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                    anchors.fill: parent
                }

                onClicked: {
                    accepted()
                    window.close()
                }
            }

            Button {
                id: btnCancel

                width: 75
                height: 25
                text: qsTr("取消")
                font.pixelSize: 12
                visible: window.showCancel

                background: Rectangle {
                    radius: 3
                    color: "#88888F"
                }

                contentItem: Text {
                    text: btnCancel.text
                    font: btnCancel.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                    anchors.fill: parent
                }

                onClicked: {
                    canceled()
                    window.close()
                }
            }
        }
    }

}
