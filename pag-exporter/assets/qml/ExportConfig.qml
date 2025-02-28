import QtQuick 2.12
import QtQuick.Controls
import Qt.labs.qmlmodels 1.0
import QtQuick.Window 2.13
PApplicationWindow {
    property var windowWidth: 819
    property var windowHeight: 778
    property var windowMarginLeft: 12
    property var borderWidth: 1
    property var normalIconSize: 20
    property var bmpMarginRight: 11
    property var windowMarginRight: windowMarginLeft
    property var isPreview: false
    property var isFirstEnter: true
    property var lastPressTime: 0
    property var allChecked: compositionModel.isAllSelected()
    property var hasData: compositionModel.count == 0

    property var bmpIconPath: isPreview ? "../images/export_bmp.png" : "qrc:/images/export_bmp.png"
    property var settingIconPath: isPreview ? "../images/export_settings.png" : "qrc:/images/export_settings.png"
    property var previewIconPath: isPreview ? "../images/export_preview.png" : "qrc:/images/export_preview.png"
    property var checkboxOffIconPath: isPreview ? "../images/checkbox_off.png" : "qrc:/images/checkbox_off.png"
    property var checkboxOnIconPath: isPreview ? "../images/checkbox_on.png" : "qrc:/images/checkbox_on.png"
    property var eportFoldIconPath: isPreview ? "../images/export_fold.png" : "qrc:/images/export_fold.png"
    property var eportOpenIconPath: isPreview ? "../images/export_open.png" : "qrc:/images/export_open.png"
    property var folderIconPath: isPreview ? "../images/export_folder.png" : "qrc:/images/export_folder.png"
    property var radioOnIconPath: isPreview ? "../images/export_radio_on.png" : "qrc:/images/export_radio_on.png"
    property var radioOffIconPath: isPreview ? "../images/export_radio_off.png" : "qrc:/images/export_radio_off.png"
    property var searchIconPath: isPreview ? "../images/export_search.png" : "qrc:/images/export_search.png"

    id: wind
    visible: true
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    windowBackground: Qt.rgba(20 / 255, 20 / 255, 30 / 255, 1)
    titlebarBackground: windowBackground
    canResize: false
    isWindows: true
    titleFontSize: 14
    maxBtnVisible: false
    minBtnVisible: false
    canMaxSize: false

        onClosing: function(closeEvent) {
            console.log("ExportConfig window close")
            exportConfigDialog.exit()
        }

        onActiveChanged:{
            console.log("onActiveChanged:" + active)
    //        if(active && isFirstEnter){
    //            exportConfigDialog.resetData()
    //            exportConfigDialog.refreshErrorListView()
    //            isFirstEnter = false
    //        }
        }

        Rectangle{
            id: layerContainer
            width: parent.width
            height: 440
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top : parent.top
            anchors.leftMargin: windowMarginLeft
            anchors.rightMargin: windowMarginRight
            anchors.topMargin: 11
            border.color: "#000000"
            border.width: borderWidth
            radius: 2
            color: Qt.rgba(1, 1, 1, 15/255)
            clip: true

            Item{
                //暂存鼠标拖动的位置
                property int posXTemp: 0

                id: headerItem
                anchors.left: parent.left
                anchors.right: parent.right
                height: hasData ? layerTableView.titleHeight : layerTableView.titleHeight + layerTableView.firstRowMarginTop

                MouseArea{
                    anchors.fill: parent
                    onPressed: headerItem.posXTemp=mouseX;
                    onPositionChanged: {
                        if(layerTableView.contentX + (headerItem.posXTemp-mouseX) > 0){
                            layerTableView.contentX += (headerItem.posXTemp-mouseX);
                        }else{
                            layerTableView.contentX = 0;
                        }
                        headerItem.posXTemp = mouseX;
                    }
                }

                Row {
                    id: headerItem_row
                    anchors.fill: parent
                    leftPadding: -layerTableView.contentX
                    clip: true

                    Repeater {
                        model: layerTableView.columns > 0 ? layerTableView.columns : 0

                        Rectangle{
                            id: headerContainer
                            height: hasData ? layerTableView.titleHeight : layerTableView.titleHeight + layerTableView.firstRowMarginTop
                            width: layerTableView.getColumnWidth(index)
                            border.width: 0
                            color: "#22222B"
                            border.color: "red"

                            Rectangle{
                                id: horizontalTitle
                                width: parent.width
                                height: layerTableView.titleHeight
                                color: "transparent"

                                Text{
                                    visible: index >= 2
                                    anchors.fill: parent
                                    anchors.centerIn: parent
                                    font.pixelSize: 16
                                    font.family: "PingFang SC"
                                    horizontalAlignment: index <= 2 ? Text.AlignLeft : Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    color: Qt.rgba(1,1,1,104/255)
                                    text: headerItem.getTitle(index)
                                }

                                Image{
                                    id:titleCheckbox
                                    anchors.centerIn: parent
                                    width: normalIconSize
                                    height: normalIconSize
                                    source: allChecked ? checkboxOnIconPath : checkboxOffIconPath
                                    visible: index == 0

                                    MouseArea{
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onPressed: {
                                            allChecked = !allChecked
                                            exportConfigDialog.setAllChecked(allChecked)
                                        }
                                    }
                                }

                                Rectangle{
                                    id: searchContainer
                                    anchors.fill: parent
                                    anchors.bottomMargin: 4
                                    anchors.topMargin: 4
                                    anchors.rightMargin: 20
                                    color: Qt.rgba(142/255,150/255,179/255,0.15)
                                    visible: index == 1
                                    radius: 2

                                    MouseArea{
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onPressed: {
                                            searchText.focus = true
                                            searchText.forceActiveFocus()
                                            searchContainer.handleFocus(true)
                                        }
                                    }

                                    Image{
                                        id: searchIcon
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: 9
                                        anchors.left: parent.left
                                        width: normalIconSize
                                        height: normalIconSize
                                        source: searchIconPath
                                    }

                                    Text{
                                        id:hintText
                                        font.pixelSize: 16
                                        font.family: "PingFang SC"
                                        color: "#8E96B3"
                                        text: qsTr("搜索合成")
                                        anchors.left: searchIcon.right
                                        anchors.right: parent.right
                                        anchors.leftMargin: 5
                                        anchors.rightMargin: 5
                                        anchors.verticalCenter: parent.verticalCenter
                                        visible: true
                                        renderType: Text.NativeRendering
                                    }

                                    TextInput{
                                        id: searchText
                                        font.pixelSize: 16
                                        font.family: "PingFang SC"
                                        anchors.left: searchIcon.right
                                        anchors.right: parent.right
                                        anchors.leftMargin: 5
                                        anchors.rightMargin: 5
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: ""
                                        color: "white"
                                        renderType: Text.NativeRendering
                                        selectByMouse: true
                                        clip: true
                                        visible: false

                                        onEditingFinished:{
                                            console.log("onEditingFinished, current text:" + searchText.text)
                                            if(searchText.text.length == 0){
                                                console.log("resetAfterNoSearch execute")
                                                exportConfigDialog.resetAfterNoSearch()
                                            }else{
                                                console.log("searchCompositionsByName execute")
                                                exportConfigDialog.searchCompositionsByName(searchText.text)
                                            }
                                        }

                                        onFocusChanged: {
                                            console.log("searchText onFocusChanged:" + focus)
                                            if(focus || searchText.text.length == 0){
                                                searchContainer.handleFocus(focus)
                                            }

                                        }

                                        Keys.onReturnPressed:{
                                            focus = false
                                        }

                                        Keys.onEnterPressed:{
                                            focus = false
                                        }
                                    }

                                    function handleFocus(hasFocus){
                                        hintText.visible = !hasFocus
                                        searchText.visible = hasFocus
                                        console.log("handleFocusIn execute, hasFocus:" + hasFocus)
                                    }

                                }
                            }

                            Rectangle{
                                id: headerDivider
                                anchors.top: horizontalTitle.bottom
                                width: parent.width
                                height: 1
                                color: "black"
                            }

                            Rectangle{
                                color: "transparent"
                                anchors.top: headerDivider.bottom
                                visible: hasData
                                width: parent.width
                                height: layerTableView.firstRowMarginTop
                            }
                        }
                    }
                }

                function getTitle(column){
                    switch(column){
                        case 2:
                            return qsTr("存储路径")
                        case 3:
                            return qsTr("设置")
                        case 4:
                            return qsTr("预览")
                        default:
                            return ""
                    }
                }
            }

            TableView{
                property var checkColumnWidth: 79
                property var savePathColumnWidth: 302
                property var settingColumnWidth: 72
                property var previewColumnWidth: 72
                property var realTableViewWidth: Qt.platform.os =='windows' ? windowWidth - windowMarginLeft - windowMarginRight : layerTableView.width
                property var layerNameColumnWidth: realTableViewWidth - checkColumnWidth - savePathColumnWidth - settingColumnWidth - previewColumnWidth - borderWidth*2
                property var layerNameColumnRealWidth: layerNameColumnWidth
                property var titleHeight: 35
                property var firstRowMarginTop: 5

                id: layerTableView
                width: parent.width
                model: compositionModel
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                anchors.top: headerItem.bottom
                anchors.bottom: parent.bottom


                Component.onCompleted:{
                    console.log("TableView width:" + layerTableView.width)
                    refreshIfLayerColumnWidthChange()
                }

                Connections {
                    target: compositionModel
                    onFoldStatusChange: {
                        console.log("onFoldStatusChange")
                        layerTableView.refreshIfLayerColumnWidthChange()
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    id: verticalScroller
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    contentItem: Rectangle{
                        radius: 2
                        visible: (verticalScroller.size<1.0)
                        implicitWidth: 3
                        color: "#535359"
                    }
                }

                ScrollBar.horizontal: ScrollBar {
                    id: horizontalScroller
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    contentItem: Rectangle{
                        radius: 2
                        visible: (horizontalScroller.size<1.0)
                        implicitHeight: 3
                        color: "#535359"
                    }
                }

                delegate: DelegateChooser{
                    DelegateChoice {
                        column: 0
                        delegate: Rectangle{
                            implicitHeight: layerTableView.titleHeight
                            implicitWidth : layerTableView.getColumnWidth(column)
                            color: model.row % 2 == 0 ? "#22222B" : "#27272F"
                            Image{
                                anchors.centerIn: parent
                                width: normalIconSize
                                height: normalIconSize
                                source: itemChecked ? checkboxOnIconPath : checkboxOffIconPath
                                visible: !isFolder

                                MouseArea{
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked:{
                                        itemChecked = !itemChecked
                                        allChecked = compositionModel.isAllSelected();
                                    }
                                }
                            }

                            MouseArea{
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                cursorShape: Qt.PointingHandCursor
                                visible: isFolder
                                onClicked:{
                                    compositionModel.setData(model.row, !isUnFold, "isUnFold")
                                }
                            }
                        }
                    }

                    DelegateChoice {
                        column: 1
                        delegate: Rectangle{
                            implicitHeight: layerTableView.titleHeight
                            implicitWidth : layerTableView.getColumnWidth(column)
                            color: model.row % 2 == 0 ? "#22222B" : "#27272F"

                            Image{
                                id: unfoldIcon
                                width: 8
                                height: 8
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: -width + calculateLeftMargin(layerLevel)
                                visible: isFolder ? true : false
                                source: isUnFold ? eportOpenIconPath : eportFoldIconPath
                            }

                            Image{
                                id: bmpIcon
                                width: normalIconSize
                                height: normalIconSize
                                anchors.left: unfoldIcon.right
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.rightMargin: bmpMarginRight
                                source: isFolder ? folderIconPath : bmpIconPath
                            }

                            Text{
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: bmpIcon.right
                                anchors.leftMargin: bmpMarginRight
                                anchors.right: parent.right
                                font.pixelSize: 14
                                font.family: "PingFang SC"
                                text: layerName
                                color: "#FFFFFF"
                                elide: Text.ElideMiddle
                            }

                            MouseArea{
                                visible: isFolder
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                cursorShape: Qt.PointingHandCursor
                                onClicked:{
                                    compositionModel.setData(model.row, !isUnFold, "isUnFold")
                                }
                            }

                            function calculateLeftMargin(level){
                                console.log("calculateLeftMargin level:" + level)
                                return level * (bmpMarginRight + normalIconSize)
                            }
                        }
                    }

                    DelegateChoice {
                        column: 2
                        delegate: Rectangle{
                            implicitHeight: layerTableView.titleHeight
                            implicitWidth : layerTableView.getColumnWidth(column)
                            color: model.row % 2 == 0 ? "#22222B" : "#27272F"

                            Text{
                                width: parent.width
                                anchors.verticalCenter: parent.verticalCenter
                                font.pixelSize: 13
                                font.family: "PingFang SC"
                                text: savePath
                                color: "#8E96B3"
                                elide: Text.ElideRight

                                MouseArea{
                                    visible: !isFolder
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked:{
                                        compositionModel.handleSavePathClick(model.row)
                                    }
                                }
                            }

                            MouseArea{
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                cursorShape: Qt.PointingHandCursor
                                visible: isFolder
                                onClicked:{
                                    compositionModel.setData(model.row, !isUnFold, "isUnFold")
                                }
                            }
                        }
                    }

                    DelegateChoice {
                        column: 3
                        delegate: Rectangle{
                            implicitHeight: layerTableView.titleHeight
                            implicitWidth : layerTableView.getColumnWidth(column)
                            color: model.row % 2 == 0 ? "#22222B" : "#27272F"

                            Image{
                                anchors.centerIn: parent
                                width: normalIconSize
                                height: normalIconSize
                                source: settingIconPath
                                visible: !isFolder

                                MouseArea{
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked:{
                                        compositionModel.handleSettingIconClick(model.row)
                                    }
                                }
                            }

                            MouseArea{
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                cursorShape: Qt.PointingHandCursor
                                visible: isFolder
                                onClicked:{
                                    compositionModel.setData(model.row, !isUnFold, "isUnFold")
                                }
                            }
                        }
                    }

                    DelegateChoice {
                        column: 4
                        delegate: Rectangle{
                            implicitHeight: layerTableView.titleHeight
                            implicitWidth : layerTableView.getColumnWidth(column)
                            color: model.row % 2 == 0 ? "#22222B" : "#27272F"

                            Image{
                                anchors.centerIn: parent
                                width: normalIconSize
                                height: normalIconSize
                                source: previewIconPath
                                visible: !isFolder

                                MouseArea{
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked:{
                                        compositionModel.handlePreviewIconClick(model.row)
                                    }
                                }
                            }

                            MouseArea{
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                cursorShape: Qt.PointingHandCursor
                                visible: isFolder
                                onClicked:{
                                    compositionModel.setData(model.row, !isUnFold, "isUnFold")
                                }
                            }
                        }
                    }
                }

                function refreshIfLayerColumnWidthChange() {
                    var realNeedWidth = getLayerColumnNeedWidth(layerNameColumnWidth)
                    console.log("realNeedWidth:" + realNeedWidth + ", layerNameColumnWidth:" + layerNameColumnWidth)
                    if(realNeedWidth != layerNameColumnRealWidth){
                        console.log("realNeedWidth:" + realNeedWidth + ", layerNameColumnRealWidth:" + layerNameColumnRealWidth)
                        layerNameColumnRealWidth = realNeedWidth;

                        forceLayout()
                    }
                }

                function getColumnWidth(column){
                    var columnWidth = 0
                    switch(column){
                        case 0:
                            columnWidth = checkColumnWidth
                            break
                        case 1:
                            columnWidth = layerNameColumnRealWidth
                            break
                        case 2:
                            columnWidth = savePathColumnWidth
                            break
                        case 3:
                            columnWidth = settingColumnWidth
                            break
                        case 4:
                            columnWidth = previewColumnWidth
                            break
                    }
                    console.log("column width:" + columnWidth)
                    return columnWidth
                }
            }
        }

         Rectangle{
            id: errorContainer
            width: parent.width
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top : layerContainer.bottom
            anchors.bottom: audioCheckbox.top
            anchors.leftMargin: windowMarginLeft
            anchors.rightMargin: windowMarginRight
            anchors.topMargin: 8
            anchors.bottomMargin: 24
            border.color: "#000000"
            border.width: 1
            radius: 2
            color: Qt.rgba(1, 1, 1, 15/255)
            clip: true

            Text{
                id: errorTipLabel
                font.pixelSize: 15
                font.family: "PingFang SC"
                anchors.centerIn: parent
                text: qsTr("暂未发现错误信息")
                color: "#FFFFFF"
                visible:  errorListModel.rowCount() == 0
            }

            ErrorListView{
                id: errorListContainer
                anchors.fill: parent
                visible: errorListModel.rowCount() != 0
            }

            Connections{
                target: errorListModel
                onAlertInfosChanged: {
                    var empty = errorListModel.rowCount() == 0;
                    console.log("onCompositionCheckChanged, empty:" + empty)
                    errorTipLabel.visible = empty
                    errorListContainer.visible = !empty
                }

            }
        }

        Rectangle{
            property var isEnable: compositionModel.isLayerSelected()

            id: exportBtn
            width: 130
            height: 48
            color: isEnable ? "#1982EB" : "#365B8D"
            radius: 2
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            anchors.rightMargin: windowMarginRight

            Text{
                font.pixelSize: 18
                font.family: "PingFang SC"
                anchors.centerIn: parent
                text: qsTr("导出")
                font.bold: true
                color: "#FFFFFF"
            }

            MouseArea{
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    if(!exportBtn.isEnable){
                        return
                    }
                    exportConfigDialog.exportFiles()
                    wind.close()
                }
            }

            Connections {
                target: compositionModel
                onCompositionCheckChanged: {
                    exportBtn.isEnable = compositionModel.isLayerSelected()
                }
            }
        }

        Rectangle{
            id: cancelBtn
            width: 130
            height: 48
            color: "#282832"
            radius: 2
            anchors.right: exportBtn.left
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            anchors.rightMargin: 20

            Text{
                font.pixelSize: 18
                font.family: "PingFang SC"
                anchors.centerIn: parent
                text: qsTr("取消")
                font.bold: true
                color: "#FFFFFF"
            }

            MouseArea{
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    wind.hide()
                    wind.close()
                }
            }
        }

        Image{
            property var isChecked : exportConfigDialog.isAudioExport()

            id:audioCheckbox
            width: normalIconSize
            height: normalIconSize
            anchors.left: parent.left
            anchors.leftMargin: windowMarginLeft
            anchors.bottom : cancelBtn.top
            anchors.bottomMargin: 20
            source: isChecked ? checkboxOnIconPath : checkboxOffIconPath

            MouseArea{
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    audioCheckbox.isChecked = !audioCheckbox.isChecked
                    exportConfigDialog.onExportAudioChange(audioCheckbox.isChecked)
                    audioCheckbox.source = ""
                    audioCheckbox.source = audioCheckbox.isChecked ? checkboxOnIconPath : checkboxOffIconPath
                }
            }
        }

        Text{
            id: audioTipText
            font.pixelSize: 16
            font.family: "PingFang SC"
            anchors.left: audioCheckbox.right
            anchors.bottom: audioCheckbox.bottom
            anchors.leftMargin:8
            text: qsTr("同时导出音频")
            color: "#FFFFFF"
        }

        Text{
            id: helperText
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: audioTipText.left
            anchors.top: audioTipText.top
            font.pixelSize: 14
            font.family: "PingFang SC"
            color: "#FFFFFF"
            visible: false
        }

         // 计算得到合成名列最大宽度
        function getLayerColumnNeedWidth(defaultWidth) {
            var currentWidth = 0;
            var textMaxWidth = layerTableView.layerNameColumnWidth - normalIconSize - bmpMarginRight;
            for( var i = 0; i < compositionModel.rowCount(); i++ ) {
                var itemData = compositionModel.get(i)
                var itemLeftMargin = itemData.layerLevel * (bmpMarginRight + normalIconSize)

                helperText.text = itemData.layerName;
                var textWidth = Math.min(textMaxWidth, helperText.width)
                var itemNeedWidth = itemLeftMargin + textWidth;

                console.log("itemNeedWidth width:" + itemNeedWidth)
                currentWidth = Math.max(currentWidth, itemNeedWidth);
            }
            return Math.max(currentWidth, defaultWidth);
        }
}
