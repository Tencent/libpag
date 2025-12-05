import QtQuick
import QtQuick.Controls

Rectangle {
    id: preComposition

    required property var model

    required property string backgroundColor

    required property var parentWindow

    color: "transparent"

    Rectangle {
        id: compositionContainer
        width: 300
        height: parent.height
        anchors.top: parent.top
        anchors.left: parent.left
        color: "transparent"

        Rectangle {
            id: title
            width: parent.width
            height: 35
            anchors.top: parent.top
            anchors.left: parent.left
            color: "transparent"

            Text {
                id: nameText
                text: qsTr("Name")
                font.pixelSize: 14
                font.family: "PingFang SC"
                elide: Text.ElideRight
                color: "#FFFFFF"
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                id: bmpText
                text: "BMP"
                font.pixelSize: 14
                font.family: "PingFang SC"
                elide: Text.ElideRight
                color: "#FFFFFF"
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            id: titleDivider
            width: parent.width
            height: 1
            anchors.top: title.bottom
            color: "#000000"
        }

        ListView {
            id: compositonList
            width: parent.width
            height: parent.height - title.height - titleDivider.height
            anchors.top: titleDivider.bottom
            anchors.left: parent.left
            model: preComposition.model
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
                        visible: compositonList.ScrollBar.vertical.size < 1.0
                    }
                }
            }

            delegate: Item {
                id: item

                required property var row

                required property var index

                required property string name

                required property int level

                required property bool isExportAsBmp

                required property bool isEnable

                height: 35
                implicitWidth: ListView.view.width

                Rectangle {
                    anchors.fill: parent
                    color: "transparent"

                    Rectangle {
                        height: parent.height
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.leftMargin: 12 + (level < 5 ? level * 20 : 80)
                        anchors.right: parent.right
                        color: "transparent"

                        Image {
                            id: icon
                            width: 20
                            height: 20
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            source: "qrc:/images/bmp.png"
                        }

                        Text {
                            id: nameText
                            text: name
                            font.pixelSize: 14
                            font.family: "PingFang SC"
                            elide: Text.ElideRight
                            color: "#FFFFFF"
                            anchors.left: icon.right
                            anchors.leftMargin: 8
                            anchors.right: bmpCheckBox.left
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Image {
                            id: bmpCheckBox

                            property var alertDialog: null

                            width: 20
                            height: 20
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            source: isExportAsBmp ? "qrc:/images/checkbox-on.png" : "qrc:/images/checkbox-off.png"
                            opacity: isEnable ? 1.0 : 0.2

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (!isEnable) {
                                        return;
                                    }
                                    if (!isExportAsBmp && model.isCompositionHasEditableLayer(row)) {
                                        let component = Qt.createComponent("qrc:/qml/AlertDialog.qml");
                                        if (component.status === Component.Ready) {
                                            let dialog = component.createObject(parentWindow, {
                                                modality: Qt.WindowModal
                                            });
                                            bmpCheckBox.alertDialog = dialog;
                                            if (dialog) {
                                                dialog.accepted.connect(function () {
                                                    model.setExportAsBmp(row, true);
                                                    if (bmpCheckBox.alertDialog) {
                                                        bmpCheckBox.alertDialog.destroy();
                                                        bmpCheckBox.alertDialog = null;
                                                    }
                                                });
                                                dialog.show();
                                            }
                                        }
                                    } else {
                                        model.setExportAsBmp(row, !isExportAsBmp);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: compositionDivider
        width: 1
        height: parent.height
        color: "#000000"
        anchors.top: parent.top
        anchors.left: compositionContainer.right
    }

    Rectangle {
        id: frameDisplayContainer
        width: parent.width - 1 - compositionContainer.width - 1
        height: parent.height
        anchors.top: parent.top
        anchors.left: compositionDivider.right
        color: "transparent"

        Rectangle {
            id: frameImageConatiner
            width: parent.width
            height: parent.height - sliderContaner.height
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: sliderContaner.top
            color: backgroundColor

            Image {
                id: frameImage
                width: parent.width
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                asynchronous: true
                cache: false
                source: "image://" + model.imageProviderName + "/" + model.currentFrame

                Component.onCompleted: {
                    let width = model.width;
                    let height = model.height;
                    if ((height / width) > (frameImage.height / frameImage.width)) {
                        frameImage.width = frameImage.height * (width / height);
                    } else if ((height / width) < (frameImage.height / frameImage.width)) {
                        frameImage.height = frameImage.width * (height / width);
                    }
                }
            }
        }

        Rectangle {
            id: sliderContaner
            width: parent.width
            height: 50
            anchors.bottom: parent.bottom
            color: "#2E2E38"

            Slider {
                id: slider
                height: 20
                value: 0
                to: Number(model.duration)
                stepSize: 1
                enabled: true
                padding: 0
                anchors.top: parent.top
                anchors.topMargin: 3
                anchors.bottomMargin: 0
                anchors.left: parent.left
                anchors.leftMargin: 0
                anchors.right: parent.right
                anchors.rightMargin: 0

                background: Rectangle {
                    x: slider.leftPadding
                    y: slider.topPadding + slider.availableHeight / 2 - height / 2
                    implicitWidth: 200
                    implicitHeight: 4
                    width: slider.availableWidth
                    height: implicitHeight
                    radius: 0
                    color: "#7D7D7D"

                    Rectangle {
                        width: slider.visualPosition * parent.width
                        height: parent.height
                        color: "#448EF9"
                        radius: 0
                    }
                }

                handle: Rectangle {
                    x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                    y: slider.topPadding + slider.availableHeight / 2 - height / 2
                    implicitWidth: 12
                    implicitHeight: 12
                    radius: 12
                    color: slider.pressed ? "#f0f0f0" : "#f6f6f6"
                    border.color: "#bdbebf"
                }

                onValueChanged: {
                    model.currentFrame = value;
                }
            }

            Rectangle {
                id: progressContainer
                width: parent.width
                height: 25
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                color: "transparent"

                Item {
                    width: childrenRect.width
                    height: childrenRect.height
                    anchors.centerIn: parent

                    Row {
                        id: row
                        spacing: 0

                        Text {
                            id: currrentTimeText
                            text: model.currentTime
                            font.pixelSize: 12
                            color: "#FFFFFF"
                        }

                        Item {
                            id: currentTimeDivider
                            width: 20
                            height: 1
                        }

                        Text {
                            id: currentFrameText
                            text: model.currentFrame
                            font.pixelSize: 12
                            color: "#FFFFFF"
                        }

                        Text {
                            id: frameDivider
                            text: " / "
                            font.pixelSize: 12
                            color: "#EEEEEE"
                        }

                        Text {
                            id: totalFrameText
                            text: model.duration
                            font.pixelSize: 12
                            color: "#FFFFFF"
                        }
                    }
                }
            }
        }
    }
}
