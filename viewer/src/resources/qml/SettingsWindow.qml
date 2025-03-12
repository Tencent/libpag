import QtQuick
import QtQuick.Controls

PApplicationWindow {
    id:  settingsWindow

    property bool useBeta: false
    property bool useEnglish: false
    property bool autoCheckUpdate: true
    property bool showVideoFrames: true

    maximumWidth:width
    maximumHeight: height
    minimumWidth:width
    minimumHeight: height
    title: qsTr("Settings")
    canResize: false

    RectangleWithRadius {
        id: rectangle

        color: "#2D2D37"
        anchors.fill: parent
        leftTop: false
        rightTop: false
        radius: 5

        Image {
            id: image

            x: 36
            y: 100
            width: 96
            height: 96
            sourceSize.width: 192
            sourceSize.height: 192
            anchors.verticalCenterOffset: 0
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            source: "qrc:/image/appicon.png"
        }

        Item {
            id: element

            height: 120
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 161
            anchors.right: parent.right
            anchors.rightMargin: 29

            CheckBox {
                id: checkBoxShowImageFrames

                x: -77
                y: -4
                width: 387
                height: 40
                text: qsTr("Show Video Sequence Frames Which in the File")
                font.pixelSize: 22
                focusPolicy: Qt.ClickFocus
                scale: 0.6
                display: AbstractButton.TextBesideIcon
                checked: settingsWindow.showVideoFrames

                contentItem: Text {
                    text: checkBoxShowImageFrames.text
                    font: checkBoxShowImageFrames.font
                    verticalAlignment: Text.AlignVCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 42
                    color: "#FFFFFF"
                }

                onCheckedChanged: {
                    settingsWindow.showVideoFrames = checkBoxShowImageFrames.checked
                }
            }

            CheckBox {
                id: checkBoxUseEnglish

                x: -77
                y: 86
                width: 387
                height: 40
                text: qsTr("Use English -Take Effect After Restart")
                font.pixelSize: 22
                checked: settingsWindow.useEnglish
                scale: 0.6
                focusPolicy: Qt.ClickFocus
                display: AbstractButton.TextBesideIcon

                contentItem: Text {
                    text: parent.text
                    anchors.left: parent.left
                    anchors.leftMargin: 42
                    font: parent.font
                    color: "#FFFFFF"
                    verticalAlignment: Text.AlignVCenter
                }

                onCheckedChanged: {
                    settingsWindow.useEnglish = checkBoxUseEnglish.checked
                }
            }

            CheckBox {
                id: checkBoxUseBeta

                x: -77
                y: 56
                width: 387
                height: 40
                text: qsTr("Use PAG and AE Export Plug-in in Beta Version")
                font.pixelSize: 22
                checked: settingsWindow.useBeta
                scale: 0.6
                focusPolicy: Qt.ClickFocus
                display: AbstractButton.TextBesideIcon

                contentItem: Text {
                    text: checkBoxUseBeta.text
                    font: checkBoxUseBeta.font
                    verticalAlignment: Text.AlignVCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 42
                    color: "#FFFFFF"
                }

                onCheckedChanged: {
                    settingsWindow.useBeta = checkBoxUseBeta.checked
                }
            }

            CheckBox {
                id: checkBoxAutoCheckUpdate

                x: -77
                y: 26
                width: 387
                height: 40
                text: qsTr("Check Updates Automatically")
                font.pixelSize: 22
                checked: settingsWindow.autoCheckUpdate
                scale: 0.6
                focusPolicy: Qt.ClickFocus
                display: AbstractButton.TextBesideIcon

                contentItem: Text {
                    text: checkBoxAutoCheckUpdate.text
                    anchors.left: parent.left
                    anchors.leftMargin: 42
                    font: checkBoxAutoCheckUpdate.font
                    color: "#FFFFFF"
                    verticalAlignment: Text.AlignVCenter
                }

                onCheckedChanged: {
                    settingsWindow.autoCheckUpdate = checkBoxAutoCheckUpdate.checked
                }
            }
        }
    }
}
