import QtQuick

Rectangle {
    id: root

    property bool leftTop: true
    property bool leftBottom: true
    property bool rightTop: true
    property bool rightBottom: true

    Rectangle {
        id: rleftTop

        radius: 0
        width: root.radius + 3
        height: root.radius + 3
        visible: !leftTop
        color: root.color
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 0
    }

    Rectangle {
        id: rrightTop

        radius: 0
        width: root.radius + 3
        height: root.radius + 3
        visible: !rightTop
        color: root.color
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 0
    }

    Rectangle {
        id: rleftBottom

        radius: 0
        width: root.radius + 3
        height: root.radius + 3
        visible: !leftBottom
        color: root.color
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
    }

    Rectangle {
        id: rrightBottom

        radius: 0
        width: root.radius + 3
        height: root.radius + 3
        visible: !rightBottom
        color: root.color
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
    }
}
