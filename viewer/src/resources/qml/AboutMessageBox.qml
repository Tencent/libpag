import QtQuick
import QtQuick.Controls

PApplicationWindow {
    id: window

    signal accepted()
    signal canceled()

    property bool showOK: true
    property bool showCancel: false
    property int textSize: 12
    property string message: ""
    property string messageLicense: ""
    property string messagePrivacy: ""

    maximumWidth: width
    maximumHeight: height
    minimumWidth: width
    minimumHeight: height
    canResize: false

    RectangleWithRadius {
        id: rectangle

        color: "#2D2D37"
        anchors.fill: parent
        leftTop: false
        rightTop: false
        radius: 5

        Image {
            id: image

            x: 36
            y: 100
            width: 80
            height: 80
            sourceSize.width: 160
            sourceSize.height: 160
            anchors.verticalCenterOffset: 0
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            source: "qrc:/image/appicon.png"
        }

        Text {
            id: element

            height: 60

            text: message
            font.pixelSize: window.textSize
            verticalAlignment: Text.AlignVCenter
            anchors.top: parent.top
            anchors.topMargin: 40
            anchors.left: parent.left
            anchors.leftMargin: 137
            anchors.right: parent.right
            anchors.rightMargin: 12
            color: "#ffffff"
        }

        Row {
            id: row1

            height: 25
            spacing: 15
            anchors.top: element.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.right: parent.right

            Item {
                width: 122
                height: 1
            }

            Text {
                id: textLicense

                text: messageLicense
                font.pixelSize: window.textSize + 1
                verticalAlignment: Text.AlignVCenter
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                color: "#008EFF"

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        Qt.openUrlExternally(licenseUrl)
                        console.log("Open license page")
                    }
                }
            }

            Text {
                id: textPricacy

                text: messagePrivacy
                font.pixelSize: window.textSize + 1
                verticalAlignment: Text.AlignVCenter
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                color: "#008EFF"

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        Qt.openUrlExternally(privacyUrl)
                        console.log("Open privacy page")
                    }
                }
            }
        }

        Row {
            id: row2

            height: 25
            spacing: 15
            layoutDirection: Qt.RightToLeft
            anchors.top: row1.bottom
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            anchors.left: parent.left
            anchors.leftMargin: 150
            anchors.right: parent.right
            anchors.rightMargin: 36

            Button {
                id: btnOk


                width: 75
                height: 25
                text: qsTr("确定")
                font.pixelSize: 12
                visible: window.showOK

                background: Rectangle {
                    radius: 3
                    color: "#3485F6"
                }

                contentItem: Text {
                    text: btnOk.text
                    font: btnOk.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.fill: parent
                    color: "#FFFFFF"
                }

                onClicked: {
                    accepted()
                    window.close()
                }
            }

            Button {
                id: btnCancel


                width: 75
                height: 25
                text: qsTr("取消")
                font.pixelSize: 12
                visible: showCancel

                background: Rectangle {
                    radius: 3
                    color: "#88888F"
                }

                contentItem: Text {
                    text: btnCancel.text
                    font: btnCancel.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.fill: parent
                    color: "#FFFFFF"
                }

                onClicked: {
                    canceled()
                    window.close()
                }
            }
        }
    }
}