import QtQuick
import QtQuick.Controls
import "components"

PAGWindow {
    id: aboutWindow

    property string aboutMessage: ""
    property string licenseMessage: ""
    property string privacyMessage: ""
    property string licenseUrl: ""
    property string privacyUrl: ""

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
            anchors.fill: parent
            Item {
                width: 30
                height: 1
            }

            Image {
                id: image
                width: 80
                height: 80
                sourceSize.width: 160
                sourceSize.height: 160
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
                spacing: 10
                Item {
                    width: 1
                    height: 20
                }
                Text {
                    id: aboutText
                    color: "#ffffff"
                    text: aboutMessage
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 12
                }
                Row {
                    visible: licenseMessage !== "" || privacyMessage !== ""
                    height: 25
                    spacing: 15

                    Text {
                        id: licenseText
                        visible: licenseUrl !== ""
                        text: licenseMessage
                        color: "#008EFF"
                        verticalAlignment: Text.AlignVCenter
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 13
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                Qt.openUrlExternally(licenseUrl);
                            }
                        }
                    }

                    Text {
                        id: privacyText
                        visible: privacyUrl !== ""
                        text: privacyMessage
                        color: "#008EFF"
                        verticalAlignment: Text.AlignVCenter
                        anchors.topMargin: 10
                        anchors.bottomMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 13
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                Qt.openUrlExternally(privacyUrl);
                            }
                        }
                    }
                }

                Row {
                    spacing: 0
                    Item {
                        width: aboutWindow.width - 75 - 70 - 100
                        height: 1
                    }
                    Button {
                        id: btnOk
                        width: 75
                        height: 25
                        font.pixelSize: 12
                        text: qsTr("Confirm")
                        onClicked: {
                            aboutWindow.close();
                        }

                        background: Rectangle {
                            radius: 3
                            color: "#3485F6"
                        }
                        contentItem: Text {
                            text: btnOk.text
                            font: btnOk.font
                            color: "#FFFFFF"
                            anchors.fill: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }
}
