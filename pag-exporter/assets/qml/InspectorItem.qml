import QtQuick 2.13
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.13
import QtQml.Models 2.13
import PAG 1.0

Item {
    property var model;
    property int innerWidth: 100;

    property alias loaderItem: loader.item

    width: innerWidth;
    implicitWidth:innerWidth;
    Loader {
        id:loader
        sourceComponent: model.value.IsEditableKey ? editorComponent: displayComponent
        onLoaded: {
            loaderItem.getWidth()
        }
    }

    Component {
        id: displayComponent

        Item {
            function getWidth() {
                innerWidth = atrributeNameText.width+atrributeValueText.width+20;
            }
            height:22
            width: innerWidth;

            Text {
                id:atrributeNameText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                // anchors.leftMargin:10*(model.depth+2)
                anchors.leftMargin:0
                height:22
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                textFormat:TextEdit.RichText
                color: model.selected ?"#FFFFFF": "#EEEEEE"
                text: model.value.name + " : "
            }


            Text {
                id:atrributeValueText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: atrributeNameText.right
                anchors.leftMargin:1
                height:22
                clip:true
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                textFormat:TextEdit.RichText
                color: model.selected ?"#FFFFFF": "#EEEEEE"
                text: model.value.value.length>30?model.value.value.substr(0,30):model.value.value
                onTextChanged: getWidth()
            }
        }
    }

    Component {
        id: editorComponent

        Item  {
            function getWidth() {
                innerWidth = atrributeNameText.width+atrributeValueText.width + attributeEdit.width + 20;
            }

            height:22
            width: innerWidth;

            Text {
                id:atrributeNameText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                // anchors.leftMargin:10*(model.depth+2)
                anchors.leftMargin:0
                height:22
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                textFormat:TextEdit.RichText
                color: model.selected ?"#FFFFFF": "#EEEEEE"
                text: model.value.name + " : "
            }


            Text {
                id:atrributeValueText
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: atrributeNameText.right
                anchors.leftMargin:1
                height:22
                clip:true
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                textFormat:TextEdit.RichText
                color: model.selected ?"#FFFFFF": "#EEEEEE"
                text: model.value.value.length>20?model.value.value.substr(0,20):model.value.value
                onTextChanged: getWidth()
            }



            AttributeEditDialog {

                x: -width
                y: 0
                id: attributeEditDialog
                onActivated: {
                    console.log("-----saveResult: ", saveResult,"  textString :", textString)
                    if(saveResult) {
                        atrributeValueText.text = textString
                        model.value.value = textString;
                        pagTreeViewModel.setData(model.index, textString, "value")
                    } else {
                        attributeSaveErrorWindow.visible=true
                        attributeSaveErrorWindow.message = "PAGViewer 无法将修改保存到 <br>" + pagEditAttributeModel.getPAGFilePath()+", <br> 请检查您是否有修改该文件的权限。"
                        attributeSaveErrorWindow.raise()
                    }
                }
            }
            ComboBox {
                id:attributeSelect
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: atrributeNameText.right
                anchors.leftMargin:1
                height:22
                width:100
                visible:false
                model: pagEditAttributeModel.getTimeStretchModes()
                currentIndex:Math.max(0,pagEditAttributeModel.getTimeStretchModes().indexOf(modelData)-5)
                background: Rectangle {
                    color: "#16161D"
                }
                contentItem: Text {
                    id: timeStretchModeName
                    text: model.value.value
                    leftPadding: 5
                    color: "#FFFFFF"
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }
                indicator: Image {
                    id: attributeDropdown
                    width: 20
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.top: parent.top
                    anchors.topMargin: 0
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/images/icon-collapse.png"
                }
                delegate: Rectangle {
                    width: parent.width
                    height: 22
                    color: modelData == timeStretchModeName.text? "#333333": "#16161D"
                    Text {
                        text: modelData
                        color: "#DDDDDD"
                        font.family: modelData
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12
                        height: parent.height
                        renderType: Text.QtRendering
                        leftPadding: 5
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            console.log("clicked:", modelData)
                            timeStretchModeName.text = modelData;
                            attributeSelect.currentIndex = Math.max(0,pagEditAttributeModel.getTimeStretchModes().indexOf(modelData)-5);
                            // textLayerModel.onFontFamilyChanged(layerIndex, modelData);
                            attributeSelect.popup.close();
                        }
                    }
                }
                popup: Popup {
                    y: attributeSelect.height - 1
                    width: attributeSelect.width
                    implicitHeight: contentItem.implicitHeight
                    padding: 1

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: attributeSelect.popup.visible ? attributeSelect.delegateModel : null
                        ScrollIndicator.vertical: ScrollIndicator { }
                    }

                    background: Rectangle {
                        border.color: "#333333"
                        color: "#16161D"
                    }
                }
            }
            Image {
                id:attributeEdit
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: atrributeValueText.right
                anchors.leftMargin:10
                // anchors.right:parent.right
                // anchors.rightMargin:10
                visible:model.value.IsEditableKey
                source: "qrc:/images/icon-edit.png"
                width: 12
                height: 12
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked:{
                            if(model.value.name == "timeStretchMode") {
                                attributeSelect.visible = true

                                atrributeValueText.visible=false
                                attributeEdit.visible=false
                                attributeSave.visible=true
                            } else {

                                attributeEditDialog.textString = model.value.value
                                attributeEditDialog.layerIdKey = model.value.LayerIdKey
                                attributeEditDialog.markerIndexKey = model.value.MarkerIndexKey
                                attributeEditDialog.attributeName = model.value.name
                                attributeEditDialog.open()
                            }
                        }
                    }
            }

            Image {
                id:attributeSave
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: attributeSelect.right
                anchors.leftMargin:10
                visible:false
                source: "qrc:/images/icon-save.png"
                width: 12
                height: 12
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked:{

                            console.log("-----model.value.LayerIdKey", model.value.LayerIdKey);
                            console.log("------model.value.MarkerIndexKey",model.value.MarkerIndexKey);

                            console.log("----attributeSelect", attributeSelect.text);

                            atrributeValueText.visible=true
                            attributeEdit.visible=true
                            attributeSave.visible=false

                            if(attributeSelect.visible){
                                attributeSelect.visible=false
                                
                                var saveResult = pagEditAttributeModel.savePAGFileAttribute(model.value.LayerIdKey,model.value.MarkerIndexKey,model.value.name, timeStretchModeName.text)
                                if(saveResult) {
                                    atrributeValueText.text = timeStretchModeName.text;
                                    pagTreeViewModel.setData(model.index, timeStretchModeName.text, "value")
                                }else {
                                    attributeSaveErrorWindow.visible=true
                                    attributeSaveErrorWindow.message = "PAGViewer 无法将修改保存到 <br>" + pagEditAttributeModel.getPAGFilePath()+", <br> 请检查您是否有修改该文件的权限。"
                                    attributeSaveErrorWindow.raise()
                                }
                            }
                        }
                    }
            }
        }

    }
}
