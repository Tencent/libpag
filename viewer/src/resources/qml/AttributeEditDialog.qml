import QtQuick
import QtQuick.Controls
import Qt.labs.platform

Popup {
    id: root

    signal activated(bool saveResult, string textString)

    property int layerIdKey: 0
    property int markerIndexKey: 0
    property string attributeName: ""
    property string textString: ""

    width: 330
    height: 200
    modal: true
    focus: true
    closePolicy: Popup.OnEscape

    background: rect

    Rectangle {
        id: rect

        anchors.fill: parent
        color: "#20202A"
        border.width: 1
        opacity: 1

        Text {
            width: parent.width
            height: 52
            text: qsTr("文本编辑")
            font.pixelSize: 12
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            color: "#FFFFFF"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: 20

            MouseArea {
                property point clickPoint: "0,0"

                anchors.fill: parent
                acceptedButtons: Qt.LeftButton

                onPressed: {
                    clickPoint = Qt.point(mouse.x, mouse.y)
                }
                onPositionChanged: {
                    setDlgPoint(mouse.x - clickPoint.x, y - clickPoint.y)
                }
            }
        }

        Rectangle {
            x: 20
            y: 46
            width: 290
            height: 100
            color: "#16161D"

            Flickable {
                id: flick

                anchors.fill: parent
                contentWidth: textInput.paintedWidth
                contentHeight: textInput.paintedHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                TextEdit {
                    id: textInput

                    width: flick.width
                    text: textString
                    font.pixelSize: 12
                    verticalAlignment: TextEdit.AlignVCenter
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    leftPadding: 5
                    color: "#FFFFFF"
                    selectionColor: "#55FFFFFF"

                    onTextChanged: {
                        console.log("currentText:" + textInput.text)
                    }

                    onCursorRectangleChanged: {
                        ensureVisible(cursorRectangle)
                    }
                }

                function ensureVisible(r) {
                    if (contentX >= r.x) {
                        contentX = r.x
                    } else if ((contentX + width) <= (r.x + r.width)) {
                        contentX = r.x + r.width - width
                    }
                    if (contentY >= r.y) {
                        contentY = r.y
                    } else if ((contentY + height) <= (r.y + r.height)) {
                        contentY = r.y + r.height - height
                    }
                }
            }
        }

        Button {
            width: 60
            height: 20
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 25
            anchors.right: parent.right
            anchors.rightMargin: 20

            background: Rectangle {
                anchors.fill: parent
                opacity: enabled ? 1 : 0.3
                border.width: 1
                radius: 4
                color: "#6D94B0"
            }

            Text {
                text: qsTr("保存")
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                anchors.fill: parent
                color: "#FFFFFF"
            }

            onClicked: {
                // TODO
                // let saveResult = pagEditAttributeModel.savePAGFileAttribute(layerIdKey, markerIndexKey, attributeName, textInput.text)
                // root.activated(saveResult, textInput.text)
                // root.close()
            }
        }

        Button {
            width: 60
            height: 20
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 25
            anchors.left: parent.left
            anchors.leftMargin: 20

            background: Rectangle {
                anchors.fill: parent
                opacity: enabled ? 1 : 0.3
                border.width: 1
                radius: 4
                color: "#6D94B0"
            }

            Text {
                text: qsTr("JSON校验")
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                anchors.fill: parent
                color: "#FFFFFF"
            }
            onClicked: {
                // TODO
                // let errorList = pagEditAttributeModel.checkJsonStringIsValid(textInput.text)
                // errorLabel.text = errorList[0]
                // errorLabel.visible = true
                // if (errorList[0] !== "no error occurred") {
                //     textInput.cursorPosition = parseInt(errorList[1])
                //     textInput.cursorVisible = true
                // }
            }
        }

        Label {
            id: errorLabel

            height: 17
            text: qsTr("JSON校验信息")
            font.pixelSize: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            color: Qt.rgba(255, 0, 0, 1)
            visible: false
        }

        Button {
            width: 12
            height: 12
            anchors.top: parent.top
            anchors.topMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20

            background: Image {
                width: 12
                height: 12
                fillMode: Image.PreserveAspectFit
                source: "qrc:/image/icon-close.png"
            }

            onClicked: {
                root.close()
            }
        }
    }

    onOpened: {
        textInput.focus=true
        textInput.cursorVisible=true
        textInput.text = textString
        errorLabel.visible=false
    }

    function setDlgPoint(dlgX, dlgY) {
        root.x = root.x + dlgX
        root.y = root.y + dlgY
    }
}
