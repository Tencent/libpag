import PAG
import QtQuick
import QtQuick.Controls

Item {
    id: viewDelegate

    readonly property real indentation: 20
    readonly property real padding: 5

    required property TreeView treeView
    required property bool isTreeNode
    required property bool expanded
    required property bool hasChildren
    required property int depth
    required property int row
    required property int column
    required property bool current

    required property string name
    required property string value
    required property int layerIdKey
    required property int markerIndexKey
    required property bool isEditableKey

    implicitWidth: treeView.width
    implicitHeight: 22

    Rectangle {
        id: background
        anchors.fill: parent
        color: row === treeView.myCurrentRow ? "#448EF9" : (row % 2 === 0) ? "#2D2D37" : "#20202A"
    }

    TapHandler {
        onTapped: {
            treeView.myCurrentRow = row;
            if (treeView.isExpanded(row)) {
                treeView.collapse(row);
            } else {
                treeView.toggleExpanded(row);
            }
        }
    }

    Loader {
        id: loader
        anchors.fill: parent
        sourceComponent: viewDelegate.isEditableKey ? editComponent : displayComponent
    }

    Component {
        id: displayComponent

        Row {
            anchors.fill: parent
            Item {
                width: viewDelegate.padding + (viewDelegate.depth * viewDelegate.indentation)
                height: 1
            }

            Image {
                width: 8
                height: 8
                anchors.verticalCenter: parent.verticalCenter
                source: (viewDelegate.isTreeNode && viewDelegate.hasChildren) ? "qrc:/images/right-arrow.png" : ""
                visible: true
                rotation: viewDelegate.expanded ? 90 : 0
            }

            Item {
                width: viewDelegate.isTreeNode ? 4 : 0
                height: 1
            }

            Text {
                x: viewDelegate.padding + (viewDelegate.isTreeNode ? (viewDelegate.depth * viewDelegate.indentation + font.pixelSize) : 0)
                height: viewDelegate.height
                text: viewDelegate.name + " : "
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
                textFormat: TextEdit.RichText
                color: viewDelegate.selected ? "#FFFFFF" : "#EEEEEE"
            }

            Text {
                height: viewDelegate.height
                text: viewDelegate.value.length > 20 ? viewDelegate.value.substr(0, 20) : viewDelegate.value
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
                clip: true
                textFormat: TextEdit.RichText
                color: viewDelegate.selected ? "#FFFFFF" : "#EEEEEE"
            }
        }
    }

    Component {
        id: editComponent

        Row {
            anchors.fill: parent
            Item {
                width: viewDelegate.padding + (viewDelegate.depth * viewDelegate.indentation)
                height: 1
            }

            Image {
                id: indicator
                width: 8
                height: 8
                anchors.verticalCenter: parent.verticalCenter
                source: (viewDelegate.isTreeNode && viewDelegate.hasChildren) ? "qrc:/images/right-arrow.png" : ""
                visible: true
                rotation: viewDelegate.expanded ? 90 : 0
            }

            Item {
                width: viewDelegate.isTreeNode ? 4 : 0
                height: 1
            }

            Text {
                id: nameText
                x: viewDelegate.padding + (viewDelegate.isTreeNode ? (viewDelegate.depth * viewDelegate.indentation + font.pixelSize) : 0)
                height: viewDelegate.height
                text: viewDelegate.name + " : "
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
                textFormat: TextEdit.RichText
                color: viewDelegate.selected ? "#FFFFFF" : "#EEEEEE"
            }

            Text {
                id: valueText
                height: viewDelegate.height
                text: viewDelegate.value.length > 20 ? viewDelegate.value.substr(0, 20) : viewDelegate.value
                font.pixelSize: 12
                font.kerning: false
                font.letterSpacing: 0.2
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
                clip: true
                textFormat: TextEdit.RichText
                color: viewDelegate.selected ? "#FFFFFF" : "#EEEEEE"
            }

            ComboBox {
                id: comboBox
                width: 100
                height: viewDelegate.height
                visible: false
                model: editAttributeModel ? editAttributeModel.getAttributeOptions(viewDelegate.name) : []
                currentIndex: editAttributeModel ? Math.max(0, editAttributeModel.getAttributeOptions(viewDelegate.name).indexOf(viewDelegate.value) - 5) : 0

                background: Rectangle {
                    color: "#16161D"
                }
                contentItem: Text {
                    id: displayText
                    text: model.value
                    leftPadding: 5
                    color: "#FFFFFF"
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }
                indicator: Image {
                    id: theIndicator
                    width: 20
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.top: parent.top
                    anchors.topMargin: 0
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/images/icon-collapse.png"
                }
                delegate: Rectangle {
                    required property string modelData

                    width: parent.width
                    height: 22
                    color: modelData === displayText.text ? "#333333" : "#16161D"
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
                            displayText.text = modelData;
                            comboBox.currentIndex = Math.max(0, editAttributeModel.getAttributeOptions(viewDelegate.name).indexOf(modelData) - 5);
                            comboBox.popup.close();
                        }
                    }
                }
                popup: Popup {
                    y: comboBox.height - 1
                    width: comboBox.width
                    implicitHeight: contentItem.implicitHeight
                    padding: 1

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: comboBox.popup.visible ? comboBox.delegateModel : null
                        ScrollIndicator.vertical: ScrollIndicator {}
                    }

                    background: Rectangle {
                        border.color: "#333333"
                        color: "#16161D"
                    }
                }
            }

            Item {
                width: 5
                height: 1
            }

            Image {
                id: editIcon

                property bool inEditing: false

                width: 12
                height: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: viewDelegate.isEditableKey
                source: inEditing ? "qrc:/images/icon-save.png" : "qrc:/images/icon-edit.png"
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (editIcon.inEditing) {
                            valueText.visible = true;
                            if (comboBox.visible) {
                                comboBox.visible = false;
                                let result = editAttributeModel.saveAttribute(viewDelegate.layerIdKey, viewDelegate.markerIndexKey, viewDelegate.name, displayText.text);
                                if (result) {
                                    valueText.text = displayText.text;
                                    let index = treeView.index(viewDelegate.row, viewDelegate.column);
                                    treeViewModel.setData(index, displayText.text);
                                } else {
                                    console.log("Failed to save attribute");
                                    saveFileDialog.filePath = editAttributeModel.getPAGFilePath();
                                    saveFileDialog.visible = true;
                                }
                            }
                        } else {
                            if (viewDelegate.name === "timeStretchMode") {
                                comboBox.visible = true;
                                valueText.visible = false;
                            } else {
                                attributeEditDialog.layerIdKey = viewDelegate.layerIdKey;
                                attributeEditDialog.markerIndexKey = viewDelegate.markerIndexKey;
                                attributeEditDialog.attributeName = viewDelegate.name;
                                attributeEditDialog.attributeValue = displayText.text;
                                attributeEditDialog.visible = true;
                                return;
                            }
                        }
                        editIcon.inEditing = !editIcon.inEditing;
                    }
                }
            }
        }
    }

    AttributeEditDialog {
        id: attributeEditDialog
        visible: false
        parent: Overlay.overlay

        onVisibleChanged: {
            if (visible) {
                x = (parent.width - width) / 2;
                y = (parent.height - height) / 2;
            }
        }

        onAttributeEdited: function (result, attributeValue) {
            if (result) {
                valueText.text = attributeValue;
                let index = treeView.index(viewDelegate.row, viewDelegate.column);
                treeViewModel.setData(index, attributeValue);
            } else {
                console.log("Failed to save attribute");
                saveFileDialog.filePath = editAttributeModel.getPAGFilePath();
                saveFileDialog.visible = true;
            }
        }
    }

    SaveFileDialog {
        id: saveFileDialog
        width: 450
        height: 200
        visible: false
        message: qsTr("Unable to save PAG file to the following path:")
        fileSuffix: "pag"

        saveButton {
            onClicked: {
                let path = saveFileDialog.filePath;
                let lowerPath = path.toLowerCase();
                if (!lowerPath.endsWith(".pag")) {
                    saveFileDialog.close();
                }
                let result = editAttributeModel.saveAttribute(viewDelegate.layerIdKey, viewDelegate.markerIndexKey, viewDelegate.name, displayText.text, path);
                if (result) {
                    valueText.text = displayText.text;
                    saveFileDialog.close();
                } else {
                    console.log("Failed to save file to " + path);
                    pathText.text = path;
                }
            }
        }
    }
}
