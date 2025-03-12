// import PAG
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: treeViewDelegate

    required property TreeView treeView
    required property bool isTreeNode
    required property bool expanded
    required property bool current
    required property bool selected
    required property bool isEditableKey
    required property int row
    required property int depth
    required property int column
    required property int hasChildren
    required property int layerIdKey
    required property int markerIndexKey
    required property string name
    required property string value

    property var warn1: qsTr("You Don’t Have Permission to Save in This Location %1")
    property int innerWidth: 100
    property alias loaderItem: loader.item

    readonly property real indent: 20
    readonly property real padding: 5

    implicitHeight: 22
    implicitWidth: treeView.width

    Rectangle {
        id: background

        anchors.fill: parent
        color: (row === treeView.myCurrentRow) ? "#448EF9" : (row % 2 === 0) ? "#2D2D37" : "#20202A"
    }

    TapHandler {
        onTapped: {
            treeView.myCurrentRow = row
            if (treeView.isExpanded(row)) {
                treeView.collapse(row)
            } else {
                treeView.toggleExpanded(row)
            }
        }
    }

    Image {
        id: indicator

        x: treeViewDelegate.padding + (treeViewDelegate.depth * treeViewDelegate.indent)
        width: 8
        height: 8
        visible: treeViewDelegate.isTreeNode && treeViewDelegate.hasChildren
        rotation: treeViewDelegate.expanded ? 90 : 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        source: "qrc:/image/icon3.png"
    }

    Loader {
        id: loader

        sourceComponent: treeViewDelegate.isEditableKey ? editorComponent : displayComponent

        onLoaded: {
            loaderItem.getWidth()
        }
    }

    Component {
        id: displayComponent

        Item {
            width: innerWidth
            height: 22

            Text {
                id: atrributeNameText

                x: treeViewDelegate.padding + (treeViewDelegate.isTreeNode ? (treeViewDelegate.depth * treeViewDelegate.indent + font.pixelSize) : 0)
                height: 22
                text: treeViewDelegate.name + " : "
                textFormat: TextEdit.RichText
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                verticalAlignment: Text.AlignVCenter
                anchors.verticalCenter: parent.verticalCenter
                color: treeViewDelegate.selected ? "#FFFFFF" : "#EEEEEE"
            }

            Text {
                id: atrributeValueText

                height:22
                text: treeViewDelegate.value.length > 30 ? treeViewDelegate.value.substr(0,30) : treeViewDelegate.value
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                textFormat: TextEdit.RichText
                verticalAlignment: Text.AlignVCenter
                clip:true
                anchors.left: atrributeNameText.right
                anchors.leftMargin:1
                color: treeViewDelegate.selected ? "#FFFFFF" : "#EEEEEE"

                onTextChanged: {
                    getWidth()
                }
            }

            function getWidth() {
                innerWidth = atrributeNameText.width + atrributeValueText.width + 20
            }
        }
    }

    Component {
        id: editorComponent

        Item  {
            height:22
            width: innerWidth

            Text {
                id: atrributeNameText

                x: treeViewDelegate.padding + (treeViewDelegate.isTreeNode ? (treeViewDelegate.depth * treeViewDelegate.indent + font.pixelSize) : 0)
                height:22
                text: treeViewDelegate.name + " : "
                textFormat: TextEdit.RichText
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                verticalAlignment: Text.AlignVCenter
                anchors.verticalCenter: parent.verticalCenter
                color: treeViewDelegate.selected ? "#FFFFFF" : "#EEEEEE"
            }

            Text {
                id: atrributeValueText

                height: 22
                text: treeViewDelegate.value.length > 20 ? treeViewDelegate.value.substr(0,20) : treeViewDelegate.value
                textFormat: TextEdit.RichText
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                verticalAlignment: Text.AlignVCenter
                anchors.left: atrributeNameText.right
                anchors.leftMargin: 1

                clip: true
                color: treeViewDelegate.selected ?"#FFFFFF": "#EEEEEE"
                onTextChanged: {
                    getWidth()
                }
            }

            AttributeEditDialog {
                id: attributeEditDialog

                x: -width
                y: 0

                onActivated: {
                    console.log("-----saveResult: ", saveResult,"  textString :", textString)
                    if(saveResult) {
                        atrributeValueText.text = textString
                        treeViewDelegate.value = textString
                        pagTreeViewModel.setData(treeViewDelegate.index, textString, "value")
                    } else {
                        attributeSaveErrorWindow.visible = true
                        attributeSaveErrorWindow.message = warn1.arg(pagEditAttributeModel.getPAGFilePath())
                        attributeSaveErrorWindow.raise()
                    }
                }
            }

            ComboBox {
                id: attributeSelect

                width: 100
                height: 22
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: atrributeNameText.right
                anchors.leftMargin: 1
                visible: false

                model: pagEditAttributeModel.getTimeStretchModes()
                currentIndex: Math.max(0, pagEditAttributeModel.getTimeStretchModes().indexOf(currentValue) - 5)

                background: Rectangle {
                    color: "#16161D"
                }

                contentItem: Text {
                    id: timeStretchModeName

                    text: model.value
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 5
                    color: "#FFFFFF"
                }

                indicator: Image {
                    id: attributeDropdown

                    width: 20
                    anchors.top: parent.top
                    anchors.topMargin: 0
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/image/icon-collapse.png"
                }
                delegate: Rectangle {
                    required property string modelData

                    width: parent.width
                    height: 22
                    color: (modelData === timeStretchModeName.text) ? "#333333": "#16161D"

                    Text {
                        height: parent.height
                        text: modelData
                        font.pixelSize: 12
                        font.family: modelData
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        renderType: Text.QtRendering
                        color: "#DDDDDD"
                        leftPadding: 5
                    }

                    MouseArea {
                        anchors.fill: parent

                        onClicked: {
                            console.log("clicked:", modelData)
                            timeStretchModeName.text = modelData
                            attributeSelect.currentIndex = Math.max(0, pagEditAttributeModel.getTimeStretchModes().indexOf(modelData) - 5)
                            attributeSelect.popup.close()
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
                id: attributeEdit

                width: 12
                height: 12
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: atrributeValueText.right
                anchors.leftMargin: 10
                visible: treeViewDelegate.isEditableKey
                source: "qrc:/image/icon-edit.png"

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        if(model.name === "timeStretchMode") {
                            attributeSelect.visible = true
                            atrributeValueText.visible = false
                            attributeEdit.visible = false
                            attributeSave.visible = true
                        } else {
                            attributeEditDialog.textString = treeViewDelegate.value
                            attributeEditDialog.layerIdKey = treeViewDelegate.layerIdKey
                            attributeEditDialog.markerIndexKey = treeViewDelegate.markerIndexKey
                            attributeEditDialog.attributeName = treeViewDelegate.name
                            attributeEditDialog.open()
                        }
                    }
                }
            }

            Image {
                id:attributeSave

                width: 12
                height: 12
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: attributeSelect.right
                anchors.leftMargin: 10
                visible: false
                source: "qrc:/image/icon-save.png"

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked:{
                        console.log("----model.value.LayerIdKey", treeViewDelegate.layerIdKey)
                        console.log("----model.value.MarkerIndexKey", treeViewDelegate.markerIndexKey)
                        console.log("----attributeSelect", attributeSelect.text)

                        atrributeValueText.visible = true
                        attributeEdit.visible = true
                        attributeSave.visible = false

                        if(attributeSelect.visible){
                            attributeSelect.visible = false

                            let saveResult = pagEditAttributeModel.savePAGFileAttribute(treeViewDelegate.layerIdKey, treeViewDelegate.markerIndexKey, treeViewDelegate.name, timeStretchModeName.text)
                            if(saveResult) {
                                atrributeValueText.text = timeStretchModeName.text
                                pagTreeViewModel.setData(treeViewDelegate.index, timeStretchModeName.text, "value")
                            }else {
                                attributeSaveErrorWindow.visible = true
                                attributeSaveErrorWindow.message = warn1.arg(pagEditAttributeModel.getPAGFilePath())
                                attributeSaveErrorWindow.raise()
                            }
                        }
                    }
                }
            }

            function getWidth() {
                innerWidth = atrributeNameText.width + atrributeValueText.width + attributeEdit.width + 20
            }
        }
    }
}