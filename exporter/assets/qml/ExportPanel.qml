import QtQuick
import QtQuick.Window
import QtQuick.Controls

PAGWindow {
    id: window

    property int windowWidth: 800

    property int windowHeight: 800

    title: qsTr("Export PAG")
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

    Rectangle {
        id: compositionsContainer
        width: parent.width
        height: 440
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "#22222c"
        clip: true
        radius: 1

        Column {
            width: parent.width
            height: parent.height

            property alias compositionTableView: compositionTableView

            Rectangle {
                id: header
                width: parent.width
                height: 35
                clip: true
                color: "#22222c"

                Row {
                    id: headerRow
                    anchors.fill: parent
                    spacing: 20
                    leftPadding: 20

                    Image {
                        id: titleCheckBox
                        width: 20
                        height: 20
                        anchors.verticalCenter: parent.verticalCenter
                        source: {
                            if (compositionsModel === null) {
                                return "";
                            }

                            return compositionsModel.allSelected ? "qrc:/images/checkbox-on.png" : "qrc:/images/checkbox-off.png";
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (compositionsModel !== null) {
                                    compositionsModel.setAllSelected(!compositionsModel.allSelected);
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: serachBar
                        width: parent.width - parent.leftPadding - titleCheckBox.width - savePathColumn.width - settingColumn.width - previewColumn.width - 4 * parent.spacing
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        radius: 2
                        color: "#323341"

                        Image {
                            id: searchIcon
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 9
                            anchors.left: parent.left
                            width: 20
                            height: 20
                            source: "qrc:/images/export-search.png"
                        }

                        Text {
                            id: hintText
                            text: qsTr("Search Composition")
                            color: "#8c97b6"
                            font.pixelSize: 16
                            font.family: "PingFang SC"
                            anchors.left: searchIcon.right
                            anchors.leftMargin: 9
                            anchors.right: parent.right
                            anchors.rightMargin: 5
                            anchors.verticalCenter: parent.verticalCenter
                            renderType: Text.NativeRendering
                            visible: true
                        }

                        TextInput {
                            id: searchText
                            text: ""
                            color: "white"
                            font.pixelSize: 16
                            font.family: "PingFang SC"
                            anchors.left: searchIcon.right
                            anchors.leftMargin: 9
                            anchors.right: parent.right
                            anchors.rightMargin: 5
                            anchors.verticalCenter: parent.verticalCenter
                            renderType: Text.NativeRendering
                            selectByMouse: true
                            clip: true
                            visible: false

                            Keys.onReturnPressed: {
                                focus = false;
                            }

                            Keys.onEnterPressed: {
                                focus = false;
                            }

                            onEditingFinished: {
                                if (compositionsModel !== null) {
                                    compositionsModel.setSerachText(searchText.text);
                                }
                            }

                            onFocusChanged: function (focus) {
                                if (focus || (searchText.text.length === 0)) {
                                    serachBar.handleFocus(focus);
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            cursorShape: Qt.IBeamCursor
                            onPressed: {
                                searchText.focus = true;
                                searchText.forceActiveFocus();
                                serachBar.handleFocus(true);
                            }
                        }

                        function handleFocus(focus) {
                            hintText.visible = !focus;
                            searchText.visible = focus;
                        }
                    }

                    Rectangle {
                        id: savePathColumn
                        width: 300
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        color: "transparent"

                        Text {
                            id: savePathText
                            text: qsTr("Storage Path")
                            color: "#7c7c81"
                            font.pixelSize: 16
                            font.family: "PingFang SC"
                            anchors.verticalCenter: parent.verticalCenter
                            renderType: Text.NativeRendering
                        }
                    }

                    Rectangle {
                        id: settingColumn
                        width: 72
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        color: "transparent"

                        Text {
                            id: settingText
                            text: qsTr("Setting")
                            color: "#7c7c81"
                            font.pixelSize: 16
                            font.family: "PingFang SC"
                            anchors.centerIn: parent
                            renderType: Text.NativeRendering
                        }
                    }

                    Rectangle {
                        id: previewColumn
                        width: 72
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        color: "transparent"

                        Text {
                            id: previewText
                            text: qsTr("Preview")
                            color: "#7c7c81"
                            font.pixelSize: 16
                            font.family: "PingFang SC"
                            anchors.centerIn: parent
                            renderType: Text.NativeRendering
                        }
                    }
                }
            }

            Rectangle {
                id: headerDivider
                width: parent.width
                height: 1
                color: "#000000"
            }

            CompositionListView {
                id: compositionTableView
                width: parent.width
                height: parent.height - header.height - headerDivider.height
                model: compositionsModel === null ? null : compositionsModel
                parentWindow: window
            }
        }
    }

    Rectangle {
        id: tableViewDivider
        width: parent.width
        height: 8
        anchors.top: compositionsContainer.bottom
        color: "transparent"
    }

    Rectangle {
        id: errorInfoContainer
        width: parent.width
        height: 100
        anchors.top: tableViewDivider.bottom
        anchors.bottom: errorInfoDivider.top
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "#22222c"
        radius: 2
        clip: true

        Text {
            id: noErrorInfoTip
            text: qsTr("No Error Message Found")
            font.pixelSize: 15
            font.family: "PingFang SC"
            color: "#FFFFFF"
            visible: (typeof alertInfoModel !== 'undefined' && alertInfoModel) ? alertInfoModel.count <= 0 : true
            anchors.centerIn: parent
        }

        PAGListView {
            id: errorInfoListView
            visible: (typeof alertInfoModel !== 'undefined' && alertInfoModel) ? alertInfoModel.count > 0 : false
            anchors.fill: parent
            model: (typeof alertInfoModel !== 'undefined') ? alertInfoModel : null
            showLocationBtn: true
        }
    }

    Rectangle {
        id: errorInfoDivider
        width: parent.width
        height: 8
        anchors.bottom: audioExportContainer.top
        color: "transparent"
    }

    Rectangle {
        id: audioExportContainer
        width: parent.width
        height: 30
        anchors.bottom: exportImageDivider.top
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "transparent"

        Image {
            id: audioExportIcon

            width: 20
            height: 20
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            source: {
                if (compositionsModel === null) {
                    return "";
                }
                return compositionsModel.exportAudio ? "qrc:/images/checkbox-on.png" : "qrc:/images/checkbox-off.png";
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    if (compositionsModel !== null) {
                        compositionsModel.exportAudio = !compositionsModel.exportAudio;
                    }
                }
            }
        }

        Text {
            id: audioExportText
            text: qsTr("Also Export Audio")
            font.pixelSize: 16
            font.family: "PingFang SC"
            color: "#FFFFFF"
            anchors.left: audioExportIcon.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        id: exportImageDivider
        width: parent.width
        height: 8
        anchors.bottom: buttonContainer.top
        color: "transparent"
    }

    Rectangle {
        id: buttonContainer
        width: parent.width
        height: 40
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "transparent"

        Rectangle {
            id: cancelButton
            width: 120
            height: parent.height
            color: "#282832"
            radius: 2
            anchors.right: exportButton.left
            anchors.rightMargin: 12

            Text {
                text: qsTr("Cancel")
                font.pixelSize: 16
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
                    window.close();
                }
            }
        }

        Rectangle {
            id: exportButton

            width: 120
            height: parent.height
            enabled: compositionsModel !== null && compositionsModel.canExport
            color: "#1982EB"
            radius: 2
            anchors.right: parent.right
            opacity: enabled ? 1.0 : 0.3

            Text {
                text: qsTr("Export")
                font.pixelSize: 16
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
                    let component = Qt.createComponent("qrc:/qml/ExportCompositionsProgress.qml");
                    if (component.status === Component.Ready) {
                        let progressListWindow = component.createObject(this, {});
                        if (progressListWindow) {
                            progressListWindow.closing.connect(function () {
                                progressListWindow.close();
                                window.show();
                            });
                            window.hide();
                            progressListWindow.show();
                        }
                    }
                    if (compositionsModel !== null) {
                        compositionsModel.exportSelectedCompositions();
                    }
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: function (event) {
            window.close();
            event.accepted = true;
        }
    }

    Timer {
        id: raiseTimer
        interval: 200
        running: true
        repeat: true
        onTriggered: {
            if (!window.active && window.visible && exportingPanelWindow.isAEWindowActive()) {
                window.raise();
                window.flags |= Qt.WindowStaysOnTopHint;
            } else {
                window.flags &= ~Qt.WindowStaysOnTopHint;
            }
        }
    }

    onClosing: function (closeEvent) {
        closeEvent.accepted = true;
        exportingPanelWindow.onWindowClosing();
    }

    onVisibleChanged: function (visible) {
        if (visible) {
            raiseTimer.start();
        } else {
            raiseTimer.stop();
        }
    }
}
