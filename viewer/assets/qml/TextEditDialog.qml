import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform

Popup {
    id: dialog

    required property var textModel

    property bool fauxBold: false
    property bool fauxItalic: false
    property int layerIndex: -1
    property double fontSize: 120.0
    property double strokeWidth: 1.0
    property string fillColor: ""
    property string fontFamily: ""
    property string fontStyle: ""
    property string strokeColor: ""
    property string displatText: ""

    z: 9999
    width: 330
    height: 270
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape

    background: Rectangle {
        anchors.fill: parent
        border.width: 1
        radius: 5
        color: "#20202A"
        opacity: 1

        MouseArea {
            property int mouseStartX: 0
            property int mouseStartY: 0
            property int windowX: 0
            property int windowY: 0

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: function (mouse) {
                mouseStartX = mouseX;
                mouseStartY = mouseY;
                windowX = dialog.x;
                windowY = dialog.y;
            }
            onPositionChanged: function (mouse) {
                let offsetX = mouseX - mouseStartX;
                let offsetY = mouseY - mouseStartY;
                if ((offsetX === (dialog.x - windowX)) && (offsetY === (dialog.y - windowY))) {
                    return;
                }
                dialog.x = windowX + offsetX;
                dialog.y = windowY + offsetY;
            }
        }

        Column {
            spacing: 0
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20

            Row {
                width: parent.width
                height: 40

                Text {
                    id: title
                    width: parent.width - closeButton.width
                    height: parent.height
                    text: qsTr("Edit Text")
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                }

                Button {
                    id: closeButton
                    width: 12
                    height: 12
                    anchors.top: parent.top
                    anchors.topMargin: (parent.height - height) / 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: (parent.height - height) / 2
                    background: Image {
                        width: parent.height
                        height: parent.height
                        source: "qrc:/images/icon-close.png"
                    }

                    onClicked: {
                        dialog.close();
                    }
                }
            }

            Item {
                width: 1
                height: 4
            }

            Row {
                height: 20

                Item {
                    width: 20
                    height: 1
                }

                Label {
                    id: fontFamilyLabel
                    width: 28
                    height: parent.height
                    text: qsTr("Font")
                    font.pixelSize: 12
                    color: "#9B9B9B"
                    verticalAlignment: Text.AlignVCenter
                }

                Item {
                    width: 6
                    height: 1
                }

                ComboBox {
                    id: fontFamilyComboBox
                    width: 220
                    height: parent.height
                    topInset: 0
                    bottomInset: 0
                    model: textModel ? textModel.fontFamilies() : null
                    currentIndex: textModel ? textModel.fontFamilies().indexOf(currentValue) : -1

                    background: Rectangle {
                        color: "#16161D"
                    }

                    contentItem: Text {
                        id: fontFamilyName
                        text: fontFamily
                        leftPadding: 5
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        font.pixelSize: 12
                    }

                    indicator: Image {
                        id: fontsDropdown
                        width: 20
                        anchors.top: parent.top
                        anchors.topMargin: 0
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/images/icon-collapse.png"
                    }

                    delegate: Rectangle {
                        required property string modelData
                        width: fontFamilyComboBox.width
                        height: 26
                        border.width: 1
                        border.color: "#504b4b"
                        color: modelData === fontFamilyName.text ? "#333333" : "#16161D"
                        Text {
                            height: parent.height
                            text: modelData + " 中文示例"
                            color: "#DDDDDD"
                            font.pixelSize: 12
                            font.family: modelData
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            renderType: Text.QtRendering
                            leftPadding: 5
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                fontFamilyName.text = modelData;
                                fontStyleComboBox.currentFontFamily = modelData;
                                let index = textModel.fontStyles(modelData).indexOf(fontStyleName.text);
                                if (index >= 0) {
                                    fontStyleComboBox.currentIndex = index;
                                } else {
                                    fontStyleComboBox.currentIndex = 0;
                                    fontStyleName.text = textModel.fontStyles(modelData)[0];
                                }
                                fontFamilyComboBox.currentIndex = fontFamilyComboBox.model.indexOf(modelData);
                                textModel.changeFontFamily(layerIndex, modelData);
                                dialog.fontFamily = modelData;
                                fontFamilyComboBox.popup.close();
                            }
                        }
                    }
                    popup: Popup {
                        y: fontFamilyComboBox.height - 1
                        width: fontFamilyComboBox.width
                        implicitHeight: Math.min(contentItem.implicitHeight, 300)
                        padding: 0

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: fontFamilyComboBox.popup.visible ? fontFamilyComboBox.delegateModel : null

                            ScrollIndicator.vertical: ScrollIndicator {}
                        }

                        background: Rectangle {
                            border.color: "#333333"
                            color: "#16161D"
                        }
                    }
                }
            }

            Item {
                width: 1
                height: 16
            }

            Row {
                height: 20

                Item {
                    width: 20
                    height: 1
                }

                Label {
                    id: fontStyleLabel
                    width: 28
                    height: parent.height
                    text: qsTr("Style")
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    color: "#9B9B9B"
                }

                Item {
                    width: 6
                    height: 1
                }

                ComboBox {
                    id: fontStyleComboBox

                    property string currentFontFamily: fontFamilyName.text

                    width: 220
                    height: parent.height
                    topInset: 0
                    bottomInset: 0
                    model: textModel ? textModel.fontStyles(currentFontFamily) : null
                    currentIndex: textModel ? textModel.fontStyles(currentFontFamily).indexOf(currentValue) : -1

                    background: Rectangle {
                        color: "#16161D"
                    }

                    contentItem: Text {
                        id: fontStyleName
                        text: fontStyle
                        leftPadding: 5
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        font.pixelSize: 12
                    }

                    indicator: Image {
                        id: fontStyleDropdown
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
                        height: 26
                        color: modelData === fontStyleName.text ? "#333333" : "#16161D"
                        Text {
                            height: parent.height
                            text: modelData + " 中文示例"
                            color: "#DDDDDD"
                            font.pixelSize: 12
                            font.family: fontStyleName.text
                            font.styleName: modelData
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            renderType: Text.QtRendering
                            leftPadding: 5
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                fontStyleName.text = modelData;
                                fontStyleComboBox.currentIndex = fontStyleComboBox.model.indexOf(modelData);
                                textModel.changeFontStyle(layerIndex, modelData);
                                dialog.fontStyle = modelData;
                                fontStyleComboBox.popup.close();
                            }
                        }
                    }

                    popup: Popup {
                        y: fontStyleComboBox.height - 1
                        width: fontStyleComboBox.width
                        implicitHeight: Math.min(contentItem.implicitHeight, 300)
                        padding: 1

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: fontStyleComboBox.popup.visible ? fontStyleComboBox.delegateModel : null
                            ScrollIndicator.vertical: ScrollIndicator {
                                implicitWidth: 7
                                implicitHeight: 7

                                background: Rectangle {
                                    color: "#00000000"
                                }
                            }
                        }

                        background: Rectangle {
                            border.color: "#333333"
                            color: "#16161D"
                        }
                    }
                }
            }

            Item {
                width: 1
                height: 16
            }

            Row {
                height: 20

                Item {
                    width: 20
                    height: 1
                }

                Label {
                    id: fontSizeLabel
                    width: 28
                    height: parent.height
                    text: qsTr("Size")
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    color: "#9B9B9B"
                }

                Item {
                    width: 6
                    height: 1
                }

                Rectangle {
                    width: 98
                    height: parent.height
                    color: "#16161D"

                    Item {
                        width: 6
                        height: 1
                    }

                    TextInput {
                        id: fontSizeTextInput
                        text: fontSize
                        font.pixelSize: 12
                        anchors.fill: parent
                        leftPadding: 5
                        color: "#FFFFFF"
                        verticalAlignment: Text.AlignVCenter
                        clip: true
                        selectByMouse: true
                        validator: RegularExpressionValidator {
                            regularExpression: /[0-9]+\.[0-9]+/
                        }
                        onTextChanged: {
                            dialog.fontSize = fontSizeTextInput.text;
                            textModel.changeFontSize(layerIndex, dialog.fontSize);
                        }
                    }
                }

                Item {
                    width: 20
                    height: 1
                }

                CheckBox {
                    id: boldCheckBox
                    height: parent.height
                    text: qsTr("Bold")
                    font.pixelSize: 12
                    spacing: 0
                    checked: fauxBold
                    topPadding: 0
                    bottomPadding: 0
                    enabled: true

                    indicator: Image {
                        id: boldImg
                        height: 16
                        width: 16
                        anchors.verticalCenter: parent.verticalCenter
                        source: boldCheckBox.checked ? "qrc:/images/checked.png" : "qrc:/images/unchecked.png"
                    }

                    contentItem: Text {
                        text: boldCheckBox.text
                        font: boldCheckBox.font
                        opacity: enabled ? 1.0 : 0.3
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 12
                    }

                    onCheckedChanged: {
                        dialog.fauxBold = boldCheckBox.checked;
                        textModel.changeFauxBold(layerIndex, dialog.fauxBold);
                    }
                }

                Item {
                    width: 10
                    height: 1
                }

                CheckBox {
                    id: italicCheckBox
                    height: parent.height
                    text: qsTr("Italic")
                    font.pixelSize: 12
                    spacing: 0
                    checked: fauxItalic
                    topPadding: 0
                    bottomPadding: 0
                    enabled: true

                    indicator: Image {
                        id: italicImg
                        height: 16
                        width: 16
                        anchors.verticalCenter: parent.verticalCenter
                        source: italicCheckBox.checked ? "qrc:/images/checked.png" : "qrc:/images/unchecked.png"
                    }

                    contentItem: Text {
                        text: italicCheckBox.text
                        font: italicCheckBox.font
                        opacity: enabled ? 1.0 : 0.3
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 12
                    }

                    onCheckedChanged: {
                        dialog.fauxItalic = italicCheckBox.checked;
                        textModel.changeFauxItalic(layerIndex, dialog.fauxItalic);
                    }
                }
            }

            Item {
                width: 1
                height: 16
            }

            Row {
                height: 20

                Item {
                    width: 20
                    height: 1
                }

                Label {
                    id: fontColorLabel
                    width: 28
                    height: parent.height
                    text: qsTr("Color")
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    color: "#9B9B9B"
                }

                Item {
                    width: 6
                    height: 1
                }

                Rectangle {
                    width: 54
                    height: 20
                    color: "#16161D"

                    Rectangle {
                        id: currentFontColor
                        width: 12
                        height: 12
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        color: fillColor
                    }

                    Button {
                        anchors.top: parent.top
                        anchors.topMargin: -10
                        anchors.left: parent.left
                        anchors.leftMargin: 32

                        background: Image {
                            width: 20
                            fillMode: Image.PreserveAspectFit
                            source: "qrc:/images/icon-collapse.png"
                        }

                        onClicked: {
                            selectColorDialog.close();
                            if (selectColorDialog.currentAcceptHandler) {
                                selectColorDialog.accepted.disconnect(selectColorDialog.currentAcceptHandler);
                            }
                            selectColorDialog.color = dialog.fillColor;
                            selectColorDialog.currentAcceptHandler = function () {
                                dialog.fillColor = selectColorDialog.color;
                                textModel.changeFillColor(layerIndex, dialog.fillColor);
                            };
                            selectColorDialog.accepted.connect(selectColorDialog.currentAcceptHandler);
                            selectColorDialog.open();
                        }
                    }
                }

                Item {
                    width: 10
                    height: 1
                }

                Label {
                    id: strokeLbale
                    width: 30
                    height: parent.height
                    text: qsTr("Stroke")
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    color: "#9B9B9B"
                }

                Item {
                    width: 6
                    height: 1
                }

                Rectangle {
                    width: 54
                    height: 20
                    color: "#16161D"

                    Rectangle {
                        id: currentStrokeColor
                        width: 12
                        height: 12
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.left: parent.left
                        anchors.leftMargin: 4
                        color: strokeColor
                    }

                    Button {
                        anchors.top: parent.top
                        anchors.topMargin: -10
                        anchors.left: parent.left
                        anchors.leftMargin: 32

                        background: Image {
                            width: 20
                            fillMode: Image.PreserveAspectFit
                            source: "qrc:/images/icon-collapse.png"
                        }

                        onClicked: {
                            selectColorDialog.close();
                            if (selectColorDialog.currentAcceptHandler) {
                                selectColorDialog.accepted.disconnect(selectColorDialog.currentAcceptHandler);
                            }
                            selectColorDialog.color = dialog.strokeColor;
                            selectColorDialog.currentAcceptHandler = function () {
                                dialog.strokeColor = selectColorDialog.color;
                                textModel.changeStrokeColor(layerIndex, dialog.strokeColor);
                            };
                            selectColorDialog.accepted.connect(selectColorDialog.currentAcceptHandler);
                            selectColorDialog.open();
                        }
                    }
                }

                Item {
                    width: 10
                    height: 1
                }

                Rectangle {
                    width: 52
                    height: parent.height
                    color: "#16161D"

                    TextInput {
                        id: strokeWidthInput
                        text: strokeWidth
                        font.pixelSize: 12
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 5
                        color: "#FFFFFF"
                        clip: true
                        autoScroll: false
                        selectByMouse: true
                        validator: RegularExpressionValidator {
                            regularExpression: /[0-9]+\.[0-9]+/
                        }
                    }

                    Label {
                        height: parent.height
                        text: qsTr("Width")
                        font.pixelSize: 12
                        color: "#7C7C7C"
                        verticalAlignment: Text.AlignVCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 0
                        anchors.top: parent.top
                    }
                }
            }

            Item {
                width: 1
                height: 16
            }

            Row {
                height: 40

                Item {
                    width: 20
                    height: 1
                }

                Label {
                    id: textLabel
                    width: 28
                    height: parent.height / 2
                    text: qsTr("Text")
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    color: "#9B9B9B"
                }

                Item {
                    width: 6
                    height: 1
                }

                Rectangle {
                    width: 220
                    height: parent.height
                    color: "#16161D"

                    Flickable {
                        id: flick
                        anchors.fill: parent
                        contentWidth: textInput.paintedWidth
                        contentHeight: textInput.paintedHeight
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true

                        TextEdit {
                            id: textInput
                            text: displatText
                            font.pixelSize: 12
                            width: flick.width
                            selectByMouse: true
                            leftPadding: 5
                            color: "#FFFFFF"
                            selectionColor: "#55FFFFFF"
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: TextEdit.Wrap

                            onTextChanged: {
                                dialog.displatText = textInput.text;
                                textModel.changeText(layerIndex, textInput.text);
                            }
                        }
                    }
                }
            }

            Item {
                width: 1
                height: 12
            }

            Row {
                width: parent.width
                height: 20
                spacing: 0

                Item {
                    width: parent.width - confirmButton.width - 20
                    height: 1
                }

                Button {
                    id: confirmButton
                    width: 60
                    height: 20

                    background: Rectangle {
                        anchors.fill: parent
                        opacity: enabled ? 1 : 0.3
                        border.width: 1
                        radius: 4
                        color: "#3485F6"
                    }

                    Text {
                        anchors.fill: parent
                        text: qsTr("Confirm")
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: "#FFFFFF"
                    }
                    onClicked: {
                        textModel.updateTextDocument(layerIndex);
                        dialog.close();
                    }
                }
            }
        }
    }

    ColorDialog {
        id: selectColorDialog

        property var currentAcceptHandler: null

        title: qsTr("Select Color")
        color: "#AAAAAA"
    }
}
