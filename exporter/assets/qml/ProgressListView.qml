import QtQuick
import QtQuick.Controls.Universal

ListView {
    id: progressListView

    clip: true
    boundsBehavior: Flickable.StopAtBounds

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
                visible: progressListView.ScrollBar.vertical.size < 1.0
            }
        }
    }

    ScrollBar.horizontal: ScrollBar {
        policy: ScrollBar.AsNeeded
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2

        background: Rectangle {
            color: "#00000000"
        }

        contentItem: Rectangle {
            implicitWidth: 100
            implicitHeight: 9
            color: "#00000000"

            Rectangle {
                anchors.fill: parent
                radius: 2
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2
                color: "#AA4B4B5A"
                visible: progressListView.ScrollBar.horizontal.size < 1.0
            }
        }
    }

    delegate: Item {
        required property var row

        required property var index

        required property string name

        required property int exportStatus

        required property double totalProgress

        required property double currentProgress

        height: 35
        implicitWidth: ListView.view.width

        Image {
            id: statusIcon
            width: 20
            height: 20
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: true
            source: {
                if (exportStatus === 2) {
                    return "qrc:/images/export-wrong.png";
                }
                if (exportStatus === 1) {
                    return "qrc:/images/export-success.png";
                }
                return "qrc:/images/export-waiting.png";
            }
            rotation: exportStatus !== 0 ? 0 : statusIcon.rotation
            RotationAnimation on rotation {
                running: exportStatus === 0
                to: 360
                duration: 1500
                loops: Animation.Infinite
            }
        }

        Text {
            id: nameText
            text: name
            height: 22
            font.pixelSize: 14
            font.family: "PingFang SC"
            verticalAlignment: Text.AlignVCenter
            color: "#FFFFFF"
            anchors.left: statusIcon.right
            anchors.leftMargin: 12
            anchors.right: progressBarContainer.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            id: progressBarContainer
            width: 140
            height: 22
            color: "transparent"
            anchors.right: viewButton.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter

            ProgressBar {
                id: progressBar
                to: totalProgress
                value: currentProgress
                width: 100
                height: 8
                anchors.verticalCenter: parent.verticalCenter

                background: Rectangle {
                    color: "#2B2B33"
                    border.width: 0
                    radius: progressBar.height / 2
                    implicitWidth: progressBar.width
                    implicitHeight: progressBar.height
                }

                contentItem: Item {
                    Rectangle {
                        visible: progressBar.visualPosition !== 0
                        width: progressBar.visualPosition * progressBar.width
                        height: progressBar.height
                        radius: progressBar.height / 2
                        color: "#4082E4"
                    }
                }
            }

            Text {
                id: progressText
                text: Math.round(currentProgress / totalProgress * 100) + "%"
                height: 22
                font.pixelSize: 14
                font.family: "PingFang SC"
                verticalAlignment: Text.AlignVCenter
                color: "#FFFFFF"
                anchors.left: progressBar.right
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            id: viewButton
            width: 48
            height: 22
            color: "#1982EB"
            radius: 2
            enabled: exportStatus === 1
            opacity: enabled ? 1.0 : 0.3
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: qsTr("View")
                font.pixelSize: 14
                font.bold: true
                font.family: "PingFang SC"
                anchors.centerIn: parent
                color: "#FFFFFF"
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    model.viewPAGFile(row);
                }
            }
        }
    }
}
