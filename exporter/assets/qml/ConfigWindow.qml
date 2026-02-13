import QtQuick
import QtQuick.Controls.Universal
import QtQuick.Window
import QtQuick.Layouts

PAGWindow {
    id: mainWindow
    property int windowWidth: 720

    property int windowHeight: 568

    title: "PAG Config"
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    visible: true
    isWindows: true
    canResize: false
    canMaxSize: false
    maxBtnVisible: false
    minBtnVisible: false
    resizeHandleSize: 5
    titleBarHeight: 32
    windowBackgroundColor: "#14141E"
    titlebarBackgroundColor: "#14141E"
    titleFontSize: 14
    modality: Qt.ApplicationModal

    Item {
        id: dummyFocusItem
        visible: false
        focus: false
    }

    onClosing: function (closeEvent) {
        closeEvent.accepted = true;
        configWindow.onWindowClosing();
    }

    Component.onCompleted: {
        if (typeof configWindow !== 'undefined') {
            let config = configWindow.getCurrentConfig();
            loadConfigToUI(config);
        }
    }

    function loadConfigToUI(config) {
        if (config.language !== undefined)
            languageComboBox.currentIndex = config.language;
        if (config.exportUseCase !== undefined)
            useCaseComboBox.currentIndex = config.exportUseCase;
        if (config.exportVersionControl !== undefined)
            exportVersionComboBox.currentIndex = config.exportVersionControl;
        if (config.tagLevel !== undefined)
            tagLevelInput.displayText = config.tagLevel.toString();
        if (config.bitmapCompressionQuality !== undefined)
            bitMapInput.displayText = config.bitmapCompressionQuality.toString();
        if (config.imageScaleRatio !== undefined)
            bitMapPixelInput.displayText = parseFloat(config.imageScaleRatio).toFixed(1);
        if (config.exportLayerName !== undefined)
            layerNameComboBox.currentIndex = config.exportLayerName ? 0 : 1;
        if (config.exportFonts !== undefined)
            exportFontComboBox.currentIndex = config.exportFonts ? 0 : 1;

        if (config.bitmapQuality !== undefined) {
            if (config.bitmapQuality === 3) {
                frameFormatComboBox.currentIndex = 0;
            } else if (config.bitmapQuality === 2) {
                frameFormatComboBox.currentIndex = 1;
            } else {
                frameFormatComboBox.currentIndex = 0;
            }
        }

        if (config.imageQuality !== undefined)
            qualityInput.displayText = config.imageQuality.toString();
        if (config.exportSizeLimit !== undefined)
            sizeLimitInput.displayText = config.exportSizeLimit.toString();
        if (config.maximumFrameRate !== undefined)
            frameRateInput.displayText = parseFloat(config.maximumFrameRate).toFixed(1);
        if (config.keyframeInterval !== undefined)
            keyFrameInput.displayText = config.keyframeInterval.toString();
    }
    function validateValue(value, min, max, isInteger) {
        var numValue;

        if (isInteger) {
            numValue = parseInt(value);
        } else {
            numValue = parseFloat(value);
        }

        if (isNaN(numValue)) {
            return isInteger ? min.toString() : min.toFixed(1);
        }

        if (numValue < min) {
            numValue = min;
        } else if (numValue > max) {
            numValue = max;
        }

        return isInteger ? numValue.toString() : numValue.toFixed(1);
    }

    function forceCommitAllEditing() {
        var inputs = [tagLevelInput, bitMapInput, bitMapPixelInput, qualityInput, sizeLimitInput, frameRateInput, keyFrameInput];

        for (var i = 0; i < inputs.length; i++) {
            if (inputs[i].activeFocus) {
                inputs[i].focus = false;
            }
        }
        dummyFocusItem.forceActiveFocus();
    }

    function validateAllInputs() {
        if (exportVersionComboBox.currentIndex === 2) {
            tagLevelInput.displayText = validateValue(tagLevelInput.displayText, 1, 9999, true);
        }

        bitMapInput.displayText = validateValue(bitMapInput.displayText, 0, 100, true);
        bitMapPixelInput.displayText = validateValue(bitMapPixelInput.displayText, 1.0, 3.0, false);
        qualityInput.displayText = validateValue(qualityInput.displayText, 0, 100, true);
        sizeLimitInput.displayText = validateValue(sizeLimitInput.displayText, 0, 10000, true);
        frameRateInput.displayText = validateValue(frameRateInput.displayText, 1.0, 120.0, false);
        keyFrameInput.displayText = validateValue(keyFrameInput.displayText, 0, 10000, true);
    }

    function collectConfigFromUI() {
        forceCommitAllEditing();
        validateAllInputs();
        var bitmapQualityValue;
        if (frameFormatComboBox.currentIndex === 0) {
            bitmapQualityValue = 3;
        } else if (frameFormatComboBox.currentIndex === 1) {
            bitmapQualityValue = 2;
        } else {
            bitmapQualityValue = 3;
        }
        return {
            "language": languageComboBox.currentIndex,
            "exportUseCase": useCaseComboBox.currentIndex,
            "exportVersionControl": exportVersionComboBox.currentIndex,
            "tagLevel": parseInt(tagLevelInput.displayText),
            "bitmapCompressionQuality": parseInt(bitMapInput.displayText),
            "imageScaleRatio": parseFloat(bitMapPixelInput.displayText),
            "exportLayerName": layerNameComboBox.currentIndex === 0,
            "exportFonts": exportFontComboBox.currentIndex === 0,
            "bitmapQuality": bitmapQualityValue,
            "imageQuality": parseInt(qualityInput.displayText),
            "exportSizeLimit": parseInt(sizeLimitInput.displayText),
            "maximumFrameRate": parseFloat(frameRateInput.displayText),
            "keyframeInterval": parseInt(keyFrameInput.displayText)
        };
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 0

        PAGRectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#14141E"
            border.width: 0
            radius: 12
            leftTopRadius: true
            rightTopRadius: true
            leftBottomRadius: false
            rightBottomRadius: false

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                TabBar {
                    id: tabBar
                    visible: false
                    currentIndex: 0
                }

                PAGRectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#22222c"
                    radius: 8
                    leftTopRadius: false
                    rightTopRadius: false
                    leftBottomRadius: true
                    rightBottomRadius: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40

                            Row {
                                anchors.centerIn: parent
                                spacing: 2

                                Rectangle {
                                    width: 140
                                    height: 40
                                    color: "transparent"

                                    Text {
                                        text: qsTr("General")
                                        color: tabBar.currentIndex === 0 ? "#ffffff" : "#888888"
                                        font.pixelSize: 15
                                        font.family: "PingFang SC"
                                        anchors.centerIn: parent
                                    }

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: 2
                                        color: "#1982EB"
                                        visible: tabBar.currentIndex === 0
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: tabBar.currentIndex = 0
                                    }
                                }

                                Rectangle {
                                    width: 140
                                    height: 40
                                    color: "transparent"

                                    Text {
                                        text: qsTr("BMP Composition")
                                        color: tabBar.currentIndex === 1 ? "#ffffff" : "#888888"
                                        font.pixelSize: 15
                                        font.family: "PingFang SC"
                                        anchors.centerIn: parent
                                    }

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: 2
                                        color: "#1982EB"
                                        visible: tabBar.currentIndex === 1
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: tabBar.currentIndex = 1
                                    }
                                }
                            }
                        }

                        StackLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.margins: 10
                            currentIndex: tabBar.currentIndex

                            Item {
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 5

                                    GridLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignHCenter
                                        columns: 2
                                        columnSpacing: 5
                                        rowSpacing: 12

                                        Text {
                                            text: qsTr("Language:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGComboBox {
                                            id: languageComboBox
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            model: [qsTr("Auto"), "中文（简体）", "English（US）"]
                                        }

                                        Text {
                                            text: qsTr("Export Use Case:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGComboBox {
                                            id: useCaseComboBox
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            model: [qsTr("General"), qsTr("UI Animation"), qsTr("Video Editing")]
                                        }
                                        Text {
                                            text: qsTr("Export Version Control:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGComboBox {
                                            id: exportVersionComboBox
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            model: ["Stable", "Beta", "Custom"]

                                            onCurrentIndexChanged: {
                                                if (currentIndex === 0) {
                                                    tagLevelInput.displayText = "1023";
                                                } else if (currentIndex === 1) {
                                                    tagLevelInput.displayText = "1023";
                                                }
                                                var modeNames = ["Stable", "Beta", "Custom"];
                                            }
                                        }

                                        Text {
                                            text: qsTr("TAG Level:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGTextInput {
                                            id: tagLevelInput
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            displayText: "1023"
                                            color: "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 4
                                            showUpDownButtons: true
                                            isFloat: false
                                            step: 1

                                            enabled: exportVersionComboBox.currentIndex === 2
                                            opacity: enabled ? 1.0 : 0.6

                                            onEditingFinish: function (text) {
                                                if (exportVersionComboBox.currentIndex === 2) {
                                                    displayText = validateValue(text, 1, 9999, true);
                                                }
                                            }
                                        }

                                        Text {
                                            text: qsTr("Bitmap Compression Quality:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGTextInput {
                                            id: bitMapInput
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            displayText: "80"
                                            color: "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 4
                                            showUpDownButtons: true
                                            isFloat: false
                                            step: 1

                                            onEditingFinish: function (text) {
                                                displayText = validateValue(text, 0, 100, true);
                                            }
                                        }

                                        Text {
                                            text: qsTr("Image Scale Ratio:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGTextInput {
                                            id: bitMapPixelInput
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            displayText: "2"
                                            color: "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 4
                                            showUpDownButtons: true
                                            isFloat: true
                                            step: 0.1
                                            decimals: 1

                                            onEditingFinish: function (text) {
                                                displayText = validateValue(text, 1.0, 3.0, false);
                                            }
                                        }

                                        Text {
                                            text: qsTr("Export Layer Name:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGComboBox {
                                            id: layerNameComboBox
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            model: [qsTr("Yes"), qsTr("No")]
                                        }

                                        Text {
                                            text: qsTr("Export Fonts:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGComboBox {
                                            id: exportFontComboBox
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            model: [qsTr("Yes"), qsTr("No")]
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignRight

                                        Rectangle {
                                            Layout.preferredWidth: 100
                                            Layout.preferredHeight: 30
                                            color: resetMouseArea.pressed ? "#4a4a50" : "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 6

                                            Text {
                                                text: qsTr("Reset Default")
                                                color: "#cccccc"
                                                font.pixelSize: 14
                                                anchors.centerIn: parent
                                            }

                                            MouseArea {
                                                id: resetMouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor

                                                onClicked: {
                                                    forceCommitAllEditing();

                                                    if (typeof configWindow !== 'undefined') {
                                                        let config = configWindow.getDefaultConfig();
                                                        loadConfigToUI(config);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            Item {
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 5

                                    GridLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignHCenter
                                        columns: 2
                                        columnSpacing: 5
                                        rowSpacing: 12

                                        Text {
                                            text: qsTr("Frame Storage Format:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGComboBox {
                                            id: frameFormatComboBox
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            model: ["H.264", "Webp"]
                                        }

                                        Text {
                                            text: qsTr("Image Quality:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGTextInput {
                                            id: qualityInput
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            displayText: "80"
                                            color: "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 4
                                            showUpDownButtons: true
                                            isFloat: false
                                            step: 1

                                            onEditingFinish: function (text) {
                                                displayText = validateValue(text, 0, 100, true);
                                            }
                                        }

                                        Text {
                                            text: qsTr("Export Size Limit（Short Size）:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGTextInput {
                                            id: sizeLimitInput
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            displayText: "720"
                                            color: "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 4

                                            onEditingFinish: function (text) {
                                                displayText = validateValue(text, 0, 10000, true);
                                            }
                                        }

                                        Text {
                                            text: qsTr("Maximum Frame Rate:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGTextInput {
                                            id: frameRateInput
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            displayText: "24"
                                            color: "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 4
                                            showUpDownButtons: true
                                            isFloat: true
                                            step: 0.1
                                            decimals: 1

                                            onEditingFinish: function (text) {
                                                displayText = validateValue(text, 1.0, 120.0, false);
                                            }
                                        }

                                        Text {
                                            text: qsTr("Keyframe Interval:")
                                            color: "#cccccc"
                                            font.pixelSize: 14
                                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                        }

                                        PAGTextInput {
                                            id: keyFrameInput
                                            Layout.preferredWidth: 200
                                            Layout.preferredHeight: 30
                                            displayText: "60"
                                            color: "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 4

                                            onEditingFinish: function (text) {
                                                displayText = validateValue(text, 0, 10000, true);
                                            }
                                        }
                                    }

                                    Item {
                                        Layout.fillHeight: true
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignRight

                                        Rectangle {
                                            Layout.preferredWidth: 100
                                            Layout.preferredHeight: 30
                                            color: bmpResetMouseArea.pressed ? "#4a4a50" : "#383840"
                                            border.color: "#555555"
                                            border.width: 1
                                            radius: 6

                                            Text {
                                                text: qsTr("Reset Default")
                                                color: "#cccccc"
                                                font.pixelSize: 14
                                                anchors.centerIn: parent
                                            }

                                            MouseArea {
                                                id: bmpResetMouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor

                                                onClicked: {
                                                    forceCommitAllEditing();

                                                    if (typeof configWindow !== 'undefined') {
                                                        let config = configWindow.getDefaultConfig();
                                                        loadConfigToUI(config);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"

            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 20
                spacing: 15

                Rectangle {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 30
                    color: cancelMouseArea.pressed ? "#4a4a50" : "#383840"
                    border.color: "#555555"
                    border.width: 1
                    radius: 6

                    Text {
                        text: qsTr("Cancel")
                        color: "#cccccc"
                        font.pixelSize: 14
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: cancelMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            Qt.callLater(function () {
                                mainWindow.close();
                            });
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 30
                    color: exportMouseArea.pressed ? "#1465b8" : "#1982EB"
                    border.color: "#1982EB"
                    border.width: 1
                    radius: 6

                    Text {
                        text: qsTr("OK")
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.bold: true
                        anchors.centerIn: parent
                    }

                    MouseArea {
                        id: exportMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            if (typeof configWindow !== 'undefined') {
                                let configData = collectConfigFromUI();
                                configWindow.updateConfigFromQML(configData);
                                configWindow.saveConfig();
                            }

                            Qt.callLater(function () {
                                mainWindow.close();
                            });
                        }
                    }
                }
            }
        }
    }
}
