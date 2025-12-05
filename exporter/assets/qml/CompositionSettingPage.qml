import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

PAGWindow {
    id: window

    property int windowWidth: 660

    property int windowHeight: 700

    property string compositionName: ""

    required property string backgroundColor

    required property var textLayerModel

    required property var imageLayerModel

    required property var timeStretchModel

    required property var compositionInfoModel

    property alias tabBar: tabBar

    title: qsTr("Setting Panel") + " - " + compositionName
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    visible: true
    isWindows: true
    canResize: false
    canMaxSize: false
    maxBtnVisible: false
    minBtnVisible: false
    resizeHandleSize: 5
    titleBarHeight: 32
    windowBackgroundColor: "#14141E"
    titlebarBackgroundColor: "#14141E"
    titleFontSize: 14
    modality: Qt.WindowModal

    Rectangle {
        id: tabBarContainer
        width: parent.width
        height: 40
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "#22222c"
        clip: true
        radius: 1

        TabBar {
            id: tabBar
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8

            PAGTabButton {
                id: compositionTabButton
                text: qsTr("PreComposition")
                height: parent.height
            }

            PAGTabButton {
                id: textLayerTabButton
                text: qsTr("Text Layer")
                height: parent.height
            }

            PAGTabButton {
                id: placeHolderImagesTabButton
                text: qsTr("Placeholder Images")
                height: parent.height
            }

            PAGTabButton {
                id: timeStretchTabButton
                text: qsTr("Time Stretch")
                height: parent.height
            }
        }
    }

    Rectangle {
        id: tabBarDivider
        width: parent
        height: 1
        anchors.top: tabBarContainer.bottom
        color: "transparent"
    }

    Rectangle {
        id: contentContainer
        width: parent.width
        anchors.top: tabBarDivider.bottom
        anchors.bottom: contentDivider.top
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "#22222c"
        clip: true
        radius: 1

        StackLayout {
            id: stackLayout
            anchors.fill: parent
            currentIndex: tabBar.currentIndex

            PreCompositionItem {
                model: compositionInfoModel
                parentWindow: window
                backgroundColor: window.backgroundColor
            }

            TextLayerItem {
                model: textLayerModel
            }

            PlaceholderImageItem {
                model: imageLayerModel
            }

            TimeStretchItem {
                model: timeStretchModel
            }
        }
    }

    Rectangle {
        id: contentDivider
        width: parent
        height: 16
        anchors.bottom: buttonContainer.top
        color: "transparent"
    }

    Rectangle {
        id: buttonContainer
        width: parent.width
        height: 40
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 12
        color: "transparent"

        Rectangle {
            id: confirmButton

            width: 120
            height: parent.height
            color: "#1982EB"
            radius: 2
            anchors.right: parent.right

            Text {
                text: qsTr("Confirm")
                font.pixelSize: 16
                font.bold: true
                font.family: "PingFang SC"
                anchors.centerIn: parent
                color: "#FFFFFF"
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    window.close();
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: function (event) {
            window.close();
            event.accepted = true;
        }
    }
}
