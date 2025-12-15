import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: window

    signal accepted
    signal rejected

    width: 400
    height: 200
    minimumWidth: 400
    minimumHeight: 200
    maximumWidth: 400
    maximumHeight: 200
    visible: true
    flags: Qt.FramelessWindowHint | Qt.Dialog
    modality: Qt.WindowModal
    color: "#14141e"

    Text {
        id: title
        text: qsTr("Convert to BMP?")
        color: "#FFFFFF"
        font.pixelSize: 18
        font.family: "PingFang SC"
        renderType: Text.NativeRendering
        anchors.top: parent.top
        anchors.topMargin: 25
        anchors.left: parent.left
        anchors.leftMargin: 30
    }

    Text {
        id: message
        text: qsTr("Placeholder image and text in composition cannot edit after converting to BMP")
        color: "#FFFFFF"
        font.pixelSize: 14
        font.family: "PingFang SC"
        wrapMode: Text.Wrap
        renderType: Text.NativeRendering
        anchors.top: title.bottom
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.leftMargin: 30
        anchors.right: parent.right
        anchors.rightMargin: 30
    }

    Rectangle {
        id: cancelButton
        width: 86
        height: 36
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        anchors.right: confirmButton.left
        anchors.rightMargin: 10
        color: "#282833"
        radius: 2

        Text {
            text: qsTr("Cancel")
            font.pixelSize: 15
            font.family: "PingFang SC"
            color: "#FFFFFF"
            renderType: Text.NativeRendering
            anchors.centerIn: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                rejected();
                window.close();
            }
        }
    }

    Rectangle {
        id: confirmButton
        width: 86
        height: 36
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        anchors.right: parent.right
        anchors.rightMargin: 20
        color: "#0084f3"
        radius: 2

        Text {
            text: qsTr("Confirm")
            font.pixelSize: 15
            font.family: "PingFang SC"
            renderType: Text.NativeRendering
            color: "#FFFFFF"
            anchors.centerIn: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                accepted();
                window.close();
            }
        }
    }

    Item {
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: function (event) {
            rejected();
            window.close();
            event.accepted = true;
        }
    }
}
