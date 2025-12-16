import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls

T.ComboBox {
    id: comboBox

    property int itemHeight: height + 2

    implicitWidth: 120
    implicitHeight: 30
    spacing: 0
    leftPadding: padding
    rightPadding: padding + indicator.width + spacing
    font.pixelSize: 14
    font.family: "PingFang SC"

    contentItem: T.TextField {
        leftPadding: 10
        rightPadding: 10
        text: comboBox.displayText
        font.pixelSize: 14
        font.family: "PingFang SC"
        color: "white"
        verticalAlignment: Text.AlignVCenter
        selectByMouse: true
        selectedTextColor: "green"
        selectionColor: "white"
        clip: true
        enabled: comboBox.editable
        autoScroll: comboBox.editable
        readOnly: comboBox.down
        inputMethodHints: comboBox.inputMethodHints
        validator: comboBox.validator
        renderType: Text.NativeRendering
        background: Rectangle {
            visible: comboBox.enabled && comboBox.editable
            border.width: parent && parent.activeFocus ? 1 : 0
            border.color: "#383840"
            color: "transparent"
        }
    }

    background: Rectangle {
        implicitWidth: comboBox.implicitWidth
        implicitHeight: comboBox.implicitHeight
        color: "#383840"
        radius: 2
    }

    indicator: Image {
        id: theIndicator
        width: 16
        height: comboBox.height
        anchors.right: comboBox.right
        anchors.verticalCenter: comboBox.verticalCenter
        source: "qrc:/images/down-arrow.png"
    }

    delegate: ItemDelegate {
        width: comboBox.width
        height: comboBox.itemHeight
        padding: 0
        contentItem: Text {
            text: (comboBox.textRole ? (Array.isArray(comboBox.model) ? modelData[comboBox.textRole] : model[comboBox.textRole]) : modelData)
            color: "white"
            leftPadding: 10
            rightPadding: 10
            font.pixelSize: 14
            font.family: "PingFang SC"
            elide: Text.ElideRight
            renderType: Text.NativeRendering
            verticalAlignment: Text.AlignVCenter
        }
        hoverEnabled: comboBox.hoverEnabled
        background: Rectangle {
            radius: 2
            color: (comboBox.highlightedIndex === index) ? "#0063E1" : "#383840"
            Rectangle {
                height: 1
                width: parent.width - 4
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#383840"
            }
        }
    }

    popup: T.Popup {
        y: comboBox.height + 1
        width: comboBox.width
        implicitHeight: {
            if (comboBox.delegateModel) {
                return comboBox.delegateModel.count * comboBox.itemHeight + 2;
            }

            return 0;
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: comboBox.popup.visible ? comboBox.delegateModel : null
            currentIndex: comboBox.highlightedIndex
            snapMode: ListView.SnapToItem
        }
    }
}
