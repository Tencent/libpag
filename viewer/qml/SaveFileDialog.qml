import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "components"
import "utils"

PAGWindow {
    id: saveFileDialog

    property string message: ""
    property string filePath: ""
    property string fileSuffix: ""

    property alias pathText: pathText
    property alias saveButton: saveButton
    property alias selectPathButton: selectPathButton

    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    hasMenu: false
    canResize: false
    titleBarHeight: isWindows ? 32 : 22

    PAGRectangle {
        id: rectangle

        color: "#2D2D37"
        anchors.fill: parent
        leftTopRadius: false
        rightTopRadius: false
        radius: 5

        Row {
            spacing: 0

            anchors.fill: parent

            Item {
                width: 20
                height: 1
            }

            Image {
                id: image
                width: 96
                height: 96
                sourceSize.width: 192
                sourceSize.height: 192
                anchors.verticalCenterOffset: 0
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
                source: "qrc:/images/window-icon.png"
            }

            Item {
                width: 20
                height: 1
            }

            Column {
                spacing: 0
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                Item {
                    width: 1
                    height: 30
                }

                Text {
                    id: messageText
                    color: "#ffffff"
                    text: message
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 14
                }

                Item {
                    width: 1
                    height: 10
                }

                Rectangle {
                    width: parent.width
                    height: 30
                    color: "transparent"
                    border.color: "#4A4A54"
                    border.width: 1
                    radius: 3

                    Text {
                        id: pathText
                        anchors.fill: parent
                        anchors.margins: 5
                        color: filePath === "" ? "#E0E0E0" : "#707070"
                        text: filePath
                        elide: Text.ElideMiddle
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                        clip: true

                        ToolTip.text: filePath
                        ToolTip.delay: 500
                        ToolTip.visible: hovered

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }

                Item {
                    width: 1
                    height: 20
                }

                Row {
                    width: parent.width

                    Item {
                        width: 10
                        height: 1
                    }

                    Button {
                        id: selectPathButton
                        width: 90
                        height: 25
                        font.pixelSize: 14
                        text: qsTr("Select Path")

                        background: Rectangle {
                            radius: 3
                            color: "#3485F6"
                        }
                        contentItem: Text {
                            text: selectPathButton.text
                            font: selectPathButton.font
                            color: "#FFFFFF"
                            anchors.fill: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            selectPathDialog.defaultSuffix = saveFileDialog.fileSuffix;
                            selectPathDialog.selectedFile = saveFileDialog.filePath;
                            selectPathDialog.currentFolder = Utils.getFileDir(saveFileDialog.filePath);
                            selectPathDialog.open();
                        }
                    }

                    Item {
                        width: parent.width - selectPathButton.width - saveButton.width - 20
                        height: 1
                    }

                    Button {
                        id: saveButton
                        width: 90
                        height: 25
                        font.pixelSize: 14
                        text: qsTr("Save")

                        background: Rectangle {
                            radius: 3
                            color: "#3485F6"
                        }
                        contentItem: Text {
                            text: saveButton.text
                            font: saveButton.font
                            color: "#FFFFFF"
                            anchors.fill: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Item {
                        width: 10
                        height: 1
                    }
                }
            }

            Item {
                width: 20
                height: 1
            }
        }
    }

    FileDialog {
        id: selectPathDialog
        visible: false
        title: qsTr("Select save path")
        fileMode: FileDialog.SaveFile

        onAccepted: {
            saveFileDialog.filePath = Utils.removeFileSchema(selectPathDialog.selectedFile);
        }
    }
}
