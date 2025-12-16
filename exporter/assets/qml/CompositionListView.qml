import QtQuick
import QtQuick.Controls
import Qt.labs.platform as Platform

ListView {
    id: compositionTableView

    property var parentWindow: null

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
                visible: compositionTableView.ScrollBar.vertical.size < 1.0
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
                visible: compositionTableView.ScrollBar.horizontal.size < 1.0
            }
        }
    }

    delegate: Item {
        required property var row

        required property var index

        required property string name

        required property string savePath

        required property bool isFolder

        required property bool isSelected

        required property bool isUnfold

        required property int level

        height: 35
        implicitWidth: ListView.view.width

        Rectangle {
            anchors.fill: parent
            color: (row % 2 === 0) ? "#22222B" : "#27272F"

            Row {
                anchors.fill: parent
                spacing: 20
                leftPadding: 20

                Image {
                    id: selectedColumn
                    width: 20
                    height: 20
                    anchors.verticalCenter: parent.verticalCenter
                    visible: true
                    source: isFolder ? "" : (isSelected ? "qrc:/images/checkbox-on.png" : "qrc:/images/checkbox-off.png")

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Qt.callLater(function () {
                                model.setIsSelected(row, !isSelected);
                            });
                        }
                    }
                }

                Rectangle {
                    width: parent.width - parent.leftPadding - selectedColumn.width - savePathColumn.width - settingColumn.width - previewColumn.width - 4 * parent.spacing
                    height: parent.height
                    color: "transparent"

                    Image {
                        id: unfoldIcon
                        width: 8
                        height: 8
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: level * 20 - (isFolder ? 8 : 0)
                        visible: isFolder
                        source: isUnfold ? "qrc:/images/folder-unfold.png" : "qrc:/images/folder-fold.png"
                    }

                    Image {
                        id: icon
                        width: 20
                        height: 20
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: unfoldIcon.right
                        anchors.leftMargin: isFolder ? 8 : 0
                        source: isFolder ? "qrc:/images/folder.png" : "qrc:/images/bmp.png"
                    }

                    Text {
                        id: nameText
                        text: name
                        font.pixelSize: 14
                        font.family: "PingFang SC"
                        elide: Text.ElideRight
                        anchors.left: icon.right
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#FFFFFF"
                    }

                    MouseArea {
                        visible: isFolder
                        height: parent.height
                        anchors.left: parent.left
                        anchors.right: nameText.right
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            model.setIsUnfold(row, !isUnfold);
                        }
                    }
                }

                Rectangle {
                    id: savePathColumn
                    width: 300
                    height: parent.height
                    color: "transparent"

                    Text {
                        text: savePath
                        width: parent.width
                        font.pixelSize: 13
                        font.family: "PingFang SC"
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#8E96B3"

                        MouseArea {
                            enabled: !isFolder
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                openFolderDialog.row = row;
                                openFolderDialog.folder = pathToFileUrl(savePath);
                                openFolderDialog.open();
                            }
                        }
                    }
                }

                Rectangle {
                    id: settingColumn

                    property var subWindow: null

                    width: 72
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    color: "transparent"

                    Image {
                        width: 20
                        height: 20
                        anchors.centerIn: parent
                        visible: !isFolder
                        source: "qrc:/images/setting.png"

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor

                            onClicked: {
                                let component = Qt.createComponent("qrc:/qml/CompositionSettingPage.qml");
                                if (component.status === Component.Ready) {
                                    exportingPanelWindow.updateCompositionSetting(row);
                                    let backgroundColor = exportingPanelWindow.getBackgroundColor(row);
                                    let textLayerModel = exportingPanelWindow.getTextLayerModel(row);
                                    let imageLayerModel = exportingPanelWindow.getImageLayerModel(row);
                                    let timeStretchModel = exportingPanelWindow.getTimeStretchModel(row);
                                    let compositionInfoModel = exportingPanelWindow.getCompositionInfoModel(row);
                                    settingColumn.subWindow = component.createObject(parentWindow, {
                                        "backgroundColor": backgroundColor,
                                        "compositionName": name,
                                        "textLayerModel": textLayerModel,
                                        "imageLayerModel": imageLayerModel,
                                        "timeStretchModel": timeStretchModel,
                                        "compositionInfoModel": compositionInfoModel
                                    });
                                    if (settingColumn.subWindow) {
                                        settingColumn.subWindow.closing.connect(function () {
                                            Qt.callLater(function() {
                                                if (settingColumn.subWindow && !settingColumn.subWindow.destroyed) {
                                                    if(settingColumn.subWindow.close){
                                                        settingColumn.subWindow.close();
                                                    }
                                                    settingColumn.subWindow.destroy();
                                                    settingColumn.subWindow = null;
                                                    if (compositionsModel !== null) {
                                                        compositionsModel.updateNames();
                                                    }
                                                }
                                            });
                                        });
                                        settingColumn.subWindow.show();
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: previewColumn
                    width: 72
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    color: "transparent"

                    Image {
                        width: 20
                        height: 20
                        anchors.centerIn: parent
                        visible: !isFolder
                        source: "qrc:/images/preview.png"

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (compositionsModel !== null) {
                                    compositionsModel.prepareForPreview(row);
                                }
                                let component = Qt.createComponent("qrc:/qml/ExportCompositionProgress.qml");
                                if (component.status === Component.Ready) {
                                    let progressWindow = component.createObject(parentWindow, {});
                                    if (progressWindow) {
                                        progressWindow.closing.connect(function () {
                                            Qt.callLater(function () {
                                                if (progressWindow && !progressWindow.destroyed) {
                                                    progressWindow.destroy();
                                                }
                                            });
                                            parentWindow.show();
                                        });
                                        progressWindow.show();
                                        parentWindow.hide();
                                    }
                                }
                                if (compositionsModel !== null) {
                                    compositionsModel.previewComposition(row);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Platform.FolderDialog {
        id: openFolderDialog

        property int row: -1

        visible: false
        title: qsTr("Select Save Path")
        onAccepted: {
            compositionTableView.model.setSavePath(row, openFolderDialog.folder);
        }
    }

    function pathToFileUrl(path) {
        let url = path;
        if (url.startsWith("file://")) {
            return url;
        }
        if (Qt.platform.os === "windows") {
            return "file:///" + url.replace(/\\/g, "/");
        } else {
            return "file://" + url;
        }
    }
}
