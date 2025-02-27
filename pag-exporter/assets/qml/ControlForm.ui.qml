import QtQuick 2.5
import QtQuick.Controls 2.13

Item {

    property alias btnNext: btnNext
    property alias btnPrevious: btnPrevious
    property alias btnPause: btnPause
    property alias btnPlay: btnPlay
    property alias txtProgress: txtProgress
    property alias txtDurationInSecond: txtDurationInSecond
    

    property bool hasPAGFile: false
    property double progress: 0
    property double duration: 0
    property bool isPlaying: true
    property bool lastPlayStatus: false

    width: 400
    height: 400
    property alias sliderProgress: sliderProgress
    property alias btnPanels: btnPanels
    property alias btnBackground: btnBackground

    RectangleWithRadius {
        id: rectangle
        height: 76
        color: "#20202A"
        anchors.fill: parent
        radius: 5
        rightBottom: !btnPanels.checked
        leftTop: false
        rightTop: false
    }

    Slider {
        id: sliderProgress
        height: 12
        z: 1
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
            y: sliderProgress.topPadding + sliderProgress.availableHeight / 2 - height / 2
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
            x: sliderProgress.leftPadding + sliderProgress.visualPosition
               * (sliderProgress.availableWidth - width)
            y: sliderProgress.topPadding + sliderProgress.availableHeight / 2 - height / 2
            implicitWidth: 12
            implicitHeight: 12
            radius: 16
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
            from: 0
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
                x: sliderVolume.leftPadding + sliderVolume.availableWidth / 2 - width / 2
                y: sliderVolume.topPadding
                implicitWidth: 4
                implicitHeight: 54
                width: implicitWidth
                height: sliderVolume.availableHeight
                radius: 2
                color: "#000000"

                Rectangle {
                    height: (1 - sliderVolume.visualPosition) * parent.height
                    width: parent.width
                    color: "#4A90E2"
                    radius: 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 0
                }
            }

            handle: Rectangle {
                y: sliderVolume.topPadding + sliderVolume.visualPosition
                   * (sliderVolume.availableHeight - height)
                x: sliderVolume.leftPadding + sliderVolume.availableWidth / 2 - width / 2
                implicitWidth: 13
                implicitHeight: 13
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
            source: "../images/volume.png"
            fillMode: Image.PreserveAspectFit
        }
    }

    CheckBox {
        id: btnBackground
        x: 492
        y: 16
        width: 44
        height: 44
        enabled: hasPAGFile
        text: qsTr("Background")
        focusPolicy: Qt.NoFocus
        display: AbstractButton.TextOnly
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 64
        anchors.right: parent.right

        indicator: Image {
            height: 44
            width: 44
            source: btnBackground.checked ? "qrc:/images/background-on.png" : "qrc:/images/background.png"
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
            height: 44
            width: 44
            source: btnPanels.checked ? "qrc:/images/panels-on.png" : "qrc:/images/panels.png"
        }
    }

    Button {
        id: btnVolume
        visible: false
        enabled: hasPAGFile
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

        background: Image {
            height: 44
            width: 44
            source: "qrc:/images/volume.png"
        }
    }

    Text {
        id: txtProgress
        x: 45
        y: 28
        color: "#ffffff"
        text: qsTr("00:00")
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: 15
    }

    Text {
        x: 83
        y: 28
        color: "#EEEEEE"
        text: qsTr("/")
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: 15
    }

    Text {
        id: txtDurationInSecond
        x: 90
        y: 28
        color: "#ffffff"
        text: qsTr("00:00")
        anchors.verticalCenterOffset: 1
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: 15
    }

    Button {
        id: btnNext
        x: 137
        y: 30
        width: 24
        height: 20
        enabled: hasPAGFile
        text: qsTr("")
        focusPolicy: Qt.NoFocus
        display: AbstractButton.IconOnly
        flat: true
        z: 1
        anchors.verticalCenterOffset: 2
        anchors.verticalCenter: parent.verticalCenter
        opacity: btnNext.pressed ? 1 : btnNext.hovered ? 0.9 : 0.8

        background: Image {
            height: 20
            width: 24
            source: "qrc:/images/next.png"
        }
        onPressAndHold: {
            pagViewer.nextFrame();
            longPressNextTimer.start()
        }
        onReleased: longPressNextTimer.stop()
        Timer {
            id: longPressNextTimer

            interval: 60 //your press-and-hold interval here
            repeat: true
            running: false

            onTriggered: {
                pagViewer.nextFrame()
            }
        }
    }

    Button {
        id: btnPrevious
        x: 12
        y: 30
        width: 24
        height: 20
        text: qsTr("")
        enabled: hasPAGFile
        focusPolicy: Qt.NoFocus
        display: AbstractButton.IconOnly
        flat: true
        z: 1
        anchors.verticalCenterOffset: 2
        anchors.verticalCenter: parent.verticalCenter
        opacity: btnPrevious.pressed ? 1 : btnPrevious.hovered ? 0.9 : 0.8

        background: Image {
            height: 20
            width: 24
            source: "qrc:/images/previous.png"
        }
        onPressAndHold: {
            pagViewer.previousFrame();
            longPressPreTimer.start();
        }
        onReleased: longPressPreTimer.stop()
        Timer {
            id: longPressPreTimer

            interval: 60 //your press-and-hold interval here
            repeat: true
            running: false

            onTriggered: {
                pagViewer.previousFrame()
            }
        }
    }

    Button {
        id: btnPlay
        visible: !isPlaying && lastPlayStatus === false
        x: 492
        y: 14
        width: 36
        height: 36
        text: qsTr("")
        enabled: hasPAGFile
        focusPolicy: Qt.NoFocus
        z: 1
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 18
        anchors.right: parent.right
        opacity: btnPlay.pressed ? 1 : btnPlay.hovered ? 0.9 : 0.8

        background: Image {
            height: 36
            width: 36
            source: "qrc:/images/play.png"
        }

        onClicked: isPlaying = true
    }

    Button {
        id: btnPause
        visible: isPlaying || lastPlayStatus === true
        x: 492
        y: 14
        width: 36
        height: 36
        text: qsTr("")
        enabled: hasPAGFile
        focusPolicy: Qt.NoFocus
        display: AbstractButton.IconOnly
        flat: true
        z: 1
        anchors.verticalCenterOffset: 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 18
        anchors.right: parent.right
        opacity: btnPause.pressed ? 1 : btnPause.hovered ? 0.9 : 0.8

        background: Image {
            height: 36
            width: 36
            source: "qrc:/images/pause.png"
        }

        onClicked: isPlaying = false
    }
}
