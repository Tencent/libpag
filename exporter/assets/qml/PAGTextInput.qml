import QtQuick
import QtQuick.Controls

Rectangle {
    id: main

    property string displayText: ""
    property bool showUpDownButtons: false
    property bool isFloat: false
    property real step: 1.0
    property int decimals: 1

    signal editingFinish(string text)
    signal upButtonClicked
    signal downButtonClicked

    // 添加内部标志避免循环更新
    property bool __internalUpdate: false

    onDisplayTextChanged: {
        if (__internalUpdate)
            return;

        showText.text = displayText;

        // 只在非编辑状态或箭头按钮操作时更新 textInput.text
        if (!isInEditing() || indicatorFocused) {
            __internalUpdate = true;
            textInput.text = displayText;
            __internalUpdate = false;
        }
    }

    property bool indicatorFocused: false

    function formatNumber(value) {
        if (isFloat) {
            return Number(value).toFixed(decimals);
        } else {
            return Math.floor(value).toString();
        }
    }

    function increaseValue() {
        var currentValue = parseFloat(displayText) || 0;
        var newValue = currentValue + step;
        var formattedValue = formatNumber(newValue);
        displayText = formattedValue;
    }

    function decreaseValue() {
        var currentValue = parseFloat(displayText) || 0;
        var newValue = currentValue - step;
        var formattedValue = formatNumber(newValue);
        displayText = formattedValue;
    }

    implicitWidth: 100
    implicitHeight: 30

    Text {
        id: showText
        text: displayText
        font.pixelSize: 14
        font.family: "PingFang SC"
        color: "white"
        visible: !isInEditing()
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: showUpDownButtons ? upDownButtons.left : parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        clip: true
    }

    TextInput {
        id: textInput
        text: displayText
        font.pixelSize: 14
        font.family: "PingFang SC"
        color: "white"
        visible: isInEditing()
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: showUpDownButtons ? upDownButtons.left : parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        renderType: Text.NativeRendering
        selectByMouse: true
        clip: true

        onEditingFinished: {
            if (isFloat && text) {
                var formattedText = formatNumber(parseFloat(text));
                main.displayText = formattedText;
            } else {
                main.displayText = textInput.text;
            }
            editingFinish(main.displayText);
            showText.text = main.displayText;
        }

        onFocusChanged: function (focus) {
            if (main.__internalUpdate)
                return;

            if (focus) {
                if (textInput.text !== showText.text) {
                    textInput.text = showText.text;
                }
            } else {
                main.indicatorFocused = false;
            }
        }

        Keys.onReturnPressed: {
            focus = false;
        }

        Keys.onEnterPressed: {
            focus = false;
        }
    }

    Column {
        id: upDownButtons
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 3
        spacing: 0
        visible: showUpDownButtons && (textInput.focus || main.indicatorFocused)
        width: visible ? 14 : 0

        Rectangle {
            id: upArrow
            width: 12
            height: 10
            color: upMouseArea.containsMouse ? "#555555" : "transparent"
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "▲"
                color: upMouseArea.containsMouse ? "#ffffff" : "#888888"
                font.pixelSize: 8
            }

            MouseArea {
                id: upMouseArea
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    main.indicatorFocused = true;
                    textInput.forceActiveFocus();
                    increaseValue();
                    main.upButtonClicked();
                }
            }
        }

        Rectangle {
            id: downArrow
            width: 12
            height: 10
            color: downMouseArea.containsMouse ? "#555555" : "transparent"
            radius: 2

            Text {
                anchors.centerIn: parent
                text: "▼"
                color: downMouseArea.containsMouse ? "#ffffff" : "#888888"
                font.pixelSize: 8
            }

            MouseArea {
                id: downMouseArea
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    main.indicatorFocused = true;
                    textInput.forceActiveFocus();
                    decreaseValue();
                    main.downButtonClicked();
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: showUpDownButtons ? 18 : 0
        onPressed: {
            main.indicatorFocused = true;
            textInput.focus = true;
            textInput.forceActiveFocus();
        }
    }

    function isInEditing() {
        return textInput.activeFocus || main.activeFocus;
    }
}
