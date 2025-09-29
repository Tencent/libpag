import QtQuick
import QtQuick.Controls

PAGWindow {
    id: window
    property bool showCancel: false
    property bool showOK: true
    property int textSize: 12
    property string message: ""
    signal accepted
    signal canceled

    maximumHeight: height
    maximumWidth: width
    minimumHeight: height
    minimumWidth: width
    canResize: false

    PAGRectangle {
        id: rectangle
        color: "#2D2D37"
        anchors.fill: parent
        leftTopRadius: false
        rightTopRadius: false
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
            source: "qrc:/images/window-icon.png"
        }

        Text {
            id: element
            height: 60
            color: "#ffffff"
            text: message
            verticalAlignment: Text.AlignVCenter
            anchors.left: parent.left
            anchors.leftMargin: 137
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 12
            font.pixelSize: textSize
        }

        Row {
            id: row
            y: 121
            height: 25
            spacing: 15
            layoutDirection: Qt.RightToLeft
            anchors.left: parent.left
            anchors.leftMargin: 150
            anchors.right: parent.right
            anchors.rightMargin: 36
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18

            Button {
                id: btnOk
                visible: showOK
                width: 75
                height: 25
                font.pixelSize: 12
                text: qsTr("Confirm")
                onClicked: {
                    accepted();
                    window.close();
                }

                background: Rectangle {
                    radius: 3
                    color: "#3485F6"
                }
                contentItem: Text {
                    text: btnOk.text
                    font: btnOk.font
                    color: "#FFFFFF"
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                id: btnCancel
                visible: showCancel
                width: 75
                height: 25
                font.pixelSize: 12
                text: qsTr("Cancel")
                onClicked: {
                    canceled();
                    window.close();
                }
                background: Rectangle {
                    radius: 3
                    color: "#88888F"
                }
                contentItem: Text {
                    text: btnCancel.text
                    font: btnCancel.font
                    color: "#FFFFFF"
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
