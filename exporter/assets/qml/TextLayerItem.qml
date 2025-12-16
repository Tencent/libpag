import QtQuick
import QtQuick.Controls

Rectangle {
    id: textLayer

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
            id: layerNameText
            text: qsTr("Layer Name")
            font.pixelSize: 14
            font.family: "PingFang SC"
            elide: Text.ElideRight
            color: "#FFFFFF"
            anchors.left: numberText.right
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
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
        id: textLayerListView
        width: parent.width
        model: textLayer.model
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
                    visible: textLayerListView.ScrollBar.vertical.size < 1.0
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
                    anchors.right: isEditableRectangle.left
                    anchors.verticalCenter: parent.verticalCenter
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