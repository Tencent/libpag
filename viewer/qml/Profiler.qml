import PAG
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: element

    property int defaultWidth: 300
    property int defaultHeight: 300

    property alias graph: graph
    property alias graphCanvas: graphCanvas

    width: defaultWidth
    height: defaultHeight

    PAGRunTimeModelManager {
        id: runTimeModelManager
        objectName: "runTimeModelManager"
    }

    Column {
        spacing: 0
        anchors {
            fill: parent
            leftMargin: 12
            rightMargin: 12
        }

        Item {
            width: parent.width
            height: 12
        }

        /* Title */
        Rectangle {
            id: profilerTitle
            color: "#2D2D37"
            height: 25
            anchors.left: parent.left
            anchors.right: parent.right

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Item {
                    width: 10
                    height: 1
                }

                Label {
                    id: titleLabel
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("File Info")
                    color: "#FFFFFF"
                    font.pixelSize: 10
                    visible: true
                }

                Item {
                    Layout.fillWidth: true
                    height: 1
                }

                Image {
                    id: fileEncryptionImage
                    Layout.preferredWidth: 11
                    Layout.preferredHeight: 11
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/images/un-encryption.png"
                    fillMode: Image.PreserveAspectFit
                }

                Item {
                    width: 5
                    height: 1
                }

                Text {
                    id: encryptionLabel
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Unencrypted")
                    color: "#FFFFFF"
                    font.pixelSize: 10
                }

                Item {
                    width: 10
                    height: 1
                }
            }
        }

        Item {
            width: parent.width
            height: 1
        }

        /* File Info */
        GridView {
            id: fileInfoView
            height: 93
            width: parent.width
            cellWidth: width / 4
            cellHeight: 46
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationWraps: false
            anchors.left: parent.left
            anchors.right: parent.right

            model: runTimeModelManager.fileInfoModel

            delegate: Item {
                width: fileInfoView.cellWidth
                height: fileInfoView.cellHeight

                Rectangle {
                    height: fileInfoView.cellHeight - 1
                    width: fileInfoView.cellWidth - ((index + 1) % 4 === 0 ? 0 : 1)
                    color: "#2D2D37"
                    anchors {
                        left: parent.left
                        top: parent.top
                        rightMargin: 1
                        bottomMargin: 1
                    }
                    Text {
                        id: nameText
                        color: "#9b9b9b"
                        text: name
                        font.pixelSize: 9
                        renderType: Text.NativeRendering
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                    }

                    Text {
                        id: valueText
                        color: "#ffffff"
                        text: value
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                        renderType: Text.NativeRendering
                        anchors.top: nameText.bottom
                        anchors.topMargin: 2
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.left: nameText.left
                        anchors.leftMargin: 0
                    }

                    Text {
                        id: extText
                        color: "#9b9b9b"
                        text: ext
                        font.pixelSize: 9
                        renderType: Text.NativeRendering
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 5
                        anchors.left: valueText.right
                        anchors.leftMargin: 3
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: 2
        }

        /* performance chart */
        Rectangle {
            id: performanceChart
            height: 90
            color: "#00000000"
            anchors.left: parent.left
            anchors.right: parent.right

            Rectangle {
                id: graph
                height: 90
                color: "#2D2D37"
                clip: true
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right

                Canvas {
                    id: graphCanvas

                    height: 70
                    width: graph.width
                    anchors.bottom: parent.bottom
                    renderStrategy: Canvas.Cooperative

                    onPaint: {

                    }
                }
                Text {
                    id: currentFrameText
                    text: "0/0"
                    font.pixelSize: 9
                    anchors.top: parent.top
                    anchors.topMargin: 5
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    color: "#9B9B9B"
                    renderType: Text.NativeRendering
                }
            }
        }

        Item {
            width: parent.width
            height: 4
        }

        /* Data Statistics */
        Rectangle {
            id: statistics
            height: 62
            color: "#00000000"
            anchors.left: parent.left
            anchors.right: parent.right

            ListView {
                id: dataListView

                width: parent.width
                boundsBehavior: ListView.StopAtBounds
                interactive: false
                anchors.fill: parent
                spacing: 1

                model: runTimeModelManager.frameDisplayInfoModel

                delegate: Item {
                    height: 20
                    Row {
                        spacing: 1
                        Rectangle {
                            height: 20
                            width: (dataListView.width - 2) / 3
                            color: "#2D2D37"
                            Text {
                                text: name
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                color: colorCode
                                renderType: Text.NativeRendering
                            }
                            Text {
                                text: current
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                color: "white"
                                renderType: Text.NativeRendering
                            }
                        }
                        Rectangle {
                            height: 20
                            width: (dataListView.width - 2) / 3
                            color: "#2D2D37"
                            Text {
                                text: "AVG"
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                color: colorCode
                                renderType: Text.NativeRendering
                            }
                            Text {
                                text: avg
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                color: "white"
                                renderType: Text.NativeRendering
                            }
                        }
                        Rectangle {
                            height: 20
                            width: (dataListView.width - 2) / 3
                            color: "#2D2D37"
                            Text {
                                text: "MAX"
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                color: colorCode
                                renderType: Text.NativeRendering
                            }
                            Text {
                                text: max
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                color: "white"
                                renderType: Text.NativeRendering
                            }
                        }
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: 20
        }
    }
}
