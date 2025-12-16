import QtQuick
import QtQuick.Controls.Universal
import QtQuick.Window
import QtQuick.Layouts

PAGWindow {
    id: mainWindow
    property int windowWidth: 500
    property int windowHeight: 300

    title: qsTr("Export Error")
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    visible: true
    isWindows: true
    canResize: false
    canMaxSize: false
    maxBtnVisible: false
    minBtnVisible: false
    resizeHandleSize: 5
    titleBarHeight: 32
    windowBackgroundColor: "#14141E"
    titlebarBackgroundColor: "#14141E"
    titleFontSize: 14
    modality: Qt.ApplicationModal

    onClosing: function (closeEvent) {
        closeEvent.accepted = false;
        alertWindow.onWindowClosing();
    }

    property QtObject model: QtObject {
        function cancelAndModify() {
            mainWindow.close();
        }
    }

    property string errorMessage: alertInfoModel ? alertInfoModel.errorMessage : ""
    property var alertDataModel: alertInfoModel ? alertInfoModel : null
    property string displayErrorText: {
        if (errorMessage && errorMessage.length > 0) {
            return errorMessage;
        }
        if (alertDataModel && alertDataModel.count > 0) {
            var infos = alertDataModel.getAlertInfos();
            var messages = [];
            for (var i = 0; i < infos.length; i++) {
                if (infos[i].errorInfo) {
                    messages.push(infos[i].errorInfo);
                }
            }
            return messages.join("\n\n");
        }
        return "";
    }

    Connections {
        target: alertInfoModel
        function onErrorMessageChanged() {
            mainWindow.errorMessage = alertInfoModel ? alertInfoModel.errorMessage : ""
        }
        function onAlertInfoChanged() {
            mainWindow.errorMessage = alertInfoModel ? alertInfoModel.errorMessage : ""
        }
    }

    function setErrorMessage(message) {
        if (alertInfoModel) {
            alertInfoModel.errorMessage = message;
        }
    }

    function showSimpleError(message) {
        setErrorMessage(message);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 15

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "transparent"

            RowLayout {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 5
                spacing: 8

                Image {
                    id: errorIcon
                    width: 20
                    height: 20
                    sourceSize.width: 20
                    sourceSize.height: 20
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/images/export-wrong.png"
                }

                Text {
                    text: qsTr("Export failed due to error:")
                    color: "#cccccc"
                    font.pixelSize: 16
                    font.family: "PingFang SC"
                }
            }
        }

        PAGRectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#14141E"
            radius: 8
            border.width: 0

            ErrorTextArea {
                id: errorTextArea
                anchors.fill: parent
                anchors.margins: 15
                text: displayErrorText
                targetListView: null
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: "transparent"

            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 5
                spacing: 15

                Rectangle {
                    Layout.preferredWidth: Math.max(140, cancelButtonText.implicitWidth + 20)
                    Layout.preferredHeight: 35
                    color: cancelMouseArea.pressed ? "#1465b8" : "#1982EB"
                    border.color: "#1982EB"
                    border.width: 1
                    radius: 6

                    Text {
                        id: cancelButtonText
                        text: qsTr("Cancel and Modify")
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.family: "PingFang SC"
                        font.bold: true
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: cancelMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            if (mainWindow.model) {
                                alertWindow.cancelAndModify();
                                mainWindow.close();
                            }
                        }
                    }
                }
            }
        }
    }
}
