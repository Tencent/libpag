import QtQuick
import QtQuick.Controls.Universal
import QtQuick.Window
import QtQuick.Layouts

PAGWindow {
    id: mainWindow
    property int windowWidth: 1070
    property int windowHeight: 650

    title: "Optimization Suggestions"
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

    property var alertDataModel: (typeof alertModel !== 'undefined') ? alertModel : null

    property QtObject model: QtObject {
        function viewAllRules() {
            console.log("View All Validation Rules clicked");
            console.log("Alert count:", (alertDataModel ? alertDataModel.rowCount() : 0));
            if(alertDataModel){
                alertDataModel.jumpToUrl();
            }
        }

        function continueExport() {
            console.log("Continue Exporting clicked");
            console.log("Continuing export with", (alertDataModel ? alertDataModel.rowCount() : 0), "alerts");
            //mainWindow.close();
        }

        function cancelAndModify() {
            mainWindow.close();
        }

    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "transparent"

            RowLayout {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 5
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
                    text: "Total of " + (alertDataModel ? alertDataModel.rowCount() : 0) + " items, please refer below optimization suggestion and re-export:"
                    color: "#cccccc"
                    font.pixelSize: 16
                    font.family: "PingFang SC"
                }

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredWidth: Math.max(180, rulesButtonText.implicitWidth + 20)
                    Layout.preferredHeight: 35
                    color: rulesMouseArea.pressed ? "#4a4a50" : "#383840"
                    border.color: "#555555"
                    border.width: 1
                    radius: 6
                    Layout.alignment: Qt.AlignRight

                    Text {
                        id: rulesButtonText
                        text: "Click to View All Validation Rules"
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
                // fontSize: 18
                // fontFamily: "PingFang SC"
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
            Layout.preferredHeight: 80
            color: "transparent"

            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 5
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
                        text: "Continue Exporting"
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
                            if (mainWindow.model) {
                                mainWindow.model.continueExport();
                            }
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
                        text: "Cancel and Modify"
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
                                mainWindow.model.cancelAndModify();
                            }
                        }
                    }
                }
            }
        }
    }
}
