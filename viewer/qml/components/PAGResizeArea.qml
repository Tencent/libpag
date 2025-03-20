import QtQuick

MouseArea {
    id: mouseArea
    required property bool isEnable
    required property int direction
    property int startX: 0

    property int startY: 0

    readonly property int directionNone: 0

    readonly property int directionLeft: 1

    readonly property int directionRight: 2

    readonly property int directionTop: 4

    readonly property int directionBottom: 8
    enabled: isEnable
    visible: isEnable
    width: (direction & directionLeft) || (direction & directionRight) ? 4 : undefined
    height: (direction & directionTop) || (direction & directionBottom) ? 4 : undefined
    anchors.top: (direction & directionBottom) ? undefined : parent.top
    anchors.topMargin: 0
    anchors.bottom: (direction & directionTop) ? undefined : parent.bottom
    anchors.bottomMargin: 0
    anchors.left: (direction & directionRight) ? undefined : parent.left
    anchors.leftMargin: (direction & directionLeft) || (direction & directionRight) ? 0 : 5
    anchors.right: (direction & directionLeft) ? undefined : parent.right
    anchors.rightMargin: (direction & directionLeft) || (direction & directionRight) ? 0 : 5
    cursorShape: {
        if (direction === directionTop || direction === directionBottom || direction === directionLeft || direction === directionRight) {
            return Qt.SizeVerCursor;
        } else if (((direction & directionLeft) && (direction & directionTop)) || ((direction & directionRight) && (direction & directionBottom))) {
            return Qt.SizeFDiagCursor;
        } else {
            return Qt.SizeBDiagCursor;
        }
    }
    onPressed: {
        startX = mouseX;
        startY = mouseY;
    }
}
