import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ListView {
    id: listView

    required property var imageModel

    property int imageHeight: 60

    model: imageModel
    interactive: false
    boundsBehavior: Flickable.StopAtBounds
    boundsMovement: Flickable.StopAtBounds

    delegate: Rectangle {
        width: listView.width
        height: imageHeight
        color: "#20202A"

        DropArea {
            id: dropArea
            anchors.fill: parent
            z: 3
            onDropped: function (drag) {
                let filePath;
                if (drag.urls[0]) {
                    filePath = (drag.urls[0]);
                } else {
                    filePath = drag.source.text;
                }
                imageModel.changeImage(index, filePath);
            }
        }

        Row {
            spacing: 0
            anchors.fill: parent

            Rectangle {
                width: 30
                color: "#2D2D37"

                anchors.top: parent.top
                anchors.topMargin: 0
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 1

                Text {
                    text: index >= 0 && index < listView.count ? imageModel.convertIndex(index) : ""
                    color: "#9B9B9B"
                    font.pixelSize: 12
                    renderType: Text.NativeRendering
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.fill: parent
                }
            }

            Item {
                width: 1
                height: 1
            }

            Rectangle {
                width: parent.width - 30 - 1
                color: "#2D2D37"

                anchors.top: parent.top
                anchors.topMargin: 0
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 1

                Image {
                    id: image
                    height: 40
                    width: 40
                    anchors.top: parent.top
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    cache: false
                    fillMode: Image.PreserveAspectFit
                    source: index >= 0 && index < listView.count ? "image://PAGImageProvider/" + index : ""
                }

                Button {
                    id: replaceButton
                    height: 18
                    width: 50
                    anchors.top: parent.top
                    anchors.topMargin: (imageHeight - height) / 2
                    anchors.right: parent.right
                    anchors.rightMargin: 32
                    background: Rectangle {
                        color: "#4A4A4A"
                        radius: 9
                        anchors.fill: parent

                        Text {
                            text: qsTr("Replace")
                            font.pixelSize: 10
                            color: "#FFFFFF"
                            anchors.fill: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    onClicked: {
                        selectImageDialog.selectIndex = index;
                        selectImageDialog.open();
                    }
                }

                Button {
                    width: 19
                    height: 19
                    anchors.top: parent.top
                    anchors.topMargin: (imageHeight - height) / 2 + 2
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    enabled: canRevert
                    background: Image {
                        id: textBack
                        width: 19
                        height: 19
                        opacity: canRevert ? 1.0 : 0.3
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/images/revert.png"
                    }
                    onClicked: {
                        imageModel.revertImage(index);
                    }
                }
            }
        }

        Component.onCompleted: {
            if (parent) {
                anchors.left = parent.left;
                anchors.right = parent.right;
            }
        }
    }

    FileDialog {
        id: selectImageDialog

        property int selectIndex: -1

        visible: false
        title: "Please select a file"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Replacement files (*.jpg *.jpeg *.bmp *.png *.mp4 *.mov)"]
        onAccepted: {
            let file = selectImageDialog.selectedFile;
            imageModel.changeImage(selectIndex, file);
        }
    }
}
