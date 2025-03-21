import QtQuick

Rectangle {
    id: root
    property bool leftTopRadius: true

    property bool leftBottomRadius: true

    property bool rightTopRadius: true

    property bool rightBottomRadius: true

    Component {
        id: cornerRect
        Rectangle {
            radius: 0
            width: root.radius + 3
            height: root.radius + 3
            color: root.color
        }
    }
    Loader {
        sourceComponent: cornerRect
        active: !leftTopRadius

        anchors {
            left: parent.left
            top: parent.top
        }
    }
    Loader {
        sourceComponent: cornerRect
        active: !rightTopRadius

        anchors {
            right: parent.right
            top: parent.top
        }
    }
    Loader {
        sourceComponent: cornerRect
        active: !leftBottomRadius

        anchors {
            left: parent.left
            bottom: parent.bottom
        }
    }
    Loader {
        sourceComponent: cornerRect
        active: !rightBottomRadius

        anchors {
            right: parent.right
            bottom: parent.bottom
        }
    }
}
