
import QtQuick 2.5
import QtQuick.Controls 2.1

TextArea {
    property var beginPosition: 0

    id: suggestText
    renderType: Text.NativeRendering
    color: "white"
    verticalAlignment: Text.AlignTop
    font.pixelSize: 12
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
        property var targetTextArea: suggestText
        property var headPosition : 0
        property var endPosition: 0
        property var isInit: false
        property var hasMoved: false
        property var targetListView: errorListView

        id: suggestMouseArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: false

        onPressed: pressed => {
            hasMoved = false
            targetListView.interactive = false
            clearSelect()
            targetTextArea.forceActiveFocus()
            console.log("pressed:" + pressed + ", activeFocus:" + targetTextArea.activeFocus)
        }

        onReleased:{
            targetListView.interactive = true
            isInit = false
            if(!hasMoved){
                isFold = !isFold;
                return
            }
        }

        onPositionChanged: mouse => {
            var position = targetTextArea.positionAt(mouse.x, mouse.y)
            if(position <= targetTextArea.beginPosition){
                position = targetTextArea.beginPosition
            }
            if(!isInit){
                headPosition = position
                endPosition = position
                isInit = true
                return
            }
            hasMoved = true
            if(position > headPosition){
                endPosition = position
            }else{
                headPosition = position
            }
            
            targetTextArea.select(headPosition, endPosition)
        }

        function clearSelect() {
            targetTextArea.select(0,0)
            headPosition = 0
            endPosition = 0
        }
    }

    onFocusChanged: focus => {
        if(!focus){
            suggestMouseArea.clearSelect()
        }
    }
}
