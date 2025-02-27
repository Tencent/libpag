import QtQuick
import QtQuick.Controls

Item {
    property bool hasPAGFile: false
    property bool isPlaying: true
    property bool lastPlayStatus: false
    property bool hasNewVersion: false
    property double progress: 0
    property double duration: 0

    property alias btnNext: btnNext
    property alias btnPlay: btnPlay
    property alias btnPause: btnPause
    property alias btnUpdate: btnUpdate
    property alias btnPanels: btnPanels
    property alias btnPrevious: btnPrevious
    property alias btnBackground: btnBackground
    property alias sliderProgress: sliderProgress
    property alias txtProgress: txtProgress
    property alias txtTotalFrame: txtTotalFrame
    property alias txtCurrentFrame: txtCurrentFrame
    property alias txtDurationInSecond: txtDurationInSecond

    width: 400
    height: 400

    RectangleWithRadius {
        id: rectangle

        height: 76
        radius: 5
        leftTop: false
        rightTop: false
        color: "#20202A"
        anchors.fill: parent
        rightBottom: !btnPanels.checked
    }

    Slider {
        id: sliderProgress

        z: 1
        height: 12
        padding: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: -4
        enabled: hasPAGFile

        background: Rectangle {
            x: sliderProgress.leftPadding
            y: sliderProgress.topPadding + (sliderProgress.availableHeight / 2) - (height / 2)
            implicitWidth: 200
            implicitHeight: 4
            width: sliderProgress.availableWidth
            height: implicitHeight
            radius: 0
            color: "#7D7D7D"

            Rectangle {
                width: sliderProgress.visualPosition * parent.width
                height: parent.height
                color: "#448EF9"
                radius: 0
            }
        }

        handle: Rectangle {
            x: sliderProgress.leftPadding + (sliderProgress.visualPosition * (sliderProgress.availableWidth - width))
            y: sliderProgress.topPadding + (sliderProgress.availableHeight / 2) - (height / 2)
            implicitWidth: 12
            implicitHeight: 12
            radius: 12
            color: sliderProgress.pressed ? "#f0f0f0" : "#f6f6f6"
            border.color: "#bdbebf"
        }
    }

    Rectangle {
        id: rectangle1

        x: 404
        y: -42
        width: 36
        height: 98
        color: "#313131"
        radius: 18
        visible: false
        z: 3
        anchors.right: parent.right
        anchors.rightMargin: 160
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20

        Slider {
            id: sliderVolume

            x: 8
            width: 28
            wheelEnabled: true
            // from: 0
            padding: 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 30
            anchors.top: parent.top
            anchors.topMargin: 12
            stepSize: 0.05
            to: 1
            font.pointSize: 10
            orientation: Qt.Vertical
            value: 0
            enabled: hasPAGFile

            background: Rectangle {
                id: rectangle2

                x: sliderVolume.leftPadding + (sliderVolume.availableWidth / 2) - (width / 2)
                y: sliderVolume.topPadding
                implicitWidth: 4
                implicitHeight: 54
                width: implicitWidth
                height: sliderVolume.availableHeight
                radius: 2
                color: "#000000"

                Rectangle {
                    width: parent.width
                    height: (1 - sliderVolume.visualPosition) * parent.height
                    radius: 2
                    color: "#4A90E2"
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 0
                }
            }

            handle: Rectangle {
                x: sliderVolume.leftPadding + (sliderVolume.availableWidth / 2) - (width / 2)
                y: sliderVolume.topPadding + sliderVolume.visualPosition * (sliderVolume.availableHeight - height)
                implicitWidth: 10
                implicitHeight: 10
                radius: 3
                color: sliderVolume.pressed ? "#f0f0f0" : "#f6f6f6"
                border.color: "#bdbebf"
            }
        }

        Image {
            id: image

            x: -4
            y: 60
            width: 44
            height: 44
            scale: 0.7
            source: "qrc:/image/volume.png"
            fillMode: Image.PreserveAspectFit
        }
    }

    CheckBox {
        id: btnUpdate

        x: 446
        y: 16
        width: 44
        height: 44
        enabled: hasNewVersion
        visible: hasNewVersion
        text: qsTr("更新")
        focusPolicy: Qt.NoFocus
        display: AbstractButton.TextOnly
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 110
        anchors.right: parent.right
        opacity: mouseArea.pressed ? 0.5 : 1.0

        indicator: Image {
            height: 44
            width: 44
            source: "qrc:/image/update-red.png"
        }

        MouseArea {
            id: mouseArea

            property bool entered: false

            hoverEnabled: true
            anchors.fill: parent

            ToolTip {
                visible: parent.entered
                text: qsTr("发现新版本，点击更新")
            }

            onEntered: {
                entered = true
            }
            onExited: {
                entered = false
            }
            onClicked: {
                checkUpdate(false)
            }
        }
    }

    CheckBox {
        id: btnBackground

        x: 492
        y: 16
        width: 44
        height: 44
        enabled: hasPAGFile
        text: qsTr("背景")
        focusPolicy: Qt.NoFocus
        display: AbstractButton.TextOnly
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 64
        anchors.right: parent.right

        indicator: Image {
            width: 44
            height: 44
            source: btnBackground.checked ? "qrc:/image/background-on.png" : "qrc:/image/background.png"
        }
    }

    CheckBox {
        id: btnPanels

        x: 538
        y: 16
        width: 44
        height: 44
        enabled: hasPAGFile
        text: qsTr("Panels")
        focusPolicy: Qt.NoFocus
        display: AbstractButton.TextOnly
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter

        indicator: Image {
            width: 44
            height: 44
            source: btnPanels.checked ? "qrc:/image/panels-on.png" : "qrc:/image/panels.png"
        }
    }

    Button {
        id: btnVolume

        x: 446
        y: 16
        width: 44
        height: 44
        text: qsTr("Volume")
        focusPolicy: Qt.NoFocus
        display: AbstractButton.IconOnly
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 110
        anchors.right: parent.right
        anchors.verticalCenterOffset: 0
        visible: false
        enabled: hasPAGFile

        background: Image {
            width: 44
            height: 44
            source: "qrc:/image/volume.png"
        }
    }

    Text {
        id: txtProgress

        text: qsTr("00:00")
        font.pixelSize: 12
        color: "#ffffff"
        anchors.right: timeSeparator.left
        anchors.rightMargin: 2
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter

    }

    Text {
        id: timeSeparator

        x: 180
        y: 31
        color: "#EEEEEE"
        text: qsTr("/")
        font.pixelSize: 12
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter
        visible: false
    }

    Text {
        id: txtDurationInSecond

        color: "#ffffff"
        text: qsTr("00:00")
        font.pixelSize: 12
        anchors.left: timeSeparator.right
        anchors.leftMargin: 2
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter
        visible: false
    }

    Text {
        id: txtCurrentFrame

        color: "#ffffff"
        text: qsTr("0")
        font.pixelSize: 12
        anchors.right: frameSeparator.left
        anchors.rightMargin: 2
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter

    }

    Text {
        id: frameSeparator

        x: 208
        y: 31
        color: "#EEEEEE"
        text: qsTr("/")
        font.pixelSize: 12
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter
    }

    Text {
        id: txtTotalFrame

        color: "#ffffff"
        text: qsTr("0")
        font.pixelSize: 12
        anchors.left: frameSeparator.right
        anchors.leftMargin: 2
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter
    }

    Button {
        id: btnNext

        x: 108
        y: 30
        z: 1
        width: 24
        height: 20
        enabled: hasPAGFile
        text: qsTr("")
        focusPolicy: Qt.NoFocus
        display: AbstractButton.IconOnly
        flat: true
        anchors.verticalCenterOffset: 2
        anchors.verticalCenter: parent.verticalCenter
        opacity: btnNext.pressed ? 1 : btnNext.hovered ? 0.9 : 0.8

        background: Image {
            width: 24
            height: 20
            source: "qrc:/image/next.png"
        }

        Timer {
            id: longPressNextTimer

            interval: 60
            repeat: true
            running: false

            onTriggered: {
                pagViewer.nextFrame()
            }
        }

        onPressAndHold: {
            pagViewer.nextFrame()
            longPressNextTimer.start()
        }

        onReleased: {
            longPressNextTimer.stop()
        }
    }

    Button {
        id: btnPrevious

        x: 76
        y: 30
        z: 1
        width: 24
        height: 20
        text: qsTr("")
        enabled: hasPAGFile
        focusPolicy: Qt.NoFocus
        display: AbstractButton.IconOnly
        flat: true
        anchors.verticalCenterOffset: 2
        anchors.verticalCenter: parent.verticalCenter
        opacity: btnPrevious.pressed ? 1 : btnPrevious.hovered ? 0.9 : 0.8

        background: Image {
            width: 24
            height: 20
            source: "qrc:/image/previous.png"
        }

        Timer {
            id: longPressPreTimer

            interval: 60
            repeat: true
            running: false

            onTriggered: {
                pagViewer.previousFrame()
            }
        }

        onPressAndHold: {
            pagViewer.previousFrame()
            longPressPreTimer.start()
        }

        onReleased: {
            longPressPreTimer.stop()
        }
    }

    Button {
        id: btnPlay

        x: 18
        y: 14
        z: 1
        width: 48
        height: 48
        text: qsTr("")
        visible: !isPlaying && (lastPlayStatus === false)
        enabled: hasPAGFile
        focusPolicy: Qt.NoFocus
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: parent.verticalCenter
        opacity: btnPlay.pressed ? 1 : btnPlay.hovered ? 0.9 : 0.8

        background: Image {
            width: 48
            height: 48
            source: "qrc:/image/play.png"
        }

        onClicked: {
            isPlaying = true
        }
    }

    Button {
        id: btnPause

        x: 18
        y: 14
        z: 1
        width: 48
        height: 48
        text: qsTr("")
        visible: isPlaying || (lastPlayStatus === true)
        enabled: hasPAGFile
        focusPolicy: Qt.NoFocus
        display: AbstractButton.IconOnly
        flat: true
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: parent.verticalCenter
        opacity: btnPause.pressed ? 1 : btnPause.hovered ? 0.9 : 0.8

        background: Image {
            width: 48
            height: 48
            source: "qrc:/image/pause.png"
        }

        onClicked: {
            isPlaying = false
        }
    }
}
