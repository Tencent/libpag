import QtQuick
import QtQuick.Controls
import "components"

PAGWindow {
    id: settingsWindow

    property bool useEnglish: true

    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    hasMenu: false
    canResize: false

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

                Item {
                    width: 1
                    height: (parent.height - useEnglishCheckBox.height) / 2
                }

                CheckBox {
                    id: useEnglishCheckBox
                    checked: settingsWindow.useEnglish
                    height: 30
                    text: qsTr("Use English - Take Effect After Restart")
                    scale: 0.6
                    font.pixelSize: 22
                    padding: 0
                    spacing: 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.horizontalCenterOffset: 20 - (parent.width / 4)
                    focusPolicy: Qt.ClickFocus
                    display: AbstractButton.TextBesideIcon
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
                        settingsWindow.useEnglish = useEnglishCheckBox.checked
                    }
                }
            }

            Item {
                width: 1
                height: (parent.height - useEnglishCheckBox.height) / 2
            }
        }
    }
}