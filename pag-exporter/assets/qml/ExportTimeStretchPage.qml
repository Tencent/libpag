import QtQuick 2.5
import QtQuick.Controls 2.1

Item{
    property var pageMarginLeft: 20
    property var secondPartMarginLeft: 156
    property var editWidth: 198
    property var editHeight: 28
    property var itemHeight: editHeight + 1
    property var editBackColor: "#383840"
    property var tipTextColor: "#90FFFFFF"
    property var fpsTips: qsTr("帧率:%1 时长:%2")

    property var fps: timeStretchModel.getFps()
    property var wholeFPS: timeStretchModel.getCompositionDuration()
    property var needShowStartTimeTip: false
    property var needShowDurationTip: false

    id: timeStretchTab
    Rectangle{
        id: rootContainer
        anchors.fill: parent
        color: conentColor
        border.width: 1
        border.color: "black"

        MouseArea{
            anchors.fill: parent
            onPressed:{
                console.log("parent onPressed")
                rootContainer.focus = true
            }
        }

        Rectangle{
            id: stretchModeCon
            height: itemHeight
            anchors.top: parent.top
            anchors.topMargin: 30
            Text{
                font.pixelSize: 14
                font.family: "PingFang SC"
                anchors.left: parent.left
                anchors.leftMargin: pageMarginLeft
                anchors.verticalCenter: parent.verticalCenter
                text: "TimeStretchMode"
                color: "#FFFFFF"
                renderType: Text.NativeRendering
            }

            ExportComboBox{
                id: stretchModeComboBox
                anchors.left: parent.left
                anchors.leftMargin: secondPartMarginLeft
                anchors.verticalCenter: parent.verticalCenter
                width: editWidth
                height: editHeight
                implicitWidth: width
                implicitHeight: height
                currentIndex: timeStretchModel.getStretchMode()
                showCount: 4
                model: ListModel {
                    ListElement { text: qsTr("无") }
                    ListElement { text: qsTr("伸缩") }
                    ListElement { text: qsTr("重复") }
                    ListElement { text: qsTr("重复(倒序)") }
                }
                onActivated:{
                    onStretchModeChange(currentIndex)
                }
                Component.onCompleted:{
                    onStretchModeChange(currentIndex)
                }

                function onStretchModeChange(currentIndex){
                    timeStretchModel.setStretchMode(currentIndex)
                    startTimeEditText.setEnable(currentIndex == 1)
                    durationEditText.setEnable(currentIndex == 1)
                }
            }
        }

        Rectangle{
            id: startTimeCon
            anchors.top: stretchModeCon.bottom
            anchors.topMargin: 15
            height: itemHeight

            Text{
                font.pixelSize: 14
                font.family: "PingFang SC"
                anchors.left: parent.left
                anchors.leftMargin: pageMarginLeft
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("起始时间")
                color: "#FFFFFF"
                renderType: Text.NativeRendering
            }

            TimeEditText{
                id: startTimeEditText
                Component.onCompleted:{
                    startTimeEditText.setDuration(timeStretchModel.getStretchStartTime())
                }
                Connections{
                    target: startTimeEditText
                    onDurationDataChange:{
                        timeStretchModel.setStretchStartTime(startTimeEditText.fpsDuration)
                        needShowStartTimeTip = startTimeEditText.fpsDuration > wholeFPS
                    }
                }
            }

            Rectangle{
                id: startTipsContainer
                anchors.left: startTimeEditText.right
                anchors.leftMargin: 10
                visible: needShowStartTimeTip
                anchors.verticalCenter: parent.verticalCenter

                Image{
                    id: startTipsWarnIcon
                    width: 18
                    height: 18
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    source: warnIconPath
                }

                Text{
                    id: startTipsText
                    anchors.left: startTipsWarnIcon.right
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 12
                    font.family: "PingFang SC"
                    text: qsTr("起始时间超出范围")
                    color: tipTextColor
                    renderType: Text.NativeRendering
                }
            }
        }

        Rectangle{
            id: durationCon
            anchors.top: startTimeCon.bottom
            anchors.topMargin: 15
            height: itemHeight

            Text{
                font.pixelSize: 14
                font.family: "PingFang SC"
                anchors.left: parent.left
                anchors.leftMargin: pageMarginLeft
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("持续时间")
                color: "#FFFFFF"
                renderType: Text.NativeRendering
            }

            TimeEditText{
                id: durationEditText
                Component.onCompleted:{
                    durationEditText.setDuration(timeStretchModel.getStretchDuration())
                }
                Connections{
                    target: durationEditText
                    onDurationDataChange:{
                        timeStretchModel.setStretchDuration(durationEditText.fpsDuration)
                        needShowDurationTip = durationEditText.fpsDuration > wholeFPS
                    }
                }
            }

            Rectangle{
                id: durationTipsContainer
                anchors.left: durationEditText.right
                anchors.leftMargin: 10
                visible: needShowDurationTip
                anchors.verticalCenter: parent.verticalCenter

                Image{
                    id: durationTipsWarnIcon
                    width: 18
                    height: 18
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    source: warnIconPath
                }

                Text{
                    id: durationTipsText
                    anchors.left: durationTipsWarnIcon.right
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 12
                    font.family: "PingFang SC"
                    text: qsTr("持续时间超出范围")
                    color: tipTextColor
                    renderType: Text.NativeRendering
                }
            }
        }

        Text{
            font.pixelSize: 12
            font.family: "PingFang SC"
            anchors.left: parent.left
            anchors.leftMargin: secondPartMarginLeft
            anchors.verticalCenter: parent.verticalCenter
            anchors.top: durationCon.bottom
            anchors.topMargin: 20
            text: fpsTips.arg(fps).arg(wholeFPS)
            color: tipTextColor
            renderType: Text.NativeRendering
        }
    }
}