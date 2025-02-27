import PAG
import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: main

    default property alias contents: placeholder.data
    property bool isWindows: Qt.platform.os === 'windows'
    property bool hasMenu: false
    property bool canResize: true
    property bool isMaximized: false
    property int windowedX: 0
    property int windowedY: 0
    property int windowedWidth: 0
    property int windowedHeight: 0

    visible: true
    width: 800
    height: 600
    color: "#00000000"
    flags: isWindows ? (Qt.FramelessWindowHint | Qt.Window | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint) : Qt.Window

    Rectangle {
        id: rect

        visible: isWindows
        anchors.fill: parent
        radius: 5
        clip: true
        border.width: 0
        border.color: "#00FFFFFF"
        color: "#15FFFFFF"


        RectangleWithRadius {
            id: titleBar

            height: 32
            color: "#22FFFFFF"
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

            RectangleWithRadius {
                height: 31
                color: "#20202a"
                radius: 5
                leftTop:!isMaximized
                rightTop: !isMaximized
                rightBottom: false
                leftBottom: false
                anchors.right: parent.right
                anchors.rightMargin: 0
                anchors.left: parent.left
                anchors.leftMargin: 0
                anchors.top: parent.top
                anchors.topMargin: 1
            }

            MouseArea {
                id: mouseArea

                property int dx: 0
                property int dy: 0
                property int wx: 0
                property int wy: 0
                property int ww: 0

                height: 40
                anchors.fill: parent

                onPressed: {
                    dx = mouseX
                    dy = mouseY
                    wx = main.x
                    wy = main.y
                    ww = main.width
                }

                onPositionChanged: {
                    let offsetX = mouseX - dx
                    let offsetY =  mouseY - dy

                    if(((wx - main.x) === offsetX) && ((wy - main.y) === offsetY)) {
                        return
                    }

                    if(isMaximized){
                        setMaximized(false)
                        let sourceX = (dx - 240) / (ww - 240 - 120)
                        let targetX = (main.width - 240 - 120) * sourceX + 240
                        let xChange = dx - targetX
                        main.x += xChange
                        dx = targetX
                        console.log("ww:", ww, " width:", main.width)
                    }

                    main.x += offsetX
                    main.y += offsetY
                    ensureMinSize()
                }

                onDoubleClicked: {
                    setMaximized(!isMaximized)
                }
            }

            Text {
                id: txtTitle

                visible: main.width > (hasMenu ? 400 : 150)
                y: 8
                height: 16
                color: "#DDDDDD"
                text: main.title
                font.pixelSize: 12
                font.family: "Microsoft Yahei"
                font.weight: Font.Bold
                anchors.left: parent.left
                anchors.leftMargin: hasMenu ? (main.width < 600 ? 220 : 0) : 0
                anchors.right: parent.right
                anchors.rightMargin: hasMenu ? (main.width < 600 ? 120 : 0) : 0
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                renderType:Text.NativeRendering
            }

            Image {
                id: imgIcon

                x: 10
                y: 8
                width: 16
                height: 16
                fillMode: Image.PreserveAspectFit
                source: "qrc:/image/appicon-32x.png"
            }

            RectangleWithRadius {
                id: btnClose

                width: 40
                height: 31
                color: mouseAreaClose.containsMouse ? "#FFDD0000" : "#00ffffff"
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
                    source: "qrc:/image/close-dark.svg"
                }

                MouseArea {
                    id: mouseAreaClose

                    x: 15
                    y: 11
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: main.close()
                }
            }

            Rectangle {
                id: btnZoom

                width: 40
                height: 31
                color: mouseAreaZoom.containsMouse ? "#33FFFFFF" : "#00ffffff"
                border.width: 0
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.right: parent.right
                anchors.rightMargin: 40

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
                    source: isMaximized? "qrc:/image/restore-dark.svg" : "qrc:/image/maximize-dark.svg"
                }

                MouseArea {
                    id: mouseAreaZoom

                    x: -22
                    y: 10
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: {
                        setMaximized(!isMaximized)
                    }
                }
            }

            Rectangle {
                id: btnMin

                width: 40
                height: 31
                color: mouseAreaMin.containsMouse ? "#33FFFFFF" : "#00ffffff"
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.right: parent.right
                anchors.rightMargin: 80

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
                    source: "qrc:/image/minimize-dark.svg"
                }

                MouseArea {
                    id: mouseAreaMin

                    x: -62
                    y: 10
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked:  {
                        main.visibility = Window.Minimized
                        isMaximized = false
                    }
                }
            }

        }
    }

    Rectangle {
        id: macTitleBar

        height: 32
        color: "#20202a"
        visible: !isWindows
        anchors.top: parent.top
        anchors.topMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0

        Text {
            id: macTxtTitle

            y: 3
            height: 16
            color: "#DDDDDD"
            text: main.title
            verticalAlignment: Text.AlignVCenter
            font.weight: Font.Medium
            font.pixelSize: 12
            elide: Text.ElideRight
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0
            horizontalAlignment: Text.AlignHCenter
        }
    }


    Item {
        id: placeholder

        anchors.fill: parent
        anchors.topMargin: isWindows ? 32 :22
        anchors.leftMargin: isWindows ? 1 : 0
        anchors.rightMargin: isWindows ? 1 : 0
        anchors.bottomMargin: isWindows ? 1 : 0
        clip: true
    }

    MouseArea {
        id: rightHandle

        property int dx: 0
        property int dy: 0

        width: 4
        enabled: isWindows && canResize
        visible: enabled
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 0
        cursorShape: Qt.SizeHorCursor

        onPressed: {
            dx = mouseX
            dy = mouseY
        }

        onPositionChanged: {
            updateWidth(mouseX - dx)
            ensureMinSize()
        }
    }

    MouseArea {
        id: leftHandle

        property int dx: 0
        property int dy: 0

        width: 4
        enabled: isWindows && canResize
        visible: enabled
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 0
        cursorShape: Qt.SizeHorCursor
        onPressed: {
            dx = mouseX
            dy = mouseY
        }
        onPositionChanged: {
            updateWidth(-( mouseX - dx))
            main.x += (mouseX - dx)
            ensureMinSize()
        }
    }

    MouseArea {
        id: bottomHandle

        property int dx: 0
        property int dy: 0

        height: 4
        enabled: isWindows && canResize
        visible: enabled
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        cursorShape: Qt.SizeVerCursor

        onPressed: {
            dx = mouseX
            dy = mouseY
        }

        onPositionChanged: {
            updateHeight(mouseY - dy)
            ensureMinSize()
        }
    }

    MouseArea {
        id: topHandle

        property int dx: 0
        property int dy: 0

        height: 4
        enabled: isWindows && canResize
        visible: enabled
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 0
        cursorShape: Qt.SizeVerCursor

        onPressed: {
            dx = mouseX
            dy = mouseY
        }

        onPositionChanged: {
            updateHeight(-(mouseY - dy))
            main.y += (mouseY - dy)
            ensureMinSize()
        }
    }

    MouseArea {
        id: leftTopHandle

        property int dx: 0
        property int dy: 0

        width: 4
        height: 4
        enabled: isWindows && canResize
        visible: enabled
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 0
        cursorShape: Qt.SizeFDiagCursor

        onPressed: {
            dx = mouseX
            dy = mouseY
        }

        onPositionChanged: {
            main.x += (mouseX - dx)
            main.y += (mouseY - dy)
            updateWidth(-(mouseX - dx))
            updateHeight(-(mouseY - dy))
            ensureMinSize()
        }
    }


    MouseArea {
        id: rightTopHandle

        property int dx: 0
        property int dy: 0

        width: 4
        height: 4
        enabled: isWindows && canResize
        visible: enabled
        anchors.top: parent.top
        anchors.topMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        cursorShape: Qt.SizeBDiagCursor

        onPressed: {
            dx = mouseX
            dy = mouseY
        }

        onPositionChanged: {
            main.y += (mouseY - dy)
            updateWidth(mouseX - dx)
            updateHeight(-(mouseY - dy))
            ensureMinSize()
        }
    }

    MouseArea {
        id: leftBottomHandle

        property int dx: 0
        property int dy: 0

        width: 4
        height: 4
        enabled: isWindows && canResize
        visible: enabled
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        cursorShape: Qt.SizeBDiagCursor

        onPressed: {
            dx = mouseX
            dy = mouseY
        }

        onPositionChanged: {
            main.x += (mouseX - dx)
            updateWidth(- (mouseX - dx))
            updateHeight( mouseY - dy)
            ensureMinSize()
        }
    }

    MouseArea {
        id: rightBottomHandle

        property int dx: 0
        property int dy: 0

        width: 5
        height: 5
        enabled: isWindows && canResize
        visible: enabled
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        cursorShape: Qt.SizeFDiagCursor

        onPressed: {
            dx = mouseX
            dy = mouseY
        }

        onPositionChanged: {
            updateWidth(mouseX - dx)
            updateHeight(mouseY - dy)
            ensureMinSize()
        }
    }

    function updateWidth(offset) {
        let width = main.width
        let target = width + offset
        target = Math.max(main.minimumWidth, target)
        main.width = target
    }


    function updateHeight(offset) {
        let height = main.height
        let target = height + offset
        target = Math.max(main.minimumHeight,target)
        main.height = target
    }

    function ensureMinSize() {
        if((main.x + main.width ) < 10){
            main.x = 10 - main.width
        }

        if(main.y < 0){
            main.y = 0
        }
    }

    function setMaximized(maximized) {
        isMaximized = maximized

        let useFake = (Qt.platform.os === 'windows')
            && (Screen.height === Screen.desktopAvailableHeight)
            && (Screen.width === Screen.desktopAvailableWidth)
            && isWindows

        console.log("useFake:", useFake)

        if(!useFake) {
            main.visibility = maximized ? Window.Maximized : Window.AutomaticVisibility
            return
        }

        if(maximized){
            windowedX = main.x
            windowedY = main.y
            windowedWidth = main.width
            windowedHeight = main.height
            main.visibility = Window.AutomaticVisibility
            main.y = 0
            main.x = 0
            main.width = Screen.width
            main.height = Screen.desktopAvailableHeight - 1
        }
        else {
            main.visibility = Window.AutomaticVisibility
            main.x = windowedX
            main.y = windowedY
            main.width = windowedWidth
            main.height = windowedHeight

        }
    }

    onVisibleChanged: {
        if(visible && typeof(windowHelper) != "undefined"){
            windowHelper.setupWindowStyle(main)
        }
    }
}