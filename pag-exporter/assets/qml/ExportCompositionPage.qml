import QtQuick 2.5
import QtQuick.Controls 2.1

Item{
    property var bmpIconPath : isPreview ? "../images/export_bmp.png" : "qrc:/images/export_bmp.png"
    property var checkboxOffIconPath : isPreview ? "../images/checkbox_off.png" : "qrc:/images/checkbox_off.png"
    property var checkboxOnIconPath : isPreview ? "../images/checkbox_on.png" : "qrc:/images/checkbox_on.png"
    property var leftParthWidth: 301
    property var dividerWidth: 1
    property var snapshotImgMargin: 50
    property var sliderContainerHeight: 47

    id: compositionTab
    Rectangle{
        anchors.fill: parent
        color: conentColor
        border.width: 1
        border.color: "black"

        Rectangle{
            property var comLeftMargin: 15

            id: comLeftPart
            width: leftParthWidth
            height: parent.height
            color: "transparent"

            Rectangle{
                id: comLeftTitle
                anchors.left: parent.left
                anchors.right: parent.right
                width: parent.width
                height:36
                color: "transparent"

                Text{
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: comLeftPart.comLeftMargin
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    text: qsTr("名称")
                    font.bold: true
                    color: "#FFFFFF"
                }

                Text{
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    text: "BMP"
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
                id: layerListView
                model: compositionModel
                delegate: compositionDelegate
                anchors.top: comLeftTitle.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                clip: true

                ScrollBar.vertical: ScrollBar {
                    visible: layerListView.contentHeight > layerListView.height
                    hoverEnabled: true
                    active: hovered || pressed
                    orientation: Qt.Vertical
                    size: parent.height / layerListView.height
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
            }
        }
        
        Rectangle{
            id: verticalDivider
            width: dividerWidth
            height: parent.height
            anchors.left: comLeftPart.right
            color: "black"
        }

        Rectangle{
            id: comRightPart
            height: parent.height
            anchors.left: verticalDivider.right
            anchors.right: parent.right
            color: "transparent"

            Rectangle{
                id: snapshotContainer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: snapshotImgMargin
                anchors.rightMargin: snapshotImgMargin
                anchors.top: parent.top
                anchors.bottom: sliderContainer.top
                border.color: "red"
                border.width: 0
                color: "transparent"

                Image{
                    property var showWidth: isWinPlatform ? windowWidth - leftParthWidth - dividerWidth -  snapshotImgMargin * 2: width
                    property var showHeight: isWinPlatform ? windowHeight - sliderContainerHeight : height
                    property var compositionName: compositionModel.getCompositionName()

                    id:snapshotImg
                    anchors.fill: parent
                    asynchronous: true
                    cache: false
                    Connections{
                        target: snapshotCenter
                        onImageChange:{
                            console.log("onImageChange")
                            snapshotImg.source = "image://CompositionImg/"+ frameId
                        }
                    }

                    Component.onCompleted:{
                        snapshotCenter.setSize(snapshotImg.showWidth, snapshotImg.showHeight)
                        snapshotCenter.getImageInProgress(1)
                    }
                }
            }

            Rectangle{
                id: sliderContainer
                anchors.bottom: parent.bottom
                height: sliderContainerHeight
                width: parent.width
                color: Qt.rgba(1,1,1,0.06)

                Slider{
                    property var lastTime: new Date().getTime()

                    id: snapshotSlider
                    from: 1
                    to: compositionModel.getSliderMaxValue()
                    value: 0
                    stepSize: 1
                    width: parent.width
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter
                    
                    background: Rectangle {
                        x: snapshotSlider.leftPadding
                        y: snapshotSlider.topPadding + snapshotSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 4
                        width: snapshotSlider.availableWidth
                        height: implicitHeight
                        radius: 2
                        color: Qt.rgba(1,1,1,0.2)

                        Rectangle {
                            width: snapshotSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#1982EB"
                            radius: 2
                        }
                    }

                    handle: Rectangle {
                        x: snapshotSlider.leftPadding + snapshotSlider.visualPosition * (snapshotSlider.availableWidth - width)
                        y: snapshotSlider.topPadding + snapshotSlider.availableHeight / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: snapshotSlider.pressed ? "#f0f0f0" : "#f6f6f6"
                        border.color: "#bdbebf"
                    }

                    onValueChanged:{
                        snapshotCenter.getImageInProgress(snapshotSlider.value)
                    }
                }
            }
        }
    }

    Component {
        id: compositionDelegate
        Rectangle {
            id: wrapper
            width: layerListView.width
            height: 36
            color: index == 0 ? Qt.rgba(1, 1, 1, 5/255):"transparent"

            Image {
                id:compIcon
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: index == 0 ? 16 : 47
                width: normalIconSize
                height: normalIconSize
                source: bmpIconPath
            }

            Text {
                id: comNameText
                text: name
                renderType: Text.NativeRendering
                anchors.left: compIcon.right
                anchors.right: bmpCheckBox.left
                anchors.leftMargin: 12
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignTop
                elide: Qt.ElideMiddle
                color: "white"
                font.pixelSize: 14
                font.family: "PingFang SC"
            }

            Image {
                id:bmpCheckBox
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                width: normalIconSize
                height: normalIconSize
                source: itemChecked ? checkboxOnIconPath : checkboxOffIconPath
                opacity: itemEnable? 1.0 : 0.2

                MouseArea{
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onPressed: {
                        if(itemEnable){
                            compositionModel.onLayerBmpStatusChange(index, !itemChecked)
                        }
                    }
                }
            }
        }
    }
}