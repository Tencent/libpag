import PAG
import QtQuick
import QtQuick.Controls
import Qt.labs.platform

Popup {
    id: popup

    signal attributeEdited(bool result, string attributeValue)

    property int layerIdKey: 0
    property int markerIndexKey: 0
    property string attributeName: ""
    property string attributeValue: ""

    width: 330
    height: 185
    modal: true
    focus: true
    closePolicy: Popup.OnEscape
    background: Rectangle {
        anchors.fill: parent
        color: "#20202A"
        border.width: 1
        opacity: 1

        Column {
            width: parent.width - 40
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter

            Row {
                width: parent.width
                height: 42

                Text {
                    width: parent.width - closeButton.width
                    height: parent.height
                    text: qsTr("Text Edit")
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }

                Button {
                    id: closeButton
                    width: 12
                    height: 12
                    anchors.top: parent.top
                    anchors.topMargin: (parent.height - height) / 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: (parent.height - height) / 2
                    background: Image {
                        width: parent.height
                        height: parent.height
                        source: "qrc:/images/icon-close.png"
                    }

                    onClicked: {
                        popup.close();
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 100
                color: "#16161D"

                Flickable {
                    id: flickable
                    anchors.fill: parent
                    contentWidth: textInput.width
                    contentHeight: textInput.height
                    clip: true

                    TextEdit {
                        id: textInput
                        font.pixelSize: 12
                        color: "#FFFFFF"
                        selectByMouse: true
                        selectionColor: "#55FFFFFF"
                        verticalAlignment: TextEdit.AlignVCenter
                        wrapMode: TextEdit.Wrap
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: scrollBar
                        policy: ScrollBar.AsNeeded
                        anchors.right: parent.right
                        visible: flickable.contentHeight > flickable.height
                    }
                }
            }

            Item {
                width: parent.width
                height: 10
            }

            Row {
                width: parent.width
                height: 20

                Item {
                    width: parent.width - saveButton.width
                    height: parent.height
                }

                Button {
                    id: saveButton
                    width: 60
                    height: parent.height

                    background: Rectangle {
                        anchors.fill: parent
                        opacity: enabled ? 1 : 0.3
                        border.width: 1
                        radius: 4
                        color: "#6D94B0"
                    }

                    Text {
                        anchors.fill: parent
                        text: qsTr("Save")
                        font.pixelSize: 12
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        let result = editAttributeModel.saveAttribute(layerIdKey, markerIndexKey, attributeName, textInput.text);
                        popup.attributeEdited(result, textInput.text);
                        popup.close();
                    }
                }
            }
        }
    }

    onOpened: {
        textInput.text = attributeValue;
        textInput.focus = true;
        textInput.cursorVisible = true;
    }
}
