// import PAG
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls

PListView {
    id: textListView

    model: textLayerModel

    delegate: Rectangle {
        height: 40
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#20202A"

        Rectangle {
            width: 30
            color: "#2D2D37"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.bottomMargin: 1

            Text {
                text: textLayerModel.convertIndex(index)
                font.pixelSize: 12
                renderType: Text.NativeRendering
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#9B9B9B"
                anchors.fill: parent
            }
        }

        Rectangle {
            color: "#2D2D37"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            anchors.left: parent.left
            anchors.leftMargin: 31
            anchors.right: parent.right
            anchors.rightMargin: 14

            TextInput {
                id: textEdit

                text: name
                font.pixelSize: 12
                renderType: Text.NativeRendering
                horizontalAlignment: TextInput.AlignLeft
                verticalAlignment: TextInput.AlignVCenter
                color: "#FFFFFF"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 100
                selectByMouse: true
                selectionColor: "#444444"
                clip: true

                onFocusChanged: {
                    if (!focus) {
                        textLayerModel.replaceText(index, text)
                    }
                }
                Component.onCompleted: {
                    textEdit.ensureVisible(0)
                }

            }

            Button {
                width: 36
                height: 18
                anchors.top: parent.top
                anchors.topMargin: 11
                anchors.right: parent.right
                anchors.rightMargin: 32

                background: Rectangle {
                    color: "#4A4A4A"
                    radius: 9
                    anchors.fill: parent

                    Text {
                        text: qsTr("编辑")
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: "#FFFFFF"
                        anchors.fill: parent
                    }
                }

                onClicked: {
                    console.log("text edit.......")
                    var textDocument = textLayerModel.getTextDocumentAt(index)
                    textLayerModel.recordOldText(index)
                    textEditDialog.x = textEdit.x - 200
                    textEditDialog.y = textEdit.y + 40
                    textEditDialog.fontFamily = textDocument.fontFamily
                    textEditDialog.fontStyle = textDocument.fontStyle
                    textEditDialog.fillColor = textDocument.fillColor
                    textEditDialog.strokeColor = textDocument.strokeColor
                    textEditDialog.open()
                    textEditDialog.layerIndex = index
                    textEditDialog.fauxBold = textDocument.fauxBold
                    textEditDialog.fauxItalic = textDocument.fauxItalic
                    textEditDialog.fontSize = textDocument.fontSize
                    textEditDialog.strokeWidth = textDocument.strokeWidth
                    textEditDialog.textString = textDocument.text
                }
            }

            Button {
                width: 16
                height: 16
                anchors.top: parent.top
                anchors.topMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 10
                enabled: recover ? true : false

                background: Image {
                    id: textBack

                    width: 19
                    height: 19
                    opacity: recover ? 1.0 : 0.3
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/image/back.png"
                }
                onClicked: {
                    textLayerModel.recover(index)
                }
            }
        }
    }

    TextEditDialog {
        id: textEditDialog
    }
}
