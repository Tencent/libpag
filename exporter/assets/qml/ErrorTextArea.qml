import QtQuick
import QtQuick.Controls

TextArea {
    id: suggestText
    property var targetListView: null

    renderType: Text.NativeRendering
    color: "white"
    verticalAlignment: Text.AlignTop
    font.pixelSize: 14
    font.family: "PingFang SC"
    wrapMode: TextEdit.WrapAnywhere
    padding: 0
    readOnly: true
    selectByMouse: true
    selectionColor: "#1982eb"
    selectedTextColor: "white"
    activeFocusOnPress: true

    background: Rectangle {
        color: "transparent"
    }

    MouseArea {
        id: suggestMouseArea
        property var targetTextArea: suggestText
        property var headPosition: 0
        property var endPosition: 0
        property var isInit: false
        property var hasMoved: false
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: false

        onPressed: function (pressed) {
            hasMoved = false;
            if (suggestText.targetListView) {
                suggestText.targetListView.interactive = false;
            }
            clearSelect();
            targetTextArea.forceActiveFocus();
            console.log("pressed:" + pressed + ", activeFocus:" + targetTextArea.activeFocus);
        }

        onReleased: {
            if (suggestText.targetListView) {
                suggestText.targetListView.interactive = true;
            }
            isInit = false;
            if (!hasMoved) {
                return;
            }
        }

        onPositionChanged: function (mouse) {
            var position = targetTextArea.positionAt(mouse.x, mouse.y);
            if (!isInit) {
                headPosition = position;
                endPosition = position;
                isInit = true;
                return;
            }
            hasMoved = true;
            if (position > headPosition) {
                endPosition = position;
            } else {
                headPosition = position;
            }

            targetTextArea.select(headPosition, endPosition);
        }

        function clearSelect() {
            targetTextArea.select(0, 0);
            headPosition = 0;
            endPosition = 0;
        }
    }

    onFocusChanged: function (focus) {
        if (!focus) {
            suggestMouseArea.clearSelect();
        }
    }
}
