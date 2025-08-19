import QtQuick
import QtQuick.Controls
import "components"

PAGWindow {
    id: settingsWindow

    property bool autoCheckForUpdates: true

    property bool useBeta: true

    property bool useEnglish: true

    property int contentHeight: 30

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
            spacing: 0

            anchors.fill: parent

            Item {
                width: 30
                height: 1
            }

            Image {
                id: image
                width: 96
                height: 96
                sourceSize.width: 192
                sourceSize.height: 192
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
                spacing: 0
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width

                Item {
                    width: 1
                    height: (parent.height - settingsWindow.contentHeight * 3) / 2
                }

                CheckBox {
                    id: autoCheckForUpdatesCheckBox
                    checked: settingsWindow.autoCheckForUpdates
                    width: parent.width
                    height: settingsWindow.contentHeight
                    text: qsTr("Check For Updates Automatically")
                    scale: 0.6
                    font.pixelSize: 22
                    padding: 0
                    spacing: 0
                    anchors.left: parent.left
                    anchors.leftMargin: -80
                    focusPolicy: Qt.ClickFocus
                    display: AbstractButton.TextBesideIcon
                    indicator: Image {
                        height: 32
                        anchors.left: parent.left
                        source: parent.checked ? "qrc:/images/checked.png" : "qrc:/images/unchecked.png"
                    }
                    contentItem: Text {
                        text: parent.text
                        anchors.left: parent.indicator.right
                        anchors.leftMargin: 8
                        font: parent.font
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                    }
                    onCheckedChanged: {
                        if (settingsWindow.autoCheckForUpdates === autoCheckForUpdatesCheckBox.checked) {
                            return;
                        }
                        settingsWindow.autoCheckForUpdates = autoCheckForUpdatesCheckBox.checked;
                    }

                    Rectangle {
                        color: "#FFFFFF"
                    }
                }

                CheckBox {
                    id: useBetaCheckBox
                    checked: settingsWindow.useBeta
                    width: parent.width
                    height: settingsWindow.contentHeight
                    text: qsTr("Use PAG and AE Export Plugin in Beta Version")
                    scale: 0.6
                    font.pixelSize: 22
                    padding: 0
                    spacing: 0
                    anchors.left: parent.left
                    anchors.leftMargin: -80
                    focusPolicy: Qt.ClickFocus
                    display: AbstractButton.TextBesideIcon
                    indicator: Image {
                        height: 32
                        anchors.left: parent.left
                        source: parent.checked ? "qrc:/images/checked.png" : "qrc:/images/unchecked.png"
                    }
                    contentItem: Text {
                        text: parent.text
                        anchors.left: parent.indicator.right
                        anchors.leftMargin: 8
                        font: parent.font
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                    }
                    onCheckedChanged: {
                        if (settingsWindow.useBeta === useBetaCheckBox.checked) {
                            return;
                        }
                        settingsWindow.useBeta = useBetaCheckBox.checked;
                    }

                    Rectangle {
                        color: "#FFFFFF"
                    }
                }

                CheckBox {
                    id: useEnglishCheckBox
                    checked: settingsWindow.useEnglish
                    width: parent.width
                    height: settingsWindow.contentHeight
                    text: qsTr("Use English - Take Effect After Restart")
                    scale: 0.6
                    font.pixelSize: 22
                    padding: 0
                    spacing: 0
                    anchors.left: parent.left
                    anchors.leftMargin: -80
                    focusPolicy: Qt.ClickFocus
                    display: AbstractButton.TextBesideIcon
                    indicator: Image {
                        id: indicatorImg
                        height: 32
                        width: 32
                        anchors.left: parent.left
                        source: useEnglishCheckBox.checked ? "qrc:/images/checked.png" : "qrc:/images/unchecked.png"
                    }
                    contentItem: Text {
                        text: parent.text
                        anchors.left: parent.indicator.right
                        anchors.leftMargin: 8
                        font: parent.font
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                    }
                    onCheckedChanged: {
                        if (settingsWindow.useEnglish === useEnglishCheckBox.checked) {
                            return;
                        }
                        settingsWindow.useEnglish = useEnglishCheckBox.checked;
                    }
                }
            }

            Item {
                width: 1
                height: (parent.height - settingsWindow.contentHeight * 3) / 2
            }
        }
    }
}
