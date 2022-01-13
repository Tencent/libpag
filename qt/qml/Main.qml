import QtQuick 2.5
import QtQuick.Window 2.13
import PAG 1.0

Window {
    id: wind
    visible: true
    width: 600
    height: 800

    PAGView {
        id: pagView
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        objectName: "pagView"
    }
}
