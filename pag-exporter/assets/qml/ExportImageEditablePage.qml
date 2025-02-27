import QtQuick 2.5
import QtQuick.Controls 2.1

Item {
    property var checkboxOffIconPath : isPreview ? "../images/checkbox_off.png" : "qrc:/images/checkbox_off.png"
    property var checkboxOnIconPath : isPreview ? "../images/checkbox_on.png" : "qrc:/images/checkbox_on.png"
    property var tipIconPath : isPreview ? "../images/tip.png" : "qrc:/images/tip.png"
    property var allChecked: placeholderModel.isAllSelected()
    property var leftParthWidth: 301
    property var dividerWidth: 1
    property var snapshotImgMargin: 50
    property var sliderContainerHeight: 47

    id: imageEditableTab
    Rectangle {
        anchors.fill: parent
        color: conentColor
        border.width: 1
        border.color: "black"

        Rectangle {
            id: imageLayerPart
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width  //leftParthWidth
            height: parent.height
            color: "transparent"
            
            Rectangle {
                id: imageTitleLine
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                width: parent.width
                height: 36
                color: "transparent"

                Text {
                    id: indexImage
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    text: qsTr("序号")
                    font.bold: true
                    color: "#FFFFFF"
                }
                
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: indexImage.right
                    anchors.leftMargin: 12
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    text: qsTr("图片名称")
                    font.bold: true
                    color: "#FFFFFF"
                }
                
                
                Button {
                    anchors.right: fillModeTitle.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: normalIconSize
                    height: normalIconSize
                    text: qsTr("")
                    focusPolicy: Qt.NoFocus
                    opacity: pressed ? 0.5 : 1.0

                    background: Image {
                        width: normalIconSize
                        height: normalIconSize
                        source: tipIconPath
                    }

                    onClicked: Qt.openUrlExternally("https://pag.art/docs/pag-fillmode.html");
                }
                
                Text {
                    id: fillModeTitle
                    anchors.verticalCenter: parent.verticalCenter
                    //anchors.left: parent.left
                    anchors.right: imageEditableTitle.left
                    anchors.rightMargin: 24
                    //width: 36
                    font.pixelSize: 14
                    font.family: fontFamily
                    text: qsTr("填充模式")
                    font.bold: true
                    color: "#FFFFFF"
                }
                
                Button {
                    anchors.right: imageEditableTitle.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: normalIconSize
                    height: normalIconSize
                    text: qsTr("")
                    focusPolicy: Qt.NoFocus
                    opacity: pressed ? 0.5 : 1.0

                    background: Image {
                        width: normalIconSize
                        height: normalIconSize
                        source: tipIconPath
                    }

                    onClicked: Qt.openUrlExternally("https://pag.art/docs/pag-edit.html");
                }
                
                Text {
                    id: imageEditableTitle
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: imageAllCheckBox.left
                    //anchors.rightMargin: 4
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    text: qsTr("是否可编辑")
                    font.bold: true
                    color: "#FFFFFF"
                }
                
                Image {
                    id: imageAllCheckBox
                    
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    width: normalIconSize
                    height: normalIconSize
                    source: allChecked ? checkboxOnIconPath : checkboxOffIconPath

                    MouseArea{
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: {
                            allChecked = !allChecked
                            placeholderModel.setAllChecked(allChecked)
                        }
                    }
                }
            }
            
            
            ListView {
                id: imageListView
                model: placeholderModel
                delegate: imageDelegate
                anchors.top: imageTitleLine.bottom
                anchors.bottom: parent.bottom
                width: parent.width
                //height: 100
                clip: true

                ScrollBar.vertical: ScrollBar {
                    visible: imageListView.contentHeight > imageListView.height
                    hoverEnabled: true
                    active: hovered || pressed
                    orientation: Qt.Vertical
                    size: parent.height / imageListView.height
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
    }
    
    Component {
        id: imageDelegate
        Rectangle {
            property var imageIndex: index
            id: wrapper
            width: imageListView.width
            height: 36
            //color: "transparent"
            color: index%2==0 ? Qt.rgba(1, 1, 1, 5/255): "transparent"

            Text {
                id: imageIndexText
                text: index.toString()
                renderType: Text.NativeRendering
                anchors.left: parent.left
                anchors.leftMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignTop
                elide: Qt.ElideMiddle
                color: "white"
                font.pixelSize: 14
                font.family: "PingFang SC"
            }

            Text {
                id: imageNameText
                text: name
                renderType: Text.NativeRendering
                anchors.left: imageIndexText.right
                anchors.right: imageEditableCheckBox.left
                anchors.leftMargin: 18
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignTop
                elide: Qt.ElideMiddle
                color: "white"
                font.pixelSize: 14
                font.family: "PingFang SC"
            }
            
            ExportComboBox {
                id: fillModeComboBox
                //anchors.left: fillModeTitle.left
                anchors.right: imageEditableCheckBox.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 88
                width: 80
                //height: comboBoxHeight
                //implicitHeight: height
                currentIndex: scaleMode
                enabled: itemChecked
                showCount: 4
                opacity: enabled ? 1.0 : 0.2
                model: ListModel {
                    ListElement { text: qsTr("不缩放") }
                    ListElement { text: qsTr("拉伸") }
                    ListElement { text: qsTr("黑边") }
                    ListElement { text: qsTr("裁剪") }
                }
                onActivated: {
                  placeholderModel.onFillModeChanged(imageIndex, currentIndex)
                }
            }
            
            Image {
                id:imageEditableCheckBox
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: normalIconSize
                height: normalIconSize
                source: itemChecked ? checkboxOnIconPath : checkboxOffIconPath

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onPressed: {
                      placeholderModel.onImageEditableStatusChange(index, !itemChecked)
                      allChecked = placeholderModel.isAllSelected();
                    }
                }
            }
        }
    }
}
