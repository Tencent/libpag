import QtQuick 2.5
import QtQuick.Controls 2.1

Item{
    property var fpsDuration: 0
    property var colorDisable: "FFFFFF"

    anchors.left: parent.left
    anchors.leftMargin: secondPartMarginLeft
    anchors.verticalCenter: parent.verticalCenter
    width: editWidth
    height: editHeight

    signal durationDataChange(int resultData)

    Rectangle{
        id: editContainer
        anchors.fill: parent
        color: isEditing() ? "white" : editBackColor
        radius: 2
        clip: true
        border.width: isEditing() ? 1 : 0
        border.color: "blue"

        MouseArea{
            anchors.fill: parent
            onPressed: {
                editText.focus = true
            }
        }

        Text{
            id: showText
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 14
            font.family: "PingFang SC"
            color: "white"
            text: "00:00:00:00"
            visible: !isEditing()
        }

        TextInput{
            id: editText
            font.pixelSize: 14
            font.family: "PingFang SC"
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: "00:00:00:00"
            color: "black"
            renderType: Text.NativeRendering
            selectByMouse: true
            clip: true
            visible: isEditing()

            onEditingFinished:{
                console.log("onEditingFinished, current text:" + editText.text)
                var editContent = editText.text
                if(editContent.length > 21){
                    editContent = editContent.substring(0, 21)
                    console.log("editContent too long, substring to:" + editContent)
                }
                var result = verifyInput(editContent)
                if(result.length != 0){
                    showText.text = result
                    fpsDuration = textToFps(result)
                    emit: durationDataChange(fpsDuration)
                }
            }

            onCursorVisibleChanged:{
                if(isCursorVisible){
                    text = showText.text
                }
            }

            Keys.onReturnPressed:{
                focus = false
            }

             Keys.onEnterPressed:{
                 focus = false
             }
        }
    }

    // 对齐 AE 中时间输入校验，裁剪有效数字部分，并按对应帧数进位
    function verifyInput(content) {
        var numberList = []
        var contentList = content.split(":")
        var reg = /\d+/
        console.log("content:" + content + ", contentList size:" + contentList.length)

        for(var i = 0; i < contentList.length; i++){
            var part = contentList[i]
            var numbers = part.match(reg)
            if(numbers){
               console.log("part:" + part + ", numbers size:" + numbers.length)
               numberList.push(numbers[0])
            }else{
                numberList.push("00")
            }
        }

        var result = ""
        if(numberList.length == 0){
            console.log("no number data")
            return result
        }

        var tmpNumberList = []
        var needAddZeroSize = 4 - numberList.length
        console.log("needAddZeroSize:" + needAddZeroSize)
        for(var i = 0; i < needAddZeroSize; i++){
            tmpNumberList.push("00");
        }
        for(var i = 0; i < numberList.length; i++){
            tmpNumberList.push(numberList[i])
        }
        numberList = tmpNumberList

        var resultList = []
        var needAddPre = 0
        for(var i = numberList.length - 1; i >= 0; i--){
            var number = parseInt(numberList[i])
            number += needAddPre
            var divider = i == numberList.length - 1 ? fps : 60
            needAddPre = Math.floor(number / divider)
            var keep = number % divider
            console.log("needAddPre:" + needAddPre + ", keep:" + keep)
            resultList.push(keep < 10 ? "0"+keep.toString() : keep.toString())
        }

        for(var i = resultList.length - 1; i >= 0; i--){
            result += resultList[i];
            if(i != 0){
                result += ":"
            }
        }
        console.log("result data: " + result)
        return result;
    }

    function isEditing() {
        return editContainer.activeFocus || editText.activeFocus
    }

    function setDuration(durationData) {
        fpsDuration = durationData
        var displayText = verifyInput(fpsDuration.toString())
        showText.text = displayText
    }

    function isAllZero(displayData) {
        return displayData == "00:00:00:00"
    }

    function textToFps(content) {
        var numberList = []
        var contentList = content.split(":")
        var result = 0
        for(var i = 0; i < contentList.length; i++){
             var number = parseInt(contentList[i])
             var powNum = Math.pow(fps, contentList.length - 1 - i)
             var temResult = number * powNum
             result += temResult
        }
        console.log("textToFps, content:" + content + ", fps:" + result)
        return result
    }

    function setEnable(enable){
        editContainer.enabled = enable
        editText.enabled = enable
        showText.color = enable ? "white" : "grey"
    }
}