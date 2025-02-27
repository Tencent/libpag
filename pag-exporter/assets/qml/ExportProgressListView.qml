import QtQuick 2.5
import QtQuick.Controls 2.3
import QtQuick.Window 2.13

Rectangle {
    property var isPreview: false
    property var normalIconSize: 20
    property var barHeight: 8
    property var barWidth: 100
    property var rightMargin: 8
    property var bgColor: Qt.rgba(1, 1, 1, 0)
    property var whiteTextColor: Qt.rgba(1, 1, 1, 1)
    property var errorInfoLineWords: 38

    property var statusWaiting: 0
    property var statusSuccess: 1
    property var statusError: 2

    property var errorIconPath : isPreview ? "../images/exprot_wrong.png" : "qrc:/images/exprot_wrong.png"
    property var successIconPath : isPreview ? "../images/export_success.png" : "qrc:/images/export_success.png"
    property var waitingIconPath : isPreview ? "../images/export_waiting.png" : "qrc:/images/export_waiting.png"
    

    id: root
    visible: true
    width: parent.width
    height: parent.height
    color: bgColor
    clip: true

    Component {
        id: progressItemDelegate
        Rectangle {
            id: wrapper
            width: parent.width
            height: itemHeight
            color: bgColor
            
            Image {
                id: statusIcon
                visible: !(statusCode === statusWaiting && progressValue > 0)
                anchors.left: parent.left
                anchors.top: parent.top
                width: normalIconSize
                height: normalIconSize
                source: root.getIconPath(statusCode + "")
            }

            Image {
                id: statusIconExporting
                visible: statusCode === statusWaiting && progressValue > 0
                anchors.left: parent.left
                anchors.top: parent.top
                width: normalIconSize
                height: normalIconSize
                source: waitingIconPath
                RotationAnimation on rotation {
                    running: statusCode === statusWaiting && progressValue > 0
                    from: 0
                    to: 360
                    duration: 1500
                    loops: Animation.Infinite
                }
            }

            Text {
                id: pagNameText
                height: 22
                color: whiteTextColor
                text: pagName
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.left: parent.left
                anchors.leftMargin: 30
                anchors.right: previewBtn.left
                anchors.rightMargin: barWidth + rightMargin + 10
                font.pixelSize: 16
                clip: true
                elide: Text.ElideMiddle
            }

            Text {
                id: errorInfoText
                visible: statusCode === statusError
                color: whiteTextColor
                text: errorInfo
                anchors.top: pagNameText.bottom
                anchors.topMargin: 2
                anchors.left: parent.left
                anchors.leftMargin: 30
                wrapMode: Text.WrapAnywhere
                width: 538
                height: textHeight
                font.pixelSize: 14
                clip: true
            }

            Rectangle{
                id: previewBtn
                visible: statusCode === statusSuccess
                width: 48
                height: 22
                color: "#1982EB"
                radius: 2
                anchors.right: parent.right
                anchors.rightMargin: rightMargin
                anchors.top: parent.top
                anchors.topMargin: 2

                Text{
                    font.pixelSize: 12
                    font.family: "PingFang SC"
                    anchors.centerIn: parent
                    text: qsTr("查看")
                    color: whiteTextColor
                }

                MouseArea{
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onPressed: {
                        progressListModel.handlePreviewIconClick(index);
                    }
                }
            }

            Rectangle{
                id: progressContainer
                visible: statusCode === statusWaiting
                anchors.right: parent.right
                anchors.rightMargin: rightMargin
                anchors.top: parent.top
                color: bgColor

                ProgressBar {
                    id: progressBar
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 7
                    from: 0.0
                    to: 1.0
                    value: progressValue
                    height: barHeight
                    width: barWidth

                    background: Rectangle {
                        visible: statusCode === statusWaiting
                        radius: barHeight / 2
                        color: Qt.rgba(1, 1, 1, 0.2)
                        border.width: 0
                        width: barWidth
                        height: barHeight
                    }

                    contentItem: Item {
                        Rectangle {
                            visible: progressValue != 0
                            width: progressValue * barWidth
                            height: barHeight
                            radius: barHeight / 2
                            color: "#4082E4"
                        }
                    }
                }

                Text {
                    id: progressText
                    height: 20
                    width: 32
                    color: whiteTextColor
                    text: (progressValue * 100).toFixed(0) + "%"
                    anchors.top: parent.top
                    anchors.right: progressBar.left
                    anchors.rightMargin: 6
                    font.pixelSize: 14
                    clip: false
                }
            }
        }
    }
        
    ListView {
        id: progressListView
        width: parent.width
        height: parent.height
        model: progressListModel
        delegate: progressItemDelegate

        ScrollBar.vertical: ScrollBar {
            visible: progressListView.contentHeight > progressListView.height
            hoverEnabled: true
            active: hovered || pressed
            orientation: Qt.Vertical
            size: parent.height / progressListView.height
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            policy: ScrollBar.AsNeeded
        
            contentItem: Rectangle {
                implicitWidth: 3
                implicitHeight: 100
                radius: 3
                color: "#535359"
            }
        }
    }

    function getIconPath(statusCode) {
        var iconPaths = {
            "0": waitingIconPath,
            "1": successIconPath,
            "2": errorIconPath,
        };
        return iconPaths[statusCode];
    }

}
