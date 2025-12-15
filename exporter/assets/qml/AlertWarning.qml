import QtQuick
import QtQuick.Controls.Universal
import QtQuick.Window
import QtQuick.Layouts

PAGWindow {
    id: mainWindow
    property int windowWidth: 1070
    property int windowHeight: 650

    title: qsTr("Optimization Suggestions")
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

    property var alertDataModel: (typeof alertInfoModel !== 'undefined') ? alertInfoModel : null

    property QtObject model: QtObject {
        function viewAllRules() {
            if (alertDataModel) {
                alertDataModel.JumpToUrl();
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 15
        anchors.rightMargin: 15
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "transparent"

            RowLayout {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                width: parent.width

                Image {
                    id: warningIcon
                    width: 20
                    height: 20
                    sourceSize.width: 20
                    sourceSize.height: 20
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/images/tip.png"
                }

                Text {
                    text: qsTr("Total of %1 items, please refer below optimization suggestion and re-export:").arg(alertDataModel ? alertDataModel.rowCount() : 0)
                    color: "#cccccc"
                    font.pixelSize: 16
                    font.family: "PingFang SC"
                }

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredWidth: Math.max(180, rulesButtonText.implicitWidth + 20)
                    Layout.preferredHeight: 30
                    color: rulesMouseArea.pressed ? "#4a4a50" : "#383840"
                    border.color: "#555555"
                    border.width: 1
                    radius: 6
                    Layout.alignment: Qt.AlignRight

                    Text {
                        id: rulesButtonText
                        text: qsTr("Click to View All Validation Rules")
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.family: "PingFang SC"
                        font.bold: true
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: rulesMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            if (mainWindow.model) {
                                mainWindow.model.viewAllRules();
                            }
                        }
                    }
                }
            }
        }

        PAGRectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#22222c"
            radius: 8
            border.width: 0

            PAGListView {
                id: warningListView
                anchors.fill: parent

                isPreview: true
                normalColor: "#22222c"
                secondRowColor: "#272730"
                showLocationBtn: false

                model: alertDataModel

                onItemClicked: function (index, modelData) {
                    if (alertDataModel) {
                        alertDataModel.setData(alertDataModel.index(index, 0), !modelData.isFold, alertDataModel.IsFoldRole);
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: "transparent"

            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 15

                Rectangle {
                    Layout.preferredWidth: Math.max(140, continueButtonText.implicitWidth + 20)
                    Layout.preferredHeight: 35
                    color: continueMouseArea.pressed ? "#4a4a50" : "#383840"
                    border.color: "#555555"
                    border.width: 1
                    radius: 6

                    Text {
                        id: continueButtonText
                        text: qsTr("Continue Exporting")
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.family: "PingFang SC"
                        font.bold: true
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: continueMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            alertWindow.continueExport();
                            mainWindow.close();
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: Math.max(120, cancelButtonText.implicitWidth + 20)
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
                            alertWindow.cancelAndModify();
                            mainWindow.close();
                        }
                    }
                }
            }
        }
    }
}
