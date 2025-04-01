import QtCore
import QtQuick
import QtQuick.Controls

MenuBar {
    id: baseMenuBar
    property int barHeight: 32
    property int itemHeight: 30
    property int itemWidth: 30
    height: barHeight
    contentWidth: itemWidth
    contentHeight: itemHeight

    delegate: MenuBarItem {
        id: menuBarItem
        topPadding: (baseMenuBar.height - baseMenuBar.contentHeight) / 2
        bottomPadding: (baseMenuBar.height - baseMenuBar.contentHeight) / 2

        hoverEnabled: true

        contentItem: Text {
            text: menuBarItem.text
            font.pixelSize: 12
            font.family: "Microsoft Yahei"
            opacity: enabled ? 1.0 : 0.3
            color: menuBarItem.highlighted ? "#FFFFFF" : "#9B9B9B"
            renderType: Text.NativeRendering
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            implicitWidth: baseMenuBar.contentWidth
            implicitHeight: baseMenuBar.contentHeight
            opacity: enabled ? 1 : 0.3
            color: menuBarItem.highlighted ? "#2E436E" : "#00FFFFFF"
        }
    }

    background: Rectangle {
        implicitWidth: baseMenuBar.contentHeight
        implicitHeight: baseMenuBar.contentHeight
        color: "#00000000"
    }
}
