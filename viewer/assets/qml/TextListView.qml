import QtQuick
import QtQuick.Controls

ListView {
    id: listView

    required property var textModel

    property int textHeight: 40

    property alias textEditDialog: textEditDialog

    model: textModel
    interactive: false
    boundsBehavior: Flickable.StopAtBounds
    boundsMovement: Flickable.StopAtBounds

    delegate: Rectangle {
        width: listView.width
        height: textHeight
        color: "#20202A"

        Rectangle {
            id: indexRectangle
            width: 30
            height: parent.height
            color: "#2D2D37"

            anchors.top: parent.top
            anchors.topMargin: 0
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            anchors.left: parent.left
            anchors.leftMargin: 0

            Text {
                text: textModel.convertIndex(index)
                color: "#9B9B9B"
                font.pixelSize: 12
                renderType: Text.NativeRendering
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                anchors.fill: parent
            }
        }

        Rectangle {
            id: contentRectangle
            height: parent.height
            color: "#2D2D37"
            anchors.top: parent.top
            anchors.topMargin: 0
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            anchors.left: indexRectangle.right
            anchors.leftMargin: 1
            anchors.right: parent.right
            anchors.rightMargin: 0

            TextInput {
                id: textEdit
                text: value
                font.pixelSize: 12
                horizontalAlignment: TextInput.AlignLeft
                verticalAlignment: TextInput.AlignVCenter
                renderType: Text.NativeRendering
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 100
                selectByMouse: true
                persistentSelection: true
                color: "#FFFFFF"
                selectionColor: "#444444"
                clip: true

                onEditingFinished: {
                    if (!focus) {
                        listView.textModel.changeText(index, text);
                    }
                }

                onAccepted: {
                    focus = false;
                }

                Component.onCompleted: {
                    ensureVisible(0);
                }
            }

            Button {
                id: editButton
                height: 18
                width: 36
                enabled: textEditDialog !== null
                anchors.top: parent.top
                anchors.topMargin: (textHeight - height) / 2
                anchors.right: parent.right
                anchors.rightMargin: 32
                background: Rectangle {
                    color: "#4A4A4A"
                    radius: 9
                    anchors.fill: parent

                    Text {
                        text: qsTr("Edit")
                        font.pixelSize: 10
                        color: "#FFFFFF"
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                onClicked: {
                    textModel.recordTextDocument(index);
                    textEditDialog.layerIndex = index;
                    textEditDialog.fauxBold = textModel.fauxBold(index);
                    textEditDialog.fauxItalic = textModel.fauxItalic(index);
                    textEditDialog.strokeWidth = textModel.strokeWidth(index);
                    textEditDialog.fillColor = textModel.fillColor(index);
                    textEditDialog.strokeColor = textModel.strokeColor(index);
                    textEditDialog.fontSize = textModel.fontSize(index);
                    textEditDialog.fontFamily = textModel.fontFamily(index);
                    textEditDialog.fontStyle = textModel.fontStyle(index);
                    textEditDialog.displatText = textModel.text(index);
                    textEditDialog.open();
                }
            }

            Button {
                width: 19
                height: 19
                anchors.top: parent.top
                anchors.topMargin: (textHeight - height) / 2 + 2
                anchors.right: parent.right
                anchors.rightMargin: 10
                enabled: canRevert
                padding: 0
                background: Image {
                    id: textBack
                    width: 19
                    height: 19
                    opacity: canRevert ? 1.0 : 0.3
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/images/revert.png"
                }
                onClicked: {
                    textModel.revertText(index);
                }
            }
        }
    }

    TextEditDialog {
        id: textEditDialog
        textModel: listView.textModel

        parent: Overlay.overlay

        onOpened: {
            if (parent) {
                x = (parent.width - width) / 2;
                y = (parent.height - height) / 2;
            }
        }

        onClosed: {
            layerIndex = -1;
        }
    }
}
