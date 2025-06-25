import QtQuick
import QtQuick.Controls

Rectangle {
    id: main

    property string displayText: ""

    signal editingFinish(string text)

    // 监听displayText变化，同步更新显示
    onDisplayTextChanged: {
        showText.text = displayText;
        if (!isInEditing()) {
            textInput.text = displayText;
        }
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
        anchors.right: parent.right
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
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        renderType: Text.NativeRendering
        selectByMouse: true
        clip: true

        onEditingFinished: {
            main.displayText = textInput.text;
            editingFinish(textInput.text);
            showText.text = main.displayText;
        }

        onFocusChanged: function (focus) {
            if (focus || (textInput.text.length === 0)) {
                textInput.text = showText.text;
                textInput.focus = focus;
                showText.focus = !focus;
            }
        }

        Keys.onReturnPressed: {
            focus = false;
        }

        Keys.onEnterPressed: {
            focus = false;
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: {
            textInput.focus = true;
            textInput.forceActiveFocus();
            showText.focus = false;
        }
    }

    function isInEditing() {
        return textInput.activeFocus || main.activeFocus;
    }
}
