import QtQuick
import QtQuick.Controls
import Qt.labs.platform

Popup {
    id: root

    enum DialogType {
        Center,
        Left,
        Right
    }

    property int dialogType: PerformanceWarnDialog.DialogType.Center
    property string warnString1: ""
    property string warnString2: ""
    property string warnString3: ""

    width: 200
    height: 200
    modal: true
    focus: true
    background: rect
    closePolicy: Popup.OnEscape

    Overlay.modal: Rectangle {
        color: "transparent"
    }

    onOpened: {
        text3.visible = warnString3 !== ""
        text3Backgroud.visible = warnString3 !== ""

        if (warnString3 === "") {
            root.height = 17 * 3 + text1.paintedHeight + text2.paintedHeight
        } else {
            root.height = 17 * 3 + text1.paintedHeight + text2.paintedHeight + text3.paintedHeight + 17 + 10
        }
    }

    BorderImage {
        id: rect

        anchors.fill: parent
        source: {
            switch (dialogType) {
                case PerformanceWarnDialog.DialogType.Left:
                    return "qrc:/image/warn-bg2.png"
                case PerformanceWarnDialog.DialogType.Right:
                    return "qrc:/image/warn-bg3.png"
                case PerformanceWarnDialog.DialogType.Center:
                    return "qrc:/image/warn-bg.png"
                default:
                    return "qrc:/image/warn-bg.png"
            }
        }

        border {
            left: dialogType === PerformanceWarnDialog.DialogType.Center ? 120 : 50
            top: dialogType === PerformanceWarnDialog.DialogType.Center ? 50 : 120
            right: 20
            bottom: 20
        }

        Text {
            id: text1

            text: warnString1
            font.pixelSize: 12
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            color: "#FFFFFF"
            anchors.top: parent.top
            anchors.topMargin: dialogType === PerformanceWarnDialog.DialogType.Center ? 17 : 10
            anchors.left: parent.left
            anchors.leftMargin: 17 + (dialogType !== PerformanceWarnDialog.DialogType.Center ? 3 : 0)
            anchors.right: parent.right
            anchors.rightMargin: 17
        }

        Rectangle {
            id: text2Backgroud

            implicitWidth: 100
            implicitHeight: text2.paintedHeight + 9 * 2
            radius: 2
            border.color: "#2F2E39"
            border.width: 1
            color: "#2F2E39"
            anchors.top: text1.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.leftMargin: 8 + (dialogType !== PerformanceWarnDialog.DialogType.Center ? 3 : 0)
            anchors.right: parent.right
            anchors.rightMargin: 8
        }

        Text {
            id: text2

            text: warnString2
            font.pixelSize: 12
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            color: "#FFFFFF"
            anchors.top: text1.bottom
            anchors.topMargin: 17
            anchors.left: parent.left
            anchors.leftMargin: 17 + (dialogType !== PerformanceWarnDialog.DialogType.Center ? 3 : 0)
            anchors.right: parent.right
            anchors.rightMargin: 17 + (dialogType === PerformanceWarnDialog.DialogType.Right ? 3 : 0)
        }

        Rectangle {
            id: text3Backgroud

            implicitWidth: 100
            implicitHeight: text3.paintedHeight + 9 * 2
            radius: 2
            color: "#2F2E39"
            border.color: "#2F2E39"
            border.width: 1
            anchors.top: text2.bottom
            anchors.topMargin: 8 + 10
            anchors.left: parent.left
            anchors.leftMargin: 8 + (dialogType !== PerformanceWarnDialog.DialogType.Center ? 3 : 0)
            anchors.right: parent.right
            anchors.rightMargin: 8 + (dialogType === PerformanceWarnDialog.DialogType.Right ? 3 : 0)
        }

        Text {
            id: text3

            text: warnString3
            font.pixelSize: 12
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            color: "#FFFFFF"
            anchors.top: text2.bottom
            anchors.topMargin: 17 + 10
            anchors.left: parent.left
            anchors.leftMargin: 17 + (dialogType !== PerformanceWarnDialog.DialogType.Center ? 3 : 0)
            anchors.right: parent.right
            anchors.rightMargin: 17 + (dialogType === PerformanceWarnDialog.DialogType.Right ? 3 : 0)
        }
    }
}
