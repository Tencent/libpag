import QtQuick
import QtQuick.Controls

Rectangle {
    id: timeStretch

    required property var model

    color: "transparent"

    Rectangle {
        id: timeStretchModeContainer
        height: 30
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.leftMargin: 20
        color: "transparent"

        Text {
            id: timeStretchModeText
            width: 150
            text: qsTr("Time Stretch Mode")
            font.pixelSize: 14
            font.family: "PingFang SC"
            renderType: Text.NativeRendering
            color: "#FFFFFF"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        PAGComboBox {
            id: timeStretchModeComboBox
            width: 200
            height: 30
            implicitWidth: width
            implicitHeight: height
            anchors.left: timeStretchModeText.right
            anchors.verticalCenter: parent.verticalCenter
            currentIndex: timeStretch.model.getTimeStretchModes().indexOf(timeStretch.model.timeStretchMode)
            model: timeStretch.model.getTimeStretchModes()

            onActivated: {
                timeStretch.model.setTimeStretchMode(currentValue);
            }
        }
    }

    Rectangle {
        id: startTimeContainer
        height: 30
        anchors.top: timeStretchModeContainer.bottom
        anchors.topMargin: 15
        anchors.left: parent.left
        anchors.leftMargin: 20
        color: "transparent"

        Text {
            id: startTimeText
            width: 150
            text: qsTr("Starting Time")
            font.pixelSize: 14
            font.family: "PingFang SC"
            renderType: Text.NativeRendering
            color: "#FFFFFF"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        PAGTextInput {
            id: startTimeInput
            width: 200
            height: 28
            displayText: frameToTime(model.stretchStartTime)
            anchors.left: startTimeText.right
            anchors.verticalCenter: parent.verticalCenter
            radius: 2
            color: "#383841"
            enabled: model.timeStretchMode === "Stretch"

            onEditingFinish: function (text) {
                let frame = timeToFrame(text);
                model.setStretchStartTime(frame);
                displayText = frameToTime(model.stretchStartTime);
                startTimeTip.visible = (model.stretchStartTime + model.stretchDuration) > model.duration;
            }
        }

        Rectangle {
            id: startTimeTip
            height: parent.height
            anchors.left: startTimeInput.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            color: "transparent"
            visible: false

            Image {
                id: startTimeTipIcon
                width: 18
                height: 18
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/images/export-warning.png"
            }

            Text {
                id: startTimeTipText
                text: qsTr("Starting Time Out of Range")
                font.pixelSize: 13
                font.family: "PingFang SC"
                renderType: Text.NativeRendering
                color: "#90FFFFFF"
                anchors.left: startTimeTipIcon.right
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Rectangle {
        id: durationContainer
        height: 30
        anchors.top: startTimeContainer.bottom
        anchors.topMargin: 15
        anchors.left: parent.left
        anchors.leftMargin: 20
        color: "transparent"

        Text {
            id: durationText
            width: 150
            text: qsTr("Duration")
            font.pixelSize: 14
            font.family: "PingFang SC"
            renderType: Text.NativeRendering
            color: "#FFFFFF"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        PAGTextInput {
            id: durationInput
            width: 200
            height: 28
            displayText: frameToTime(model.stretchDuration)
            anchors.left: durationText.right
            anchors.verticalCenter: parent.verticalCenter
            radius: 2
            color: "#383841"
            enabled: model.timeStretchMode === "Stretch"

            onEditingFinish: function (text) {
                let frame = timeToFrame(text);
                model.setStretchDuration(frame);
                displayText = frameToTime(model.stretchDuration);
                durationTip.visible = model.stretchDuration > model.duration;
            }
        }

        Rectangle {
            id: durationTip
            height: parent.height
            anchors.left: durationInput.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            color: "transparent"
            visible: false

            Image {
                id: durationTipIcon
                width: 18
                height: 18
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/images/export-warning.png"
            }

            Text {
                id: durationTipText
                text: qsTr("Duration Out of Range")
                font.pixelSize: 13
                font.family: "PingFang SC"
                renderType: Text.NativeRendering
                color: "#90FFFFFF"
                anchors.left: durationTipIcon.right
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Rectangle {
        height: 30
        anchors.top: durationContainer.bottom
        anchors.topMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 20
        color: "transparent"

        Item {
            id: spacer
            width: 150
            height: 1
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 200
            height: parent.height
            anchors.left: spacer.right
            color: "transparent"

            Text {
                text: qsTr("FPS: ") + model.frameRate
                font.pixelSize: 13
                font.family: "PingFang SC"
                renderType: Text.NativeRendering
                color: "#90FFFFFF"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: qsTr("Duration: ") + model.duration
                font.pixelSize: 13
                font.family: "PingFang SC"
                renderType: Text.NativeRendering
                color: "#90FFFFFF"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    function frameToTime(frame) {
        let frameRate = model.frameRate;
        let part4 = frame % frameRate;
        let totalSeconds = Math.floor(frame / frameRate);
        let part3 = totalSeconds % 60;
        let totalMinutes = Math.floor(totalSeconds / 60);
        let part2 = totalMinutes % 60;
        let part1 = Math.floor(totalMinutes / 60) % 60;

        function pad(num) {
            return num < 10 ? "0" + num : num.toString();
        }
        return `${pad(part1)}:${pad(part2)}:${pad(part3)}:${pad(part4)}`;
    }

    function timeToFrame(time) {
        let parts = time.split(":");
        while (parts.length < 4) {
            parts.unshift("0");
        }

        if (parts.length > 4) {
            parts = parts.slice(parts.length - 4);
        }

        let frameRate = model.frameRate;
        let part1 = parseInt(parts[0]) || 0;
        let part2 = parseInt(parts[1]) || 0;
        let part3 = parseInt(parts[2]) || 0;
        let part4 = parseInt(parts[3]) || 0;

        if (part4 >= frameRate) {
            part3 = part3 + Math.floor(part4 / frameRate);
            part4 = part4 % frameRate;
        }
        if (part3 > 59) {
            part2 = part2 + Math.floor(part3 / 60);
            part3 = part3 % 60;
        }
        if (part2 > 59) {
            part1 = part1 + Math.floor(part2 / 60);
            part2 = part2 % 60;
        }
        if (part1 > 59) {
            part1 = part1 % 60;
        }

        return (60 * 60 * frameRate * part1) + (60 * frameRate * part2) + (frameRate * part3) + part4;
    }
}
