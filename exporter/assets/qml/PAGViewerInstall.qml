import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

PAGWindow {
    id: root
    width: 450
    height: 320
    title: qsTr("Install PAGViewer")
    modality: Qt.ApplicationModal
    isWindows: true
    canResize: false
    canMaxSize: false
    maxBtnVisible: false
    minBtnVisible: false
    titleBarHeight: 32
    windowBackgroundColor: "#14141E"
    titlebarBackgroundColor: "#14141E"
    titleFontSize: 14

    readonly property int confirmStage: 0
    readonly property int installingStage: 1
    readonly property int successStage: 2
    readonly property int failedStage: 3

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 25

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"

            Text {
                id: titleText
                anchors.centerIn: parent
                text: installModel.title
                font.pixelSize: 18
                font.family: "PingFang SC"
                font.bold: true
                font.underline: false
                font.strikeout: false
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.PlainText
            }
        }

        PAGRectangle {
            Layout.fillWidth: true
            Layout.fillHeight: false
            radius: 8

            Text {
                id: messageText
                anchors.centerIn: parent
                anchors.margins: 20
                text: installModel.message
                font.pixelSize: 14
                font.family: "PingFang SC"
                font.underline: false
                font.strikeout: false
                color: "#cccccc"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                width: parent.width - 40
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "transparent"

            RowLayout {
                anchors.centerIn: parent
                spacing: 15

                Rectangle {
                    Layout.preferredWidth: Math.max(80, cancelButtonText.implicitWidth + 20)
                    Layout.preferredHeight: 35
                    visible: installModel.showCancelButton
                    color: cancelMouseArea.pressed ? "#4a4a50" : "#383840"
                    border.color: "#555555"
                    border.width: 1
                    radius: 6

                    Text {
                        id: cancelButtonText
                        text: qsTr("Cancel")
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.family: "PingFang SC"
                        font.bold: true
                        font.underline: false
                        font.strikeout: false
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: cancelMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: installModel.onCancel()
                    }
                }

                Rectangle {
                    Layout.preferredWidth: Math.max(100, actionButtonText.implicitWidth + 20)
                    Layout.preferredHeight: 35
                    visible: installModel.buttonText !== ""
                    color: actionMouseArea.pressed ? "#1465b8" : "#1982EB"
                    border.color: "#1982EB"
                    border.width: 1
                    radius: 6

                    Text {
                        id: actionButtonText
                        text: installModel.buttonText
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.family: "PingFang SC"
                        font.bold: true
                        font.underline: false
                        font.strikeout: false
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: actionMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            switch (installModel.currentStage) {
                            case root.confirmStage:
                                installModel.onConfirmInstall();
                                break;
                            case root.successStage:
                                installModel.onComplete();
                                break;
                            case root.failedStage:
                                installModel.onConfirmInstall();
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}
