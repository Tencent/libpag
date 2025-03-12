// import PAG
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Qt.labs.platform as Lab11

PListView {
    id: listViewImage

    signal replaceImage(int index, string path)

    model: imageLayerModel

    delegate: Rectangle {
        height: 60
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#20202A"

        DropArea {
            id: dropArea

            z: 3
            anchors.fill: parent

            onEntered: {
                console.log("enter")
                drag.accept(Qt.CopyAction)
                if(drag.urls[0]) {
                    filePath = (drag.urls[0])
                }
            }

            onDropped: function(drag) {
                console.log("list......drop x:" + drag.x + " y:" + drag.y)
                let path
                if(filePath !== "") {
                    path = filePath
                } else {
                    path = drag.source.text
                }
                path = path.replace(/^(file:\/{2})/, "")
                path = path.replace(/^(\/)(\w\:)/,'$2')
                replaceImage(index, path)
            }
        }

        Rectangle {
            width: 30
            color: "#2D2D37"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            anchors.left: parent.left

            Text {
                text: imageLayerModel.convertIndex(index)
                font.pixelSize: 12
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

            Image {
                id: image

                width: 40
                height: 40
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.left: parent.left
                anchors.leftMargin: 10
                cache: false
                fillMode: Image.PreserveAspectFit
                source: "image://PAGImageProvider/" + index
            }

            Text {
                text: name
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHLeft
                verticalAlignment: Text.AlignVCenter
                color: "#FFFFFF"
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 60
                anchors.rightMargin: 100
                anchors.right: parent.right
            }

            Lab11.FileDialog {
                id: fileDialog

                title: "Please choose a file"
                fileMode: Lab11.FileDialog.OpenFile
                nameFilters: ["Replacement files (*.jpg *.jpeg *.bmp *.png *.mp4 *.mov)"]

                onAccepted: {
                    console.log("You chose: " + fileDialog.file)
                    let path = fileDialog.file.toString()
                    path = path.replace(/^(file:\/{2})/, "") //  file:///Path/to/file.ext -> /Path/to/file.ext
                    path = path.replace(/^(\/)(\w\:)/,'$2')  //  /D:/Path/to/file.ext     -> D:/Path/to/file.ext
                    let cleanPath = decodeURIComponent(path)
                    replaceImage(index, cleanPath)
                }

                onRejected: {
                    console.log("Canceled")
                }

                Component.onCompleted: {
                    visible = false
                }
            }

            Button {
                width: 40
                height: 18
                anchors.top: parent.top
                anchors.topMargin: 21
                anchors.right: parent.right
                anchors.rightMargin: 32

                background: Rectangle {
                    color: "#4A4A4A"
                    radius: 9
                    anchors.fill: parent

                    Text {
                        text: qsTr("Replace")
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: "#FFFFFF"
                        anchors.fill: parent
                    }
                }

                onClicked: {
                    fileDialog.open()
                }
            }

            Button {
                width: 16
                height: 16
                anchors.top: parent.top
                anchors.topMargin: 22
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.rightMargin: 10
                enabled: recover ? true : false

                background: Image {
                    id: imageBack

                    width: 19
                    height: 19
                    opacity: recover ? 1.0 : 0.3
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/image/back.png"
                }

                onClicked: {
                    replaceImage(index, "")
                    imageLayerModel.recover(index)
                }
            }
        }
    }

}
