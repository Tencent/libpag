import QtQuick
import QtQuick.Controls

Popup {
    id: popup
    width: 200
    height: 200
    visible: false
    modal: true
    focus: true
    closePolicy: Popup.OnEscape

    Overlay.modal: Rectangle {
        color: "transparent"
    }

    property var warnMessage: ""

    property var tipMessageList: []

    property bool toTop: false
    property bool toLeft: true
    property bool toRight: false

    background: borderImage

    BorderImage {
        id: borderImage
        anchors.fill: parent
        source: toTop ? "qrc:/images/top-transparent-background.png" : toLeft ? "qrc:/images/left-transparent-background.png" : "qrc:/images/right-transparent-background.png"
        border.top: toTop ? 50 : 120
        border.bottom: 20
        border.left: toTop ? 120 : toLeft ? 50 : 20
        border.right: toRight ? 50 : 20

        Column {
            width: parent.width
            height: parent.height
            spacing: 10

            Item {
                width: 1
                height: toTop ? 10 : 2
            }

            Text {
                id: warnText
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: parent.right
                anchors.rightMargin: 12
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                color: "#FFFFFF"
                text: warnMessage
                wrapMode: Text.Wrap
                font.pixelSize: 12
            }

            Repeater {
                model: tipMessageList
                delegate: Rectangle {
                    width: parent.width - 20
                    height: childrenRect.height + 20
                    anchors.left: parent.left
                    anchors.leftMargin: toLeft ? 12 : 8
                    anchors.right: parent.right
                    anchors.rightMargin: toRight ? 12 : 8
                    radius: 2
                    color: "#2F2E39"
                    implicitWidth: 100
                    implicitHeight: modelData.paintedHeight + 9 * 2
                    border.color: "#2F2E39"
                    border.width: 1

                    Text {
                        width: parent.width - 16
                        text: modelData
                        font.pixelSize: 12
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        anchors.bottomMargin: 10
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    function addTip(newString) {
        tipMessageList.push(newString);
        tipMessageList = tipMessageList.slice();
    }

    function clearAllTips() {
        tipMessageList = [];
    }

    function setToTop() {
        toTop = true;
        toLeft = false;
        toRight = false;
    }

    function setToLeft() {
        toTop = false;
        toLeft = true;
        toRight = false;
    }

    function setToRight() {
        toTop = false;
        toLeft = false;
        toRight = true;
    }
}
