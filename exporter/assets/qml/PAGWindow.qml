import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: window

    default property alias contents: placeholder.data

    property bool isWindows: Qt.platform.os === "windows"

    property bool isMaximized: false

    property bool canResize: true

    property bool canMaxSize: true

    property bool minBtnVisible: true

    property bool maxBtnVisible: true

    property bool titleBarVisible: true

    property int windowLastX: 0

    property int windowLastY: 0

    property int windowLastWidth: 0

    property int windowLastHeight: 0

    property int titleBarHeight: 32

    property int resizeHandleSize: 5

    property int titleFontSize: 12

    property string windowBackgroundColor: "#00000000"

    property string windowBorderColor: "#00FFFFFF"

    property string titleColor: "#DDDDDD"

    property string titleDividerColor: "black"

    property string titlebarBackgroundColor: "#22FFFFFF"

    visible: true
    width: 800
    height: 600
    color: windowBackgroundColor
    flags: isWindows ? (Qt.FramelessWindowHint | Qt.Window | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint) : Qt.Window

    Loader {
        active: window.isWindows && window.canResize
        anchors.fill: parent
        sourceComponent: Component {
            MouseArea {
                enabled: window.canResize
                anchors.fill: parent
                hoverEnabled: window.canResize
                cursorShape: {
                    const pos = Qt.point(mouseX, mouseY);
                    const offset = resizeHandleSize;
                    if ((pos.x < offset) && (pos.y >= (height - offset))) {
                        return Qt.SizeBDiagCursor;
                    }
                    if ((pos.x < offset) && (pos.y < offset)) {
                        return Qt.SizeFDiagCursor;
                    }
                    if ((pos.x >= (width - offset)) && (pos.y >= (height - offset))) {
                        return Qt.SizeFDiagCursor;
                    }
                    if ((pos.x < offset) || ((pos.x >= (width - offset)) && (pos.y > titleBarHeight))) {
                        return Qt.SizeHorCursor;
                    }
                    if ((pos.y > (height - offset)) || ((pos.y < offset) && (pos.x < (width - 120)))) {
                        return Qt.SizeVerCursor;
                    }
                }
                acceptedButtons: Qt.NoButton
            }
        }
    }

    Loader {
        id: windowsControlsLoader
        active: window.isWindows && window.titleBarVisible
        anchors.fill: parent

        sourceComponent: Component {
            Rectangle {
                z: 1
                anchors.fill: parent
                radius: 5
                clip: true
                border.width: 0
                border.color: window.windowBorderColor
                color: window.windowBackgroundColor

                PAGRectangle {
                    id: titleBar
                    height: window.titleBarHeight
                    color: window.titlebarBackgroundColor
                    radius: 5
                    leftTopRadius: !window.isMaximized
                    rightTopRadius: !window.isMaximized
                    rightBottomRadius: false
                    leftBottomRadius: false
                    anchors.top: parent.top
                    anchors.topMargin: 0
                    anchors.left: parent.left
                    anchors.leftMargin: 1
                    anchors.right: parent.right
                    anchors.rightMargin: 1

                    MouseArea {
                        id: mouseArea
                        property int mouseLastX: 0

                        property int mouseLastY: 0

                        property int windowX: 0

                        property int windowY: 0

                        property int windowWidth: 0
                        height: 40
                        anchors.fill: parent
                        anchors.topMargin: resizeHandleSize
                        onPressed: {
                            mouseLastX = mouseX;
                            mouseLastY = mouseY;
                            windowX = window.x;
                            windowY = window.y;
                            windowWidth = window.width;
                        }
                        onPositionChanged: {
                            let offsetX = mouseX - mouseLastX;
                            let offsetY = mouseY - mouseLastY;
                            if (((windowX - window.x) === offsetX) && ((windowY - window.y) === offsetY)) {
                                return;
                            }
                            if (window.isMaximized) {
                                setMaximized(false);
                                let sourceX = (mouseLastX - 240) / (windowWidth - 240 - 120);
                                let targetX = (window.width - 240 - 120) * sourceX + 240;
                                let xChange = mouseLastX - targetX;
                                window.x += xChange;
                                mouseLastX = targetX;
                            }
                            if (offsetX !== 0) {
                                window.x += offsetX;
                            }
                            if (offsetY !== 0) {
                                window.y += offsetY;
                            }
                            ensurePositionMovable();
                        }
                        onDoubleClicked: {
                            setMaximized(!window.isMaximized);
                        }
                    }

                    Text {
                        id: windowsTitleText
                        visible: true
                        y: 8
                        width: 100
                        height: 16
                        color: window.titleColor
                        text: window.title
                        verticalAlignment: Text.AlignVCenter
                        font.weight: Font.Bold
                        renderType: Text.NativeRendering
                        elide: Text.ElideRight
                        anchors.left: parent.left
                        anchors.right: parent.right
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: window.titleFontSize
                        font.family: "PingFang SC"
                    }

                    Image {
                        id: iconImage
                        x: 10
                        y: 8
                        width: 16
                        height: 16
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/images/window-icon-32x.png"
                    }

                    Row {
                        id: windowControlRow
                        anchors.right: parent.right

                        PAGRectangle {
                            id: minButton
                            width: 40
                            height: window.titleBarHeight - 1
                            border.width: 0
                            color: minMouseArea.containsMouse ? "#33FFFFFF" : "#00FFFFFF"
                            visible: window.minBtnVisible

                            Image {
                                id: minImage
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
                                id: minMouseArea
                                hoverEnabled: true
                                anchors.fill: parent
                                onClicked: {
                                    window.visibility = Window.Minimized;
                                    window.isMaximized = false;
                                }
                            }
                        }

                        PAGRectangle {
                            id: zoomButton
                            width: 40
                            height: window.titleBarHeight - 1
                            color: zoomMouseArea.containsMouse ? "#33FFFFFF" : "#00FFFFFF"
                            border.width: 0
                            visible: window.maxBtnVisible

                            Image {
                                id: zoomImage
                                width: 10
                                height: 10
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                sourceSize.height: 24
                                sourceSize.width: 24
                                fillMode: Image.PreserveAspectFit
                                source: window.isMaximized ? "qrc:/images/restore-dark.png" : "qrc:/images/maximize-dark.png"
                            }
                            MouseArea {
                                id: zoomMouseArea
                                hoverEnabled: true
                                anchors.fill: parent
                                onClicked: {
                                    setMaximized(!window.isMaximized);
                                }
                            }
                        }

                        PAGRectangle {
                            id: closeButton
                            width: 40
                            height: window.titleBarHeight - 1
                            color: closeMouseArea.containsMouse ? "#FFDD0000" : "#00FFFFFF"
                            border.width: 0
                            radius: titleBar.radius
                            leftTopRadius: false
                            leftBottomRadius: false
                            rightBottomRadius: false

                            Image {
                                id: closeImage
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
                                id: closeMouseArea
                                hoverEnabled: true
                                anchors.fill: parent
                                onClicked: {
                                    window.close();
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: window.titleDividerColor
                        anchors.bottom: parent.bottom
                    }
                }

                DragHandler {
                    id: resizeHandler
                    enabled: window.canResize
                    target: null
                    grabPermissions: TapHandler.TakeOverForbidden
                    onActiveChanged: {
                        if (active) {
                            const pos = resizeHandler.centroid.position;
                            const offset = resizeHandleSize + 10;
                            let edges = 0;
                            if (pos.x < offset) {
                                edges |= Qt.LeftEdge;
                            }
                            if (pos.x >= (width - offset)) {
                                edges += Qt.RightEdge;
                            }
                            if (pos.y < offset) {
                                edges |= Qt.TopEdge;
                            }
                            if (pos.y >= (height - offset)) {
                                edges |= Qt.BottomEdge;
                            }
                            window.startSystemResize(edges);
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: macTitleLoader
        active: !window.isWindows
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: window.titleBarHeight
        sourceComponent: Component {
            Rectangle {
                anchors.fill: parent
                color: "#20202a"

                Text {
                    id: macTitleText
                    height: 16
                    anchors.fill: parent
                    color: "#DDDDDD"
                    text: window.title
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    renderType: Text.NativeRendering
                }
            }
        }
    }

    Item {
        id: placeholder
        anchors.fill: parent
        anchors.topMargin: window.isWindows ? 32 : 22
        anchors.leftMargin: window.isWindows ? 1 : 0
        anchors.rightMargin: window.isWindows ? 1 : 0
        anchors.bottomMargin: window.isWindows ? 1 : 0
        clip: true
    }

    function setMaximized(maximized) {
        if (!canMaxSize) {
            return;
        }
        isMaximized = maximized;
        let useFake = isWindows && Screen.height === Screen.desktopAvailableHeight && Screen.width === Screen.desktopAvailableWidth && isWindows;
        if (!useFake) {
            window.visibility = maximized ? Window.Maximized : Window.AutomaticVisibility;
            return;
        }
        if (maximized) {
            windowLastX = window.x;
            windowLastY = window.y;
            windowLastWidth = window.width;
            windowLastHeight = window.height;
            window.visibility = Window.AutomaticVisibility;
            window.y = 0;
            window.x = 0;
            window.width = Screen.width;
            window.height = Screen.desktopAvailableHeight - 1;
        } else {
            window.visibility = Window.AutomaticVisibility;
            window.x = windowLastX;
            window.y = windowLastY;
            window.width = windowLastWidth;
            window.height = windowLastHeight;
        }
    }
    function ensurePositionMovable() {
        if ((window.x + window.width) < 10) {
            window.x = 10 - window.width;
        }
        if (window.y < 0) {
            window.y = 0;
        }
    }
}
