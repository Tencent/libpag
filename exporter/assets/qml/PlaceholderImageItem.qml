import QtQuick
import QtQuick.Controls

Rectangle {
    id: placeholderImage

    required property var model

    color: "transparent"

    Rectangle {
        id: titleContainer
        width: parent.width
        height: 35
        anchors.top: parent.top
        anchors.left: parent.left
        color: "transparent"

        Text {
            id: numberText
            width: 52
            text: qsTr("Number")
            font.pixelSize: 14
            font.family: "PingFang SC"
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            color: "#FFFFFF"
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: imageNameText
            text: qsTr("Image Name")
            font.pixelSize: 14
            font.family: "PingFang SC"
            elide: Text.ElideRight
            color: "#FFFFFF"
            anchors.left: numberText.right
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
        }

        Item {
            id: fillModeItem
            width: 82
            height: parent.height
            anchors.right: editableCheckBoxItem.left
            anchors.rightMargin: 12

            Image {
                width: 20
                height: 20
                source: "qrc:/images/tip.png"
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: filleModeText.left
                anchors.rightMargin: 2

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        Qt.openUrlExternally("https://pag.art/docs/pag-fillmode.html");
                    }
                }
            }

            Text {
                id: filleModeText
                text: qsTr("Fill Mode")
                font.pixelSize: 14
                font.family: "PingFang SC"
                elide: Text.ElideRight
                color: "#FFFFFF"
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 6
            }
        }

        Item {
            id: editableCheckBoxItem
            width: 100
            height: parent.height
            anchors.right: parent.right
            anchors.rightMargin: 12

            Image {
                width: 20
                height: 20
                source: "qrc:/images/tip.png"
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: editableText.left
                anchors.rightMargin: 2

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        Qt.openUrlExternally("https://pag.art/docs/pag-edit.html");
                    }
                }
            }

            Text {
                id: editableText
                text: qsTr("Editable")
                font.pixelSize: 14
                font.family: "PingFang SC"
                elide: Text.ElideRight
                color: "#FFFFFF"
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: editableCheckBox.left
                anchors.rightMargin: 2
            }

            Image {
                id: editableCheckBox

                property bool isAllEditable: true

                width: 20
                height: 20
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                source: model.isAllEditable ? "qrc:/images/checkbox-on.png" : "qrc:/images/checkbox-off.png"

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        model.isAllEditable = !model.isAllEditable;
                    }
                }
            }
        }
    }

    Rectangle {
        id: titleDivider
        width: parent.width
        height: 1
        anchors.top: titleContainer.bottom
        color: "#000000"
    }

    ListView {
        id: placeholderImageListView
        width: parent.width
        model: placeholderImage.model
        anchors.top: titleDivider.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        clip: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            anchors.right: parent.right
            anchors.rightMargin: 2

            background: Rectangle {
                color: "#00000000"
            }

            contentItem: Rectangle {
                implicitWidth: 9
                implicitHeight: 100
                color: "#00000000"

                Rectangle {
                    anchors.fill: parent
                    radius: 2
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    color: "#AA4B4B5A"
                    visible: placeholderImageListView.ScrollBar.vertical.size < 1.0
                }
            }
        }

        delegate: Item {
            id: item

            required property var row

            required property var index

            required property int number

            required property string name

            required property bool isEditable

            required property string fillMode

            height: 35
            implicitWidth: ListView.view.width

            Rectangle {
                anchors.fill: parent
                color: "transparent"

                Text {
                    id: layerNumber
                    width: 52
                    text: number
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    color: "#FFFFFF"
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    id: layerName
                    text: name
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    elide: Text.ElideRight
                    color: "#FFFFFF"
                    anchors.left: layerNumber.right
                    anchors.leftMargin: 12
                    anchors.right: fillModeComboBox.left
                    anchors.verticalCenter: parent.verticalCenter
                }

                PAGComboBox {
                    id: fillModeComboBox
                    width: 100
                    height: 30
                    implicitWidth: width
                    implicitHeight: height
                    anchors.right: isEditableRectangle.left
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    currentIndex: placeholderImage.model.getScaleModes().indexOf(fillMode)
                    model: placeholderImage.model.getScaleModes()
                    opacity: isEditable ? 1.0 : 0.2

                    onActivated: {
                        if (isEditable) {
                            placeholderImage.model.setScaleMode(row, currentValue);
                        }
                    }
                }

                Rectangle {
                    id: isEditableRectangle
                    width: 100
                    height: parent.height
                    anchors.right: parent.right
                    color: "transparent"

                    Image {
                        id: isEditableCheckBox
                        width: 20
                        height: 20
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: isEditable ? "qrc:/images/checkbox-on.png" : "qrc:/images/checkbox-off.png"

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                model.setIsEditable(row, !isEditable);
                            }
                        }
                    }
                }
            }
        }
    }
}
