import QtQuick 2.13
import QtQuick.Window 2.13
import "."

Window {
    default property alias contents: placeholder.data
    property bool isWindows: Qt.platform.os == 'windows'
    property bool isWinPlatform: Qt.platform.os == 'windows'
    property bool hasMenu: false
    property bool canResize: true
    property bool isMaximized: false
    property int windowedX: 0
    property int windowedY: 0
    property int windowedWidth: 0
    property int windowedHeight: 0

    property var windowBackground: "#00000000"
    property var windowBorderColor: "#00FFFFFF"
    property var titlebarBackground: "#22FFFFFF"
    property var titlebarVisible: true
    property var titleColor: "#DDDDDD"
    property var titleDividerColor: "black"
    property var titleFontSize: 12
    property var windowRadius: 5
    property var windowBorderWidth: 0
    property var minBtnVisible: true
    property var maxBtnVisible: true
    property var canMaxSize: true
    property var screenInit: false

    id: main
    visible: true
    width: 800
    height: 600
    color: windowBackground
    flags: isWindows ? (Qt.FramelessWindowHint | Qt.Window | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowStaysOnTopHint) : (Qt.WindowStaysOnTopHint | Qt.Window)

    Component.onCompleted: {
        if (!screenInit) {
            main.x = screen.width / 2 - main.width / 2
            main.y = screen.height / 2 - main.height / 2
            screenInit = true
        }
    }


    Rectangle {
        id: rect
        visible: isWindows && titlebarVisible
        anchors.fill: parent
        radius: windowRadius
        clip: true
        border.width: windowBorderWidth
        border.color: windowBorderColor
        color: windowBackground


        RectangleWithRadius {
            id: titleBar

            height: 32
            color: titlebarBackground
            radius: 5
            leftTop: !isMaximized
            rightTop: !isMaximized
            rightBottom: false
            leftBottom: false
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 0

            MouseArea {
                id: ma
                anchors.fill: parent
                property int dx
                property int dy
                property int wx
                property int wy
                property int ww
                height: 40
                onPressed: { dx = mouseX; dy = mouseY; wx = main.x; wy = main.y; ww = main.width; }
                onPositionChanged: {
                    var offsetX = mouseX - dx;
                    var offsetY = mouseY - dy;

                    //don't update position when dbclick to set main.visibility
                    if ((wx - main.x) === offsetX && (wy - main.y) === offsetY) return;

                    if (isMaximized) {
                        setMaximized(false);
                        var sourceX = (dx - 240) / (ww - 240 - 120);
                        var targetX = (main.width - 240 - 120) * sourceX + 240;
                        var xChange = dx - targetX;
                        main.x += xChange;
                        dx = targetX;
                        console.log("ww:", ww, " width:", main.width);
                    }

                    if (offsetX !== 0) main.x += offsetX;
                    if (offsetY !== 0) main.y += offsetY;
                    ensureMinSize()
                }
                onDoubleClicked: setMaximized(!isMaximized)
            }

            Text {
                property var maxWidth: (btnMin.x - parent.width / 2 - 5) * 2
                property var titleInfo: main.title

                id: txtTitle
                visible: true
                y: 8
                height: 16
                color: titleColor
                text: titleInfo
                width: maxWidth
                verticalAlignment: Text.AlignVCenter
                font.weight: Font.Bold
                renderType: Text.NativeRendering
                elide: Text.ElideMiddle
                anchors.left: parent.left
                anchors.right: parent.right
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: titleFontSize
                font.family: "PingFang SC"
            }

            Image {
                id: imgIcon
                x: 10
                y: 8
                width: 16
                height: 16
                fillMode: Image.PreserveAspectFit
                source: "qrc:/images/window-icon-32x.png"
            }

            RectangleWithRadius {
                id: btnClose
                width: 40
                height: 31
                color: mAClose.containsMouse ? "#FFDD0000" : "#00ffffff"
                border.width: 0
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.right: parent.right
                anchors.rightMargin: 0
                radius: titleBar.radius
                leftTop: false
                leftBottom: false
                rightBottom: false

                Image {
                    id: imgClose
                    x: 18
                    y: 10
                    width: 10
                    height: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    sourceSize.height: 24
                    sourceSize.width: 24
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/images/close-dark.png"
                }

                MouseArea {
                    id: mAClose
                    x: 15
                    y: 11
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: {
                        main.hide()
                        main.close()
                    }
                }
            }

            Rectangle {
                id: btnZoom
                width: 40
                height: 31
                color: mAZoom.containsMouse ? "#33FFFFFF" : "#00ffffff"
                border.width: 0
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.right: btnClose.left
                visible: maxBtnVisible

                Image {
                    id: imgZoom
                    x: 18
                    y: 10
                    width: 10
                    height: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    sourceSize.height: 24
                    sourceSize.width: 24
                    fillMode: Image.PreserveAspectFit
                    source: isMaximized ? "qrc:/images/restore-dark.png" : "qrc:/images/maximize-dark.png"
                }

                MouseArea {
                    id: mAZoom
                    x: -22
                    y: 10
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: setMaximized(!isMaximized)
                }
            }

            Rectangle {
                id: btnMin
                width: 40
                height: 31
                color: mAMin.containsMouse ? "#33FFFFFF" : "#00ffffff"
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.right: btnZoom.left
                visible: minBtnVisible

                Image {
                    id: imgMin
                    x: 18
                    y: 10
                    width: 10
                    height: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    sourceSize.height: 24
                    sourceSize.width: 24
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/images/minimize-dark.png"
                }

                MouseArea {
                    id: mAMin
                    x: -62
                    y: 10
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: {
                        main.visibility = Window.Minimized;
                        isMaximized = false;
                    }
                }
            }

            Rectangle {
                height: 1
                width: parent.width
                color: titleDividerColor
                anchors.bottom: parent.bottom
            }

        }
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: isWindows ? 32 : 0
        anchors.leftMargin: isWindows ? 1 : 0
        anchors.rightMargin: isWindows ? 1 : 0
        anchors.bottomMargin: isWindows ? 1 : 0
        clip: true
        id: placeholder    // <-- where the placeholder should be inserted
    }


    MouseArea {
        id: rightHandle
        enabled: isWindows && canResize
        visible: enabled
        property int dx
        property int dy
        width: 4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 0
        cursorShape: Qt.SizeHorCursor
        onPressed: { dx = mouseX; dy = mouseY }
        onPositionChanged: {
            updateWidth(mouseX - dx)
            ensureMinSize()
        }
    }

    MouseArea {
        id: leftHandle
        enabled: isWindows && canResize
        visible: enabled
        property int dx
        property int dy
        width: 4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 0
        cursorShape: Qt.SizeHorCursor
        onPressed: { dx = mouseX; dy = mouseY }
        onPositionChanged: {
            updateWidth(-(mouseX - dx))
            main.x += mouseX - dx
            ensureMinSize()
        }
    }

    MouseArea {
        id: bottomHandle
        enabled: isWindows && canResize
        visible: enabled
        property int dx
        property int dy
        height: 4
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        cursorShape: Qt.SizeVerCursor
        onPressed: { dx = mouseX; dy = mouseY }
        onPositionChanged: {
            updateHeight(mouseY - dy)
            ensureMinSize()
        }
    }

    MouseArea {
        id: topHandle
        enabled: isWindows && canResize
        visible: enabled
        property int dx
        property int dy
        height: 4
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 0
        cursorShape: Qt.SizeVerCursor
        onPressed: { dx = mouseX; dy = mouseY }
        onPositionChanged: {
            updateHeight(-(mouseY - dy))
            main.y += mouseY - dy
            ensureMinSize()
        }
    }

    MouseArea {
        id: leftTopHandle
        enabled: isWindows && canResize
        visible: enabled
        property int dx
        property int dy
        height: 4
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 0
        width: 4
        cursorShape: Qt.SizeFDiagCursor
        onPressed: { dx = mouseX; dy = mouseY }
        onPositionChanged: {
            main.x += mouseX - dx
            main.y += mouseY - dy
            updateWidth(-(mouseX - dx))
            updateHeight(-(mouseY - dy))
            ensureMinSize()
        }
    }


    MouseArea {
        id: rightTopHandle
        enabled: isWindows && canResize
        visible: enabled
        property int dx
        property int dy
        height: 4
        anchors.top: parent.top
        anchors.topMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        width: 4
        cursorShape: Qt.SizeBDiagCursor
        onPressed: { dx = mouseX; dy = mouseY }
        onPositionChanged: {
            main.y += mouseY - dy
            updateWidth(mouseX - dx)
            updateHeight(-(mouseY - dy))
            ensureMinSize()
        }
    }

    MouseArea {
        id: leftBottomHandle
        enabled: isWindows && canResize
        visible: enabled
        property int dx
        property int dy
        height: 4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        width: 4
        cursorShape: Qt.SizeBDiagCursor
        onPressed: { dx = mouseX; dy = mouseY }
        onPositionChanged: {
            main.x += mouseX - dx
            updateWidth(-(mouseX - dx));
            updateHeight(mouseY - dy)
            ensureMinSize()
        }
    }

    MouseArea {
        id: rightBottomHandle
        enabled: isWindows && canResize
        visible: enabled
        property int dx
        property int dy
        height: 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        width: 5
        cursorShape: Qt.SizeFDiagCursor
        onPressed: { dx = mouseX; dy = mouseY }
        onPositionChanged: {
            updateWidth(mouseX - dx)
            updateHeight(mouseY - dy)
            ensureMinSize()
        }
    }

    function updateWidth(offset) {
        var width = main.width;
        var target = width + offset;
        target = Math.max(main.minimumWidth, target);
        main.width = target;
    }


    function updateHeight(offset) {
        var height = main.height;
        var target = height + offset;
        target = Math.max(main.minimumHeight, target);
        main.height = target;
    }

    function ensureMinSize() {

        if ((main.x + main.width) < 10) {
            main.x = 10 - main.width;
        }

        if (main.y < 0) {
            main.y = 0;
        }
    }

    function setMaximized(maximized) {
        if (!canMaxSize && maximized) {
            return
        }
        isMaximized = maximized;

        var useFake = isWindows && Screen.height === Screen.desktopAvailableHeight && Screen.width === Screen.desktopAvailableWidth

        console.log("useFake:", useFake);

        if (!useFake) {
            main.visibility = maximized ? Window.Maximized : Window.AutomaticVisibility;
            return;
        }

        if (maximized) {

            windowedX = main.x;
            windowedY = main.y;
            windowedWidth = main.width;
            windowedHeight = main.height;
            main.visibility = Window.AutomaticVisibility;
            main.y = 0;
            main.x = 0;
            main.width = Screen.width
            main.height = Screen.desktopAvailableHeight - 1;
        } else {
            main.visibility = Window.AutomaticVisibility;
            main.x = windowedX;
            main.y = windowedY;
            main.width = windowedWidth;
            main.height = windowedHeight;

        }
    }

    onVisibleChanged: {
        if (visible && typeof (windowHelper) != "undefined") {
            windowHelper.setupWindowStyle(main);
        }
    }
}




















































