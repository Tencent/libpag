import QtQuick
import QtQuick.Controls

Menu {
    id: pagMenu
    required property int menuWidth
    property int itemHeight: 30
    background: Rectangle {
        implicitWidth: menuWidth
        implicitHeight: pagMenu.contentHeight + 8
        color: "#20202A"
        radius: 4
        border.color: "#918F8F"
        border.width: 1
    }

    delegate: MenuItem {
        id: menuItem
        implicitHeight: itemHeight

        contentItem: Item {
            Text {
                text: menuItem.text
                font.pixelSize: 12
                font.family: "Microsoft Yahei"
                opacity: menuItem.enabled ? 1.0 : 0.3
                color: "#DFE1E5"
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                text: shortcut.nativeText
                font.pixelSize: 12
                font.family: "Microsoft Yahei"
                opacity: menuItem.enabled ? 1.0 : 0.3
                anchors.right: parent.right
                color: "#DFE1E5"
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            anchors.top: parent.top
            anchors.topMargin: 4
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.right: parent.right
            anchors.rightMargin: 4
            implicitHeight: 30
            color: menuItem.highlighted ? "#2E436E" : "#00FFFFFF"
            radius: 2
        }

        Shortcut {
            id: shortcut
            enabled: menuItem.action ? menuItem.action.enabled : false
            sequence: menuItem.action.shortcut
        }
    }
}
