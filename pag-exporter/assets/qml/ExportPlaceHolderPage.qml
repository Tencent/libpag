import QtQuick 2.5
import QtQuick.Controls 2.1

Item{
    property var itemMarginLeft: 15
    property var itemMarginRight: 18
    property var itemHeight: 29
    property var comboBoxHeight: 28
    property var tipTextSize: 10
    property var tipTextColor: Qt.rgba(1, 1, 1, 153/255)
    property var itemBackgroundColor: "#383840"
    property var fontFamily: "PingFang SC"

    id: placeHolderTab
    Rectangle{
        anchors.fill: parent
        color: conentColor

        Rectangle{
            property var comLeftMargin: 15

            id: placeLeftPart
            width: 361
            height: parent.height
            color: "transparent"

            Rectangle{
                id: placeLeftTitle
                anchors.left: parent.left
                anchors.right: parent.right
                width: parent.width
                height: 36
                color: "transparent"

                Text{
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: placeLeftPart.comLeftMargin
                    font.pixelSize: 14
                    font.family: fontFamily
                    text: qsTr("名称")
                    font.bold: true
                    color: "#FFFFFF"
                }

                Rectangle{
                    width: parent.width
                    height: 1
                    anchors.bottom: parent.bottom
                    color: "black"
                }
            }

            ListView{
                id: placeListView
                model: placeholderModel
                delegate: placeHolderDelegate
                anchors.top: placeLeftTitle.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                clip: true

                ScrollBar.vertical: ScrollBar {
                    visible: placeListView.contentHeight > placeListView.height
                    hoverEnabled: true
                    active: hovered || pressed
                    orientation: Qt.Vertical
                    size: parent.height / placeListView.height
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    policy: ScrollBar.AsNeeded
            
                    contentItem: Rectangle {
                        implicitWidth: 3
                        implicitHeight: 100
                        radius: 3
                        color: "#535359"
                    }
                }

                Component.onCompleted:{
                    if(placeholderModel.rowCount() != 0){
                        placeholderModel.setCurrentSelect(0)
                    }
                }
            }
        }

        Rectangle{
            id: verticalDivider
            width: 1
            height: parent.height
            anchors.left: placeLeftPart.right
            color: "black"
        }

        Rectangle{
            id: rightPart
            anchors.left: verticalDivider.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 17
            height: parent.height
            color: "transparent"
            visible: placeholderModel.rowCount() != 0

            Rectangle{
                id: fillModeItem
                width: parent.width
                height: itemHeight
                color: "transparent"

                Text{
                    id: fillModeText
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: itemMarginLeft
                    width: assetModeText.width
                    font.pixelSize: 14
                    font.family: fontFamily
                    text: qsTr("填充模式")
                    font.bold: true
                    color: "#FFFFFF"
                }

                ExportComboBox{
                    id: fillModeComboBox
                    anchors.left: fillModeText.right
                    anchors.right: parent.right
                    anchors.leftMargin: 16
                    anchors.rightMargin: itemMarginRight
                    anchors.verticalCenter: parent.verticalCenter
                    height: comboBoxHeight
                    implicitHeight: height
                    currentIndex: 0
                    showCount: 4
                    model: ListModel {
                        ListElement { text: qsTr("不缩放") }
                        ListElement { text: qsTr("拉伸") }
                        ListElement { text: qsTr("黑边") }
                        ListElement { text: qsTr("裁剪") }
                    }
                    onActivated:{
                        exportSettingDialog.fillModeIndexChanged(currentIndex)
                    }
                }
            }

            Rectangle{
                id: assetModeItem
                anchors.top: fillModeItem.bottom
                anchors.topMargin: 12
                width: parent.width
                height: itemHeight
                color: "transparent"

                Text{
                    id: assetModeText
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: itemMarginLeft
                    font.pixelSize: 14
                    font.family: fontFamily
                    text: qsTr("素材格式")
                    font.bold: true
                    color: "#FFFFFF"
                }

                ExportComboBox{
                    id: assetModeComboBox
                    anchors.left: assetModeText.right
                    anchors.right: parent.right
                    anchors.leftMargin: 16
                    anchors.rightMargin: itemMarginRight
                    anchors.verticalCenter: parent.verticalCenter
                    height: comboBoxHeight
                    implicitHeight: height
                    currentIndex: 0
                    showCount: 2
                    model: ListModel {
                        ListElement { text: qsTr("替换") }
                        ListElement { text: qsTr("不替换") }
                    }
                    onActivated:{
                        exportSettingDialog.assetTypeIndexChange(currentIndex, currentIndex)
                    }
                }
            }

            Rectangle{
                anchors.top: assetModeItem.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                anchors.topMargin: 10
                color: "transparent"

                Text{
                    id: tipsText2
                    anchors.leftMargin: itemMarginLeft
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: tipsText.bottom
                    anchors.topMargin: 3
                    font.pixelSize: tipTextSize
                    font.family: fontFamily
                    text: qsTr("替换: 图层中的占位图支持用图片或视频替换")
                    color: tipTextColor
                    wrapMode: Text.WordWrap
                }

                Text{
                    id: tipsTextT
                    anchors.leftMargin: itemMarginLeft
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: tipsText2.bottom
                    anchors.topMargin: 3
                    font.pixelSize: tipTextSize
                    font.family: fontFamily
                    text: qsTr("不替换: 图层中的占位图不支持替换")
                    color: Qt.rgba(1, 1, 1, 153/255)
                    wrapMode: Text.WordWrap
                }
            }

            Connections{
                target: placeholderModel
                onItemSelectChange:{
                    console.log("onItemSelectChange, fillModeIndex:" + fillModeIndex + ", assetModeIndex:" + assetModeIndex)
                    fillModeComboBox.currentIndex = fillModeIndex
                    assetModeComboBox.currentIndex = assetModeIndex
                }
            }
        }
    }

    function showOrHideRightPart() {
        var placeHolderCount = placeholderModel.rowCount()
        rightPart.visible = placeHolderCount != 0
        console.log("showOrHideRightPart, placeHolderCount:" + placeHolderCount)
    }

    Component {
        id: placeHolderDelegate
        Rectangle {
            id: wrapper
            width: placeListView.width
            height: 64
            color: isSelected ? Qt.rgba(1, 1, 1, 5/255): "transparent"

            Image {
                property var iconSize: 48
                id: imgIcon
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                width: iconSize
                height: iconSize
                source: imageSource
                asynchronous: true
            }

            Text {
                id: placeNameText
                text: name
                renderType: Text.NativeRendering
                anchors.left: imgIcon.right
                anchors.right: parent.right
                anchors.leftMargin: 12
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignTop
                elide: Qt.ElideMiddle
                color: "white"
                font.pixelSize: 14
                font.family: fontFamily
            }

            MouseArea{
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    if(!isSelected){
                        placeholderModel.setCurrentSelect(index)
                    }
                }
            }
        }
    }
}
