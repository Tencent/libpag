import QtQuick
import QtQuick.Controls
import Qt.labs.platform

Popup {
    id: root

    property bool fauxBold: false
    property bool fauxItalic: false
    property int layerIndex: 0
    property string fillColor: ""
    property string fontStyle: ""
    property string fontFamily: ""
    property string textString: ""
    property string strokeColor: ""
    property real strokeWidth: 1.0
    property real fontSize: 120.0


    x: (parent.width / 2) - (root.width / 2)
    y: (parent.height / 2) - (root.height / 2)
    width: 330
    height: 270
    modal: true
    focus: true
    closePolicy: Popup.OnEscape

    background: rect

    Rectangle {
        id: rect

        anchors.fill: parent
        color: "#20202A"
        border.width: 1
        opacity: 1

        Text {
            id: dialogTitle

            width: parent.width
            height: 52
            text: qsTr("Text Edit")
            font.pixelSize: 12
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            color: "#FFFFFF"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: 20

            MouseArea {
                property point clickPoint: "0,0"

                anchors.fill: parent
                acceptedButtons: Qt.LeftButton

                onPressed: {
                    clickPoint = Qt.point(mouse.x, mouse.y)
                }

                onPositionChanged: {
                    let offset = Qt.point(mouse.x - clickPoint.x, mouse.y - clickPoint.y)
                    setDlgPoint(offset.x, offset.y)
                }
            }
        }

        Label {
            id: labelFont

            x: 32
            width: 28
            height: 17
            text: qsTr("Font")
            font.pixelSize: 12
            color: "#9B9B9B"
            anchors.top: dialogTitle.bottom
            anchors.topMargin: 4
        }

        ComboBox {
            id: cbFontFamily

            width: 224
            height: 20
            anchors.top: dialogTitle.bottom
            anchors.topMargin: 4
            anchors.left: labelFont.right
            anchors.leftMargin: 6

            model: textLayerModel.getFontFamilies()
            currentIndex: textLayerModel.getFontFamilies().indexOf(modelData)

            background: Rectangle {
                color: "#16161D"
            }

            contentItem: Text {
                id: fontFamilyName

                text: fontFamily
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                color: "#FFFFFF"
                leftPadding: 5
            }

            indicator: Image {
                id: fontsDropdown

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

                width: cbFontFamily.width
                height: 26
                color: (modelData === fontFamilyName.text) ? "#333333": "#16161D"

                Text {
                    height: parent.height
                    text: modelData + " 中文示例"
                    font.pixelSize: 12
                    font.family: modelData
                    elide: Text.ElideRight
                    renderType: Text.QtRendering
                    verticalAlignment: Text.AlignVCenter
                    color: "#DDDDDD"
                    leftPadding: 5
                }

                MouseArea {
                    anchors.fill: parent

                    onClicked: {
                        console.log("clicked: ", modelData)
                        fontFamilyName.text = modelData
                        cbFontStyle.currentFontFamily = modelData
                        let index = textLayerModel.getFontStyles(modelData).indexOf(fontStyleName.text)
                        if (index >= 0) {
                            cbFontStyle.currentIndex = index
                        } else {
                            cbFontStyle.currentIndex = 0
                            fontStyleName.text = textLayerModel.getFontStyles(modelData)[0]
                        }
                        cbFontFamily.currentIndex = Math.max(0, textLayerModel.getFontFamilies().indexOf(modelData))
                        console.log("current index: ", cbFontFamily.currentIndex)
                        textLayerModel.onFontFamilyChanged(layerIndex, modelData)
                        cbFontFamily.popup.close()
                    }
                }
            }

            popup: Popup {
                y: cbFontFamily.height - 1
                width: cbFontFamily.width
                implicitHeight: Math.min(contentItem.implicitHeight, 300)
                padding: 0

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: cbFontFamily.popup.visible ? cbFontFamily.delegateModel : null

                    ScrollIndicator.vertical: ScrollIndicator { }
                }

                background: Rectangle {
                    border.color: "#333333"
                    color: "#16161D"
                }
            }
        }

        Label {
            id: labelFontStyle

            x: 32
            width: 28
            height: 17
            text: qsTr("Style")
            font.pixelSize: 12
            color: "#9B9B9B"
            anchors.top: labelFont.bottom
            anchors.topMargin: 16
        }

        ComboBox {
            id: cbFontStyle

            property string currentFontFamily: fontFamilyName.text

            width: 224
            height: 20
            anchors.top: labelFont.bottom
            anchors.topMargin: 16
            anchors.left: labelFontStyle.right
            anchors.leftMargin: 6

            model: textLayerModel.getFontStyles(currentFontFamily)
            currentIndex: textLayerModel.getFontStyles(currentFontFamily).indexOf(delegate.modelData)

            background: Rectangle {
                color: "#16161D"
            }

            contentItem: Text {
                id: fontStyleName

                text: fontStyle
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                color: "#FFFFFF"
                leftPadding: 5
            }

            indicator: Image {
                id: fontStyleDropdown

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
                height: 26
                color: (modelData === fontStyleName.text) ? "#333333": "#16161D"

                Text {
                    height: parent.height
                    text: modelData + " 中文示例"
                    font.pixelSize: 12
                    font.family: fontStyleName.text
                    font.styleName: modelData
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    renderType: Text.QtRendering
                    color: "#DDDDDD"
                    leftPadding: 5
                }

                MouseArea {
                    anchors.fill: parent

                    onClicked: {
                        console.log("font style clicked: ", modelData)
                        fontStyleName.text = modelData
                        cbFontStyle.currentIndex = Math.max(0, textLayerModel.getFontStyles(cbFontStyle.currentFontFamily).indexOf(modelData))
                        textLayerModel.onFontStyleChanged(layerIndex, modelData)
                        cbFontStyle.popup.close()
                    }
                }
            }

            popup: Popup {
                y: cbFontStyle.height - 1
                width: cbFontStyle.width
                implicitHeight: Math.min(contentItem.implicitHeight, 300)
                padding: 1

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight

                    model: cbFontStyle.popup.visible ? cbFontStyle.delegateModel : null

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

        Label {
            id: labelFontSize

            x: 32
            width: 28
            height: 17
            text: qsTr("Size")
            font.pixelSize: 12
            anchors.top: labelFontStyle.bottom
            anchors.topMargin: 16
            color: "#9B9B9B"
        }

        Rectangle {
            id: rectangleFontSize

            width: 98
            height: 20
            anchors.top: labelFontStyle.bottom
            anchors.topMargin: 16
            anchors.left: labelFontSize.right
            anchors.leftMargin: 6
            color: "#16161D"

            TextInput {
                id: textSizeInput

                text: fontSize
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                anchors.fill: parent
                leftPadding: 5
                clip: true
                color: "#FFFFFF"
                selectByMouse: true

                validator: RegularExpressionValidator {
                    regularExpression: /[0-9]+\.[0-9]+/
                }

                onTextChanged: {
                    fontSize = textSizeInput.text
                    textLayerModel.onFontSizeChanged(layerIndex, textSizeInput.text)
                }
            }
        }

        CheckBox {
            id: boldCheckBox

            text: qsTr("Bold")
            font.pixelSize: 12
            checked: fauxBold
            spacing: 0
            anchors.top: labelFontStyle.bottom
            anchors.topMargin: 3
            anchors.left: rectangleFontSize.right
            anchors.leftMargin: 20
            enabled: true

            indicator: Image {
                id: boldImg

                width: 16
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                source: boldCheckBox.checked ? "qrc:/image/checked.png" : "qrc:/image/unchecked.png"
            }

            contentItem: Text {
                text: boldCheckBox.text
                font: boldCheckBox.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                opacity: enabled ? 1.0 : 0.3
                color: "#FFFFFF"
                leftPadding: 12
            }

            onCheckedChanged: {
                fauxBold = checked
                textLayerModel.onFauxBoldChanged(layerIndex, checked)
            }
        }

        CheckBox {
            id: italicCheckBox

            text: qsTr("Italic")
            font.pixelSize: 12
            anchors.top: labelFontStyle.bottom
            anchors.topMargin: 3
            anchors.left: boldCheckBox.right
            anchors.leftMargin: 20
            checked: fauxItalic
            spacing: 0
            enabled: true

            indicator: Image {
                id: italicImg

                width: 16
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                source: italicCheckBox.checked ? "qrc:/image/checked.png" : "qrc:/image/unchecked.png"
            }

            contentItem: Text {
                text: italicCheckBox.text
                font: italicCheckBox.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                opacity: enabled ? 1.0 : 0.3
                color: "#FFFFFF"

                leftPadding: 12
            }

            onCheckedChanged: {
                fauxItalic = checked
                textLayerModel.onFauxItalicChanged(layerIndex, checked)
            }
        }

        Label {
            id: labelFontColor

            x: 32
            width: 28
            height: 17
            text: qsTr("Color")
            font.pixelSize: 12
            anchors.top: labelFontSize.bottom
            anchors.topMargin: 16
            color: "#9B9B9B"
        }

        Rectangle {
            id: rectangleFontColor

            width: 54
            height: 20
            anchors.top: labelFontSize.bottom
            anchors.topMargin: 16
            anchors.left: labelFontColor.right
            anchors.leftMargin: 6
            color: "#16161D"

            Rectangle {
                id: currentColor

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
                    id: moreColor

                    width: 20
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/image/icon-collapse.png"
                }

                onClicked: {
                    colorDialog.open()
                }
            }
        }

        Item {
            id: stroke

            width: 100
            height: 20
            anchors.top: labelFontSize.bottom
            anchors.topMargin: 16
            anchors.left: rectangleFontColor.right
            anchors.leftMargin: 10

            Label {
                id: "strokeLabel"

                height: 17
                text: qsTr("Stroke")
                font.pixelSize: 12
                anchors.top: parent.top
                anchors.topMargin: 2
                anchors.left: parent.left
                color: "#9B9B9B"
            }

            Rectangle {
                width: 54
                height: 20
                anchors.left: strokeLabel.right
                anchors.leftMargin: 5
                anchors.top: parent.top
                color: "#16161D"

                Rectangle {
                    id: currentBorderColor

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
                        id: moreBorderColor

                        width: 20
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/image/icon-collapse.png"
                    }

                    onClicked: {
                        borderColorDialog.open()
                    }
                }
            }

        }

        Rectangle {
            id: rectangleStrokeWidth

            width: 48
            height: 20
            anchors.top: labelFontSize.bottom
            anchors.topMargin: 16
            anchors.left: stroke.right
            anchors.leftMargin: 16
            color: "#16161D"

            TextInput {
                id: borderWidthInput

                text: strokeWidth
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                anchors.fill: parent
                anchors.rightMargin: 15
                leftPadding: 5
                color: "#FFFFFF"
                clip: true
                autoScroll: false
                selectByMouse: true

                validator: RegularExpressionValidator {
                    regularExpression: /[0-9]+\.[0-9]+/
                }

                onTextChanged: {
                    strokeWidth = borderWidthInput.text
                    textLayerModel.onStrokeWidthChanged(layerIndex, borderWidthInput.text)
                }
            }

            Label {
                height: parent.height
                text: qsTr("Width")
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: 0
                color: "#7C7C7C"
            }
        }

        Label {
            id: labelDisplay

            x: 32
            width: 28
            height: 17
            text: qsTr("Text")
            font.pixelSize: 12
            anchors.top: labelFontColor.bottom
            anchors.topMargin: 16
            color: "#9B9B9B"
        }

        Rectangle {
            width: 224
            height: 40
            anchors.top: labelFontColor.bottom
            anchors.topMargin: 16
            anchors.left: labelDisplay.right
            anchors.leftMargin: 6
            color: "#16161D"

            Flickable {
                id: flick

                anchors.fill: parent
                contentWidth: textInput.paintedWidth
                contentHeight: textInput.paintedHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                TextEdit {
                    id: textInput

                    width: flick.width
                    text: textString
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                    selectByMouse: true
                    leftPadding: 5
                    selectionColor: "#55FFFFFF"
                    wrapMode: TextEdit.Wrap

                    onTextChanged: {
                        console.log("currentText:" + textInput.text)
                        textString = textInput.text
                        textLayerModel.onTextChanged(layerIndex, textInput.text)
                    }

                    onCursorRectangleChanged: {
                        flick.ensureVisible(cursorRectangle)
                    }
                }

                function ensureVisible(r)
                {
                    if (contentX >= r.x) {
                        contentX = r.x
                    } else if ((contentX + width) <= (r.x + r.width)) {
                        contentX = r.x + r.width - width
                    }
                    if (contentY >= r.y) {
                        contentY = r.y
                    } else if ((contentY + height) <= (r.y + r.height)) {
                        contentY = r.y + r.height - height
                    }
                }
            }
        }

        Button {
            width: 60
            height: 20
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 25
            anchors.right: parent.right
            anchors.rightMargin: 40

            background: Rectangle {
                anchors.fill: parent
                opacity: enabled ? 1 : 0.3
                border.width: 1
                radius: 4
                color: "#6D94B0"
            }

            Text {
                text: qsTr("Confirm")
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                anchors.fill: parent
                color: "#FFFFFF"
            }

            onClicked: {
                textLayerModel.onConfirmTextEditDialog(layerIndex)
                root.close()
            }
        }

        Button {
            width: 12
            height: 12
            anchors.top: parent.top
            anchors.topMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20

            background: Image {
                width: 12
                height: 12
                fillMode: Image.PreserveAspectFit
                source: "qrc:/image/icon-close.png"
            }

            onClicked: {
                textLayerModel.onCloseTextEditDialog(layerIndex)
                root.close()
            }
        }
    }

    ColorDialog {
        id: colorDialog

        title: qsTr("Select a color")
        color: "#AAAAAA"

        onAccepted: {
            console.log("You chose: " + colorDialog.color)
            currentColor.color = colorDialog.color
            textLayerModel.onFillColorChanged(layerIndex, colorDialog.color)
        }
    }

    ColorDialog {
        id: borderColorDialog

        title: qsTr("Select a color")
        color: "#AAAAAA"

        onAccepted: {
            console.log("You chose: " + borderColorDialog.color)
            currentBorderColor.color = borderColorDialog.color
            textLayerModel.onStrokeColorChanged(layerIndex, borderColorDialog.color)
        }
    }

    onOpened: {
        console.log("onOpened strokeColor:" + strokeColor + ", fillColor:" + fillColor + ", fontFamily:" + fontFamily, "fontStyle:" + fontStyle)
        currentColor.color = fillColor
        currentBorderColor.color = strokeColor
        fontFamilyName.text = fontFamily
        fontStyleName.text = fontStyle
        cbFontFamily.currentIndex = textLayerModel.getFontFamilies().indexOf(fontFamily)
        cbFontStyle.currentIndex = textLayerModel.getFontStyles(fontFamily).indexOf(fontStyle)
    }

    function setDlgPoint(dlgX, dlgY) {
        root.x = root.x + dlgX
        root.y = root.y + dlgY
    }
}
