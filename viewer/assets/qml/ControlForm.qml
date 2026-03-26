import QtQuick
import QtQuick.Controls
import "components"

Item {
    id: form
    required property var pagView
    property bool hasPAGFile: (pagView && pagView.filePath !== "")

    property bool updateAvailable: false

    property bool lastPlayStatusIsPlaying: false

    property int sliderHeight: 12

    property int controlFormHeight: 76

    property alias progressSlider: progressSlider

    property alias updateButton: updateButton

    property alias panelsButton: panelsButton

    property alias backgroundButton: backgroundButton

    property alias timeDisplayedText: timeDisplayedText

    property alias currentFrameText: currentFrameText

    property alias totalFrameText: totalFrameText

    PAGRectangle {
        height: controlFormHeight
        color: "#20202A"
        anchors.fill: parent
        radius: 5
        leftTopRadius: false
        rightTopRadius: false
        rightBottomRadius: !panelsButton.checked
    }
    Slider {
        id: progressSlider
        height: sliderHeight
        z: 1
        padding: 0
        anchors.top: parent.top
        anchors.topMargin: -4
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        enabled: hasPAGFile
        background: Rectangle {
            x: progressSlider.leftPadding
            y: progressSlider.topPadding + progressSlider.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 4
            width: progressSlider.availableWidth
            height: implicitHeight
            radius: 0
            color: "#7D7D7D"

            Rectangle {
                width: progressSlider.visualPosition * parent.width
                height: parent.height
                color: "#448EF9"
                radius: 0
            }
        }
        handle: Rectangle {
            x: progressSlider.leftPadding + progressSlider.visualPosition * (progressSlider.availableWidth - width)
            y: progressSlider.topPadding + progressSlider.availableHeight / 2 - height / 2
            implicitWidth: 12
            implicitHeight: 12
            radius: 12
            color: progressSlider.pressed ? "#f0f0f0" : "#f6f6f6"
            border.color: "#bdbebf"
        }
    }
    Row {
        z: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        Row {
            id: leftControls
            spacing: 8
            width: implicitWidth

            /* Space Holder */
            Item {
                width: 10
                height: 1
            }
            Button {
                id: playButton
                width: 48
                height: 48
                visible: true
                enabled: hasPAGFile
                focusPolicy: Qt.NoFocus
                flat: true
                display: AbstractButton.IconOnly
                opacity: pressed ? 1 : hovered ? 0.9 : 0.8
                background: Image {
                    source: ((pagView && pagView.isPlaying) || form.lastPlayStatusIsPlaying) ? "qrc:/images/pause.png" : "qrc:/images/play.png"
                }
                onClicked: {
                    pagView.isPlaying = !pagView.isPlaying;
                }
            }
            Button {
                id: previousButton
                width: 24
                height: 20
                enabled: hasPAGFile
                focusPolicy: Qt.NoFocus
                display: AbstractButton.IconOnly
                flat: true
                anchors.verticalCenter: parent.verticalCenter
                opacity: pressed ? 1 : hovered ? 0.9 : 0.8
                background: Image {
                    source: "qrc:/images/previous.png"
                }
                onPressAndHold: {
                    pagView.previousFrame();
                    longPressPreTimer.start();
                }
                onReleased: {
                    longPressPreTimer.stop();
                }
                onClicked: {
                    pagView.previousFrame();
                }

                Timer {
                    id: longPressPreTimer
                    interval: 60
                    repeat: true
                    running: false
                    onTriggered: {
                        pagView.previousFrame();
                    }
                }
            }
            Button {
                id: nextButton
                width: 24
                height: 20
                enabled: hasPAGFile
                focusPolicy: Qt.NoFocus
                display: AbstractButton.IconOnly
                flat: true
                anchors.verticalCenter: parent.verticalCenter
                opacity: pressed ? 1 : hovered ? 0.9 : 0.8
                background: Image {
                    source: "qrc:/images/next.png"
                }
                onPressAndHold: {
                    pagView.nextFrame();
                    longPressNextTimer.start();
                }
                onReleased: {
                    longPressNextTimer.stop();
                }
                onClicked: {
                    pagView.nextFrame();
                }

                Timer {
                    id: longPressNextTimer
                    interval: 60
                    repeat: true
                    running: false
                    onTriggered: {
                        pagView.nextFrame();
                    }
                }
            }
            Text {
                id: timeDisplayedText
                color: "#ffffff"
                text: qsTr("00:00")
                font.pixelSize: 12
                anchors.verticalCenterOffset: 1
                anchors.verticalCenter: parent.verticalCenter
            }
            Row {
                spacing: 2
                anchors.verticalCenterOffset: 1
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    id: currentFrameText
                    color: "#ffffff"
                    text: qsTr("0")
                    font.pixelSize: 12
                }
                Text {
                    id: frameSeparator
                    color: "#EEEEEE"
                    text: qsTr("/")
                    font.pixelSize: 12
                }
                Text {
                    id: totalFrameText
                    color: "#ffffff"
                    text: qsTr("0")
                    font.pixelSize: 12
                }
            }
        }
        /* Space Holder */
        Item {
            id: centerSpace
            width: parent.width - leftControls.width - rightControls.width
            height: parent.height
        }
        Row {
            id: rightControls
            spacing: 2

            CheckBox {
                id: updateButton
                width: 44
                height: 44
                enabled: updateAvailable
                visible: updateAvailable
                focusPolicy: Qt.NoFocus
                opacity: mouseArea.pressed ? 0.5 : 1.0
                indicator: Image {
                    width: 44
                    height: 44
                    source: "qrc:/images/update-red.png"
                }

                MouseArea {
                    id: mouseArea
                    property bool entered: false
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: {
                        updateButton.click();
                    }
                    onEntered: {
                        entered = true;
                    }
                    onExited: {
                        entered = false;
                    }

                    ToolTip {
                        visible: parent.entered
                        text: qsTr("Discover a new version, click to update")
                    }
                }
            }
            CheckBox {
                id: backgroundButton
                width: 44
                height: 44
                enabled: hasPAGFile
                focusPolicy: Qt.NoFocus
                anchors.verticalCenterOffset: 0
                anchors.verticalCenter: parent.verticalCenter
                indicator: Image {
                    width: 44
                    height: 44
                    source: backgroundButton.checked ? "qrc:/images/background-on.png" : "qrc:/images/background-off.png"
                }
            }
            CheckBox {
                id: panelsButton
                width: 44
                height: 44
                enabled: hasPAGFile
                focusPolicy: Qt.NoFocus
                indicator: Image {
                    width: 44
                    height: 44
                    source: panelsButton.checked ? "qrc:/images/panels-on.png" : "qrc:/images/panels-off.png"
                }
            }

            /* Space Holder */
            Item {
                width: 18
                height: 1
            }
        }
    }
}
