import QtQuick 2.0

Rectangle {
    id: root
    property bool leftTop: true
    property bool leftBottom: true
    property bool rightTop: true
    property bool rightBottom: true

    Rectangle {
        id: rleftTop
        visible: !leftTop
        width: root.radius+3
        height: root.radius+3
        radius: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 0
        color: root.color
    }
    Rectangle {
        id: rrightTop
        visible: !rightTop
        width: root.radius+3
        height: root.radius+3
        radius: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 0
        color: root.color
    }
    Rectangle {
        id: rleftBottom
        visible: !leftBottom
        width: root.radius+3
        height: root.radius+3
        radius: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        color: root.color
    }
    Rectangle {
        id: rrightBottom
        visible: !rightBottom
        width: root.radius+3
        height: root.radius+3
        radius: 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        color: root.color
    }
}

/*##^## Designer {
    D{i:0;autoSize:true;height:480;width:640}
}
 ##^##*/
