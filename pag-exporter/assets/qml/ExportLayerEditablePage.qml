import QtQuick 2.5
import QtQuick.Controls 2.1

Item{
    property var layerIconPath : isPreview ? "../images/export_text.png" : "qrc:/images/export_text.png"
    property var checkboxOffIconPath : isPreview ? "../images/checkbox_off.png" : "qrc:/images/checkbox_off.png"
    property var checkboxOnIconPath : isPreview ? "../images/checkbox_on.png" : "qrc:/images/checkbox_on.png"
    property var tipIconPath : isPreview ? "../images/tip.png" : "qrc:/images/tip.png"
    property var allChecked: layerEditableModel.isAllSelected()
    property var leftParthWidth: 301
    property var dividerWidth: 1
    property var snapshotImgMargin: 50
    property var sliderContainerHeight: 47

    id: layerEditableTab
    Rectangle{
        anchors.fill: parent
        color: conentColor
        border.width: 1
        border.color: "black"

        Rectangle{
            id: textLayerPart
            width: parent.width  //leftParthWidth
            height: parent.height
            color: "transparent"
            
            Rectangle{
                id: leftTitle
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: boundLine1.bottom
                width: parent.width
                height:36
                color: "transparent"

                Text{
                    id: indexText
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    text: qsTr("序号")
                    font.bold: true
                    color: "#FFFFFF"
                }
                
                Text{
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: indexText.right
                    anchors.leftMargin: 12
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    text: qsTr("图层名称")
                    font.bold: true
                    color: "#FFFFFF"
                }
                
                Button {
                    anchors.right: layerEditableTitle.left
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
                
                Text{
                    id: layerEditableTitle
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: textAllCheckBox.left
                    //anchors.rightMargin: 4
                    font.pixelSize: 14
                    font.family: "PingFang SC"
                    text: qsTr("是否可编辑")
                    font.bold: true
                    color: "#FFFFFF"
                }
                
                Image {
                    id:textAllCheckBox
                    
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
                            layerEditableModel.setAllChecked(allChecked)
                        }
                    }
                }
            }

            ListView{
                id: layerListView
                model: layerEditableModel
                delegate: layerDelegate
                anchors.top: leftTitle.bottom
                //anchors.bottom: parent.bottom
                width: parent.width
                height: parent.height/2
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
    }

    Component {
        id: layerDelegate
        Rectangle {
            id: wrapper
            width: layerListView.width
            height: 36
            //color: "transparent"
            color: index%2==0 ? Qt.rgba(1, 1, 1, 5/255): "transparent"

            Text {
                id: layerIndexText
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
            /*
            Image {
                id:layerIcon
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: layerIndexText.right
                anchors.leftMargin: 16
                width: normalIconSize
                height: normalIconSize
                source: layerIconPath
            }*/

            Text {
                id: layerNameText
                text: name
                renderType: Text.NativeRendering
                anchors.left: layerIndexText.right
                anchors.right: textEditableCheckBox.left
                anchors.leftMargin: 18
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignTop
                elide: Qt.ElideMiddle
                color: "white"
                font.pixelSize: 14
                font.family: "PingFang SC"
            }

            Image {
                id:textEditableCheckBox
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: normalIconSize
                height: normalIconSize
                source: itemChecked ? checkboxOnIconPath : checkboxOffIconPath

                MouseArea{
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onPressed: {
                        layerEditableModel.onLayerEditableStatusChange(index, !itemChecked);
                        allChecked = layerEditableModel.isAllSelected();
                    }
                }
            }
        }
    }
}
